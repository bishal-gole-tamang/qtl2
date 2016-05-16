// phase-known Diversity Outcross QTLCross class (for HMM, in particular est.map)

#include "cross_dopk.h"
#include <math.h>
#include <Rcpp.h>
#include "cross.h"
#include "cross_util.h"
#include "cross_do.h"
#include "cross_do_util.h"
#include "r_message.h"

enum gen {A=1, H=2, B=3, notA=5, notB=4};

const bool DOPK::check_geno(const int gen, const bool is_observed_value,
                            const bool is_x_chr, const bool is_female, const IntegerVector& cross_info)
{
    if(is_observed_value) {
        if(gen==0 || gen==A || gen==H || gen==B ||
           gen==notA || gen==notB) return true;
        return false;
    }

    const int n_alleles =   8;
    const int n_geno =  64;

    if(!is_x_chr || is_female) { // autosome or female X
        if(gen >= 1 && gen <= n_geno) return true;
    }
    else {
        if(gen >= n_geno+1 && gen <= n_geno+n_alleles) return true;
    }
    return false; // otherwise a problem
}


const double DOPK::init(const int true_gen,
                        const bool is_x_chr, const bool is_female,
                        const IntegerVector& cross_info)
{
    #ifndef NDEBUG
    if(!check_geno(true_gen, false, is_x_chr, is_female, cross_info))
        throw std::range_error("genotype value not allowed");
    #endif

    if(!is_x_chr || is_female) // autosome or female X
        return(-log(64.0));
    else
        return(-log(8.0));
}

const double DOPK::emit(const int obs_gen, const int true_gen, const double error_prob,
                        const IntegerVector& founder_geno, const bool is_x_chr,
                        const bool is_female, const IntegerVector& cross_info)
{
    #ifndef NDEBUG
    if(!check_geno(true_gen, false, is_x_chr, is_female, cross_info))
        throw std::range_error("genotype value not allowed");
    #endif
    const int n_geno = 64;

    if(obs_gen==0) return 0.0; // missing

    if(!is_x_chr || is_female) { // autosome or female X
        const IntegerVector true_alleles = mpp_decode_geno(true_gen, 8, true);
        int f1 = founder_geno[true_alleles[0]-1];
        int f2 = founder_geno[true_alleles[1]-1];

        // treat founder hets as missing
        if(f1==2) f1 = 0;
        if(f2==2) f2 = 0;

        // neither founder alleles observed
        if(f1 == 0 && f2 == 0) return 0.0;

        // one founder allele observed
        if(f1 == 0 || f2 == 0) {

            switch(std::max(f1, f2)) {
            case H: return 0.0; // het compatible with either founder allele
            case A:
                switch(obs_gen) {
                case A: case notB: return log(1.0-error_prob);
                case B: case notA: return log(error_prob);
                case H: return 0.0;
                }
            case B:
                switch(obs_gen) {
                case B: case notA: return log(1.0-error_prob);
                case A: case notB: return log(error_prob);
                case H: return 0.0;
                }
            }
            return 0.0;
        }

        switch((f1+f2)/2) { // values 1, 2, 3
        case A:
            switch(obs_gen) {
            case A: return log(1.0-error_prob);
            case H: return log(error_prob/2.0);
            case B: return log(error_prob/2.0);
            case notA: return log(error_prob);
            case notB: return log(1.0-error_prob/2.0);
            }
        case H:
            switch(obs_gen) {
            case A: return log(error_prob/2.0);
            case H: return log(1.0-error_prob);
            case B: return log(error_prob/2.0);
            case notA: return log(1.0-error_prob/2.0);
            case notB: return log(1.0-error_prob/2.0);
            }
        case B:
            switch(obs_gen) {
            case B: return log(1.0-error_prob);
            case H: return log(error_prob/2.0);
            case A: return log(error_prob/2.0);
            case notB: return log(error_prob);
            case notA: return log(1.0-error_prob/2.0);
            }
        }
        return 0.0;
    }
    else { // male X
        const int founder_allele = founder_geno[(true_gen - n_geno) - 1];

        switch(founder_allele) {
        case A:
            switch(obs_gen) {
            case A: case notB: return log(1.0-error_prob);
            case B: case notA: return log(error_prob);
            }
        case B:
            switch(obs_gen) {
            case B: case notA: return log(1.0-error_prob);
            case A: case notB: return log(error_prob);
            }
        }
        return 0.0;
    }

    return NA_REAL; // shouldn't get here
}


const double DOPK::step(const int gen_left, const int gen_right, const double rec_frac,
                        const bool is_x_chr, const bool is_female,
                        const IntegerVector& cross_info)
{
    #ifndef NDEBUG
    if(!check_geno(gen_left, false, is_x_chr, is_female, cross_info) ||
       !check_geno(gen_right, false, is_x_chr, is_female, cross_info))
        throw std::range_error("genotype value not allowed");
    #endif

    // info about preCC progenitors
    const static IntegerVector precc_gen = IntegerVector::create(4,5,6,7,8,9,10,11,12);
    const static NumericVector precc_alpha =
        NumericVector::create(21.0/144.0, 64.0/144.0, 24.0/144.0, 10.0/144.0, 5.0/144.0,
                               9.0/144.0,  5.0/144.0,  3.0/144.0,  3.0/144.0);

    // no. generations for this mouse
    int n_gen = cross_info[0];

    if(is_x_chr) {
        if(is_female) { // female X
            return step_femX(gen_left, gen_right, rec_frac, n_gen,
                             precc_gen, precc_alpha);
        }
        else { // male X
            return step_malX(gen_left, gen_right, rec_frac, n_gen,
                               precc_gen, precc_alpha);
        }
    }
    else { // autosome
        return step_auto(gen_left, gen_right, rec_frac, n_gen,
                         precc_gen, precc_alpha);
    }

    return NA_REAL; // shouldn't get here
}

const IntegerVector DOPK::possible_gen(const bool is_x_chr, const bool is_female,
                                       const IntegerVector& cross_info)
{
    int n_alleles = 8;
    int n_geno = 64;

    if(is_x_chr && !is_female) { // male X chromosome
        IntegerVector result(n_alleles);
        for(int i=0; i<n_alleles; i++)
            result[i] = n_geno+i+1;
        return result;
    }
    else { // autosome or female X
        IntegerVector result(n_geno);
        for(int i=0; i<n_geno; i++)
            result[i] = i+1;
        return result;
    }
}

const int DOPK::ngen(const bool is_x_chr)
{
    int n_alleles = 8;
    int n_geno = 64;

    if(is_x_chr) return n_geno+n_alleles;
    return n_geno;
}

const int DOPK::nalleles()
{
    return 8;
}

const double DOPK::nrec(const int gen_left, const int gen_right,
                        const bool is_x_chr, const bool is_female,
                        const IntegerVector& cross_info)
{
    // need to fill in this function
    return NA_REAL;
}

const double DOPK::est_rec_frac(const NumericVector& gamma, const bool is_x_chr,
                                const IntegerMatrix& cross_info, const int n_gen)
{
    Rcpp::stop("est_map not yet available for Diversity Outcross");

    return NA_REAL;
}

const NumericMatrix DOPK::geno2allele_matrix(const bool is_x_chr)
{
    const int n_alleles = 8;
    const int n_geno = 64;

    if(is_x_chr) {
        NumericMatrix result(n_geno+n_alleles, n_alleles);
        // female X
        for(int trueg=0; trueg<n_geno; trueg++) {
            IntegerVector alleles = mpp_decode_geno(trueg+1, 8, true);
            result(trueg,alleles[0]-1) += 0.5;
            result(trueg,alleles[1]-1) += 0.5;
        }
        // male X
        for(int trueg=0; trueg<n_geno; trueg++)
            result(trueg+n_geno, trueg) = 1.0;

        return result;
    }
    else { // autosome
        NumericMatrix result(n_geno,n_alleles);

        for(int trueg=0; trueg<n_geno; trueg++) {
            IntegerVector alleles = mpp_decode_geno(trueg+1, 8, true);
            result(trueg,alleles[0]-1) += 0.5;
            result(trueg,alleles[1]-1) += 0.5;
        }

        return result;
    }
}

// check that sex conforms to expectation
const bool DOPK::check_is_female_vector(const LogicalVector& is_female, const bool any_x_chr)
{
    bool result = true;
    const unsigned int n = is_female.size();
    if(!any_x_chr) { // all autosomes
        if(n > 0) {
            // not needed here, but don't call this an error
            result = true;
        }
    }
    else { // X chr included
        if(n == 0) {
            result = false;
            r_message("is_female not provided, but needed to handle X chromosome");
        }
        else {
            unsigned int n_missing = 0;
            for(unsigned int i=0; i<n; i++)
                if(is_female[i] == NA_LOGICAL) ++n_missing;
            if(n_missing > 0) {
                result = false;
                r_message("is_female contains missing values (it shouldn't)");
            }
        }
    }
    return result;
}

// check that cross_info conforms to expectation
const bool DOPK::check_crossinfo(const IntegerMatrix& cross_info, const bool any_x_chr)
{
    bool result = true;
    const unsigned int n_row = cross_info.rows();
    const unsigned int n_col = cross_info.cols();
    // one column with number of generations (needed no matter what; values should be >= 1)

    if(n_col == 0) {
        result = false;
        r_message("cross_info not provided, but should at least one column, with no. generations");
        return result;
    }

    unsigned int n_missing=0;
    unsigned int n_invalid=0;
    for(unsigned int i=0; i<n_row; i++) {
        if(cross_info[i] == NA_INTEGER) ++n_missing;
        else if(cross_info[i] < 1) ++n_invalid;
    }
    if(n_missing > 0) {
        result = false;
        r_message("cross_info has missing values (it shouldn't)");
    }
    if(n_invalid > 0) {
        result = false;
        r_message("cross_info has invalid values; no. generations should be >= 1");
    }

    return result;
}



/**********************************************************************
 * transition probability for DO, autosome, phase-known case
 *
 * left = genotype at left locus
 * right = genotype at right locus
 * r = recombination fraction
 * s = generation of DO
 *
 * precc_alpha = proportion of preCC progenitors at generation precc_gen
 *
 * This calculates log Pr(right | left) for phase-known case
 *
 **********************************************************************/
const double DOPK::step_auto(int left, int right, double r, int s,
                             IntegerVector precc_gen, NumericVector precc_alpha)
{
    double recprob;
    #ifndef NDEBUG
    int n_precc = precc_gen.size();
    if(n_precc != precc_alpha.size())
        throw std::invalid_argument("precc_gen and precc_alpha should be the same length");
    #endif

    // pull out alleles for left and right loci
    IntegerVector leftv = mpp_decode_geno(left, 8, true);
    IntegerVector rightv = mpp_decode_geno(right, 8, true);
    int left1 = leftv[0];
    int left2 = leftv[1];
    int right1 = rightv[0];
    int right2 = rightv[1];

    // probability of recombinant haplotype
    recprob = DOrec_auto(r, s, precc_gen, precc_alpha);

    if(left1 == left2) { // AA ->
        if(right1 == right2) {
            if(left1 == right1) { // AA -> AA
                return( 2.0*log(1.0 - recprob) );
            }
            else { // AA -> BB
                return( 2.0*log(recprob) - log(49.0) );
            }
        }
        else {
            if(left1 == right1 || left1 == right2) { // AA -> AB
                return( log(recprob) + log(1.0-recprob) - log(7.0) );
            }
            else { // AA -> BC
                return( 2.0*log(recprob) - log(49.0) );
            }
        }
    }
    else { // AB ->
        if(right1 == right2) {
            if(left1 == right1 || left2 == right1) { // AB -> AA
                return( log(recprob) + log(1.0 - recprob) - log(7.0) );
            }
            else { // AB -> CC
                return( 2.0*log(recprob) - log(49.0) );
            }
        }
        else {
            if(left1==right1 && left2==right2) { // AB -> AB
                return( 2.0*log(1-recprob) );
            }
            else if(left1==right2 && left2==right1) { // AB -> BA
                return( 2.0*log(recprob) - log(49.0) );
            }
            else if(left1==right1 || left2==right2) { // AB -> AC
                return( log(recprob) + log(1.0-recprob) - log(7.0) );
            }
            else if(left2==right1 || left2==right2) { // AB -> CA
                return( 2.0*log(recprob) - log(49.0) );
            }
            else { // AB -> CD
                return( 2.0*log(recprob) - log(49.0) );
            }
        }
    }
}

/**********************************************************************
 * transition probability for DO, female X chr in phase-known case
 **********************************************************************/
const double DOPK::step_femX(int left, int right, double r, int s,
                             IntegerVector precc_gen, NumericVector precc_alpha)
{
    double recprob;
    #ifndef NDEBUG
    int n_precc = precc_gen.size();
    if(n_precc != precc_alpha.size())
        throw std::invalid_argument("precc_gen and precc_alpha should be the same length");
    #endif

    // pull out alleles for left and right loci
    IntegerVector leftv = mpp_decode_geno(left, 8, true);
    IntegerVector rightv = mpp_decode_geno(right, 8, true);
    int left1 = leftv[0];
    int left2 = leftv[1];
    int right1 = rightv[0];
    int right2 = rightv[1];

    // probability of recombinant haplotype
    recprob = DOrec_femX(r, s, precc_gen, precc_alpha);

    if(left1 == left2) { // AA ->
        if(right1 == right2) {
            if(left1 == right1) { // AA -> AA
                return( 2.0*log(1.0 - recprob) );
            }
            else { // AA -> BB
                return( 2.0*log(recprob) - log(49.0) );
            }
        }
        else {
            if(left1 == right1 || left1 == right2) { // AA -> AB
                return( log(recprob) + log(1.0-recprob) - log(7.0) );
            }
            else { // AA -> BC
                return( 2.0*log(recprob) - log(49.0) );
            }
        }
    }
    else { // AB ->
        if(right1 == right2) {
            if(left1 == right1 || left2 == right1) { // AB -> AA
                return( log(recprob) + log(1.0 - recprob) - log(7.0) );
            }
            else { // AB -> CC
                return( 2.0*log(recprob) - log(49.0) );
            }
        }
        else {
            if(left1==right1 && left2==right2) { // AB -> AB
                return( 2.0*log(1-recprob) );
            }
            else if(left1==right2 && left2==right1) { // AB -> BA
                return( 2.0*log(recprob) - log(49.0) );
            }
            else if(left1==right1 || left2==right2) { // AB -> AC
                return( log(recprob) + log(1.0-recprob) - log(7.0) );
            }
            else if(left2==right1 || left2==right2) { // AB -> CA
                return( 2.0*log(recprob) - log(49.0) );
            }
            else { // AB -> CD
                return( 2.0*log(recprob) - log(49.0) );
            }
        }
    }
}

// transition probability for DO, male X chr
const double DOPK::step_malX(int left, int right, double r, int s,
                             IntegerVector precc_gen, NumericVector precc_alpha)
{
    double recprob;
    #ifndef NDEBUG
    int n_precc = precc_gen.size();
    if(n_precc != precc_alpha.size())
        throw std::invalid_argument("precc_gen and precc_alpha should be the same length");
    #endif

    // probability of recombinant haplotype
    recprob = DOrec_malX(r, s, precc_gen, precc_alpha);

    if(left == right) return log(1.0 - recprob);
    return log(recprob) - log(7.0);
}


// check that founder genotype data has correct no. founders and markers
const bool DOPK::check_founder_geno_size(const IntegerMatrix& founder_geno, const int n_markers)
{
    bool result=true;

    const int fg_mar = founder_geno.cols();
    const int fg_f   = founder_geno.rows();

    if(fg_mar != n_markers) {
        result = false;
        r_message("founder_geno has incorrect number of markers");
    }

    if(fg_f != 8) {
        result = false;
        r_message("founder_geno should have 8 founders");
    }

    return result;
}

// check that founder genotype data has correct values
const bool DOPK::check_founder_geno_values(const IntegerMatrix& founder_geno)
{
    const int fg_mar = founder_geno.cols();
    const int fg_f   = founder_geno.rows();

    for(int f=0; f<fg_f; f++) {
        for(int mar=0; mar<fg_mar; mar++) {
            int fg = founder_geno(f,mar);
            if(fg != 0 && fg != 1 && fg != 3) {
                // at least one invalid value
                r_message("founder_geno contains invalid values; should be in {0, 1, 3}");
                return false;
            }
        }
    }

    return true;
}


const bool DOPK::need_founder_geno()
{
    return true;
}
