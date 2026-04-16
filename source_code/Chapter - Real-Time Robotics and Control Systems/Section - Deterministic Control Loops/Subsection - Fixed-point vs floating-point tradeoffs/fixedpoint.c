/* Fixed-point Q32.32: 64-bit signed representation, 32 fractional bits. */
/* Portable GCC/Clang using __int128 for full precision. */
#include 
#include 

typedef int64_t q32_32_t;
enum { Q32_FRAC = 32 };

static inline q32_32_t q32_32_from_double(double d) {
    /* clamp to representable range, convert with rounding */
    const double maxv = (double)(INT64_MAX >> Q32_FRAC);
    const double minv = (double)(INT64_MIN >> Q32_FRAC);
    if (d >= maxv) return INT64_MAX;
    if (d <= minv) return INT64_MIN;
    double scaled = d * (1ULL << Q32_FRAC);
    return (q32_32_t)(scaled + (scaled >= 0 ? 0.5 : -0.5));
}

static inline double q32_32_to_double(q32_32_t q) {
    return (double)q / (double)(1ULL << Q32_FRAC);
}

/* Saturating multiply with rounding to nearest */
static inline q32_32_t q32_32_mul(q32_32_t a, q32_32_t b) {
    __int128 prod = (__int128)a * (__int128)b;         /* 128-bit exact */
    __int128 round = (__int128)1 << (Q32_FRAC - 1);    /* for rounding */
    prod += (prod >= 0) ? round : -round;
    prod >>= Q32_FRAC;
    if (prod > INT64_MAX) return INT64_MAX;
    if (prod < INT64_MIN) return INT64_MIN;
    return (q32_32_t)prod;
}

/* Fast division by constant using precomputed reciprocal (example) */
static inline q32_32_t q32_32_div_const(q32_32_t a, int32_t c, int64_t recip) {
    /* recip is round(2^(Q32_FRAC+shift)/c); caller chooses shift for precision */
    __int128 prod = (__int128)a * (__int128)recip;
    prod >>= (Q32_FRAC); /* adjust by reciprocal scaling */
    /* optional saturate/round logic here */
    if (prod > INT64_MAX) return INT64_MAX;
    if (prod < INT64_MIN) return INT64_MIN;
    return (q32_32_t)prod;
}