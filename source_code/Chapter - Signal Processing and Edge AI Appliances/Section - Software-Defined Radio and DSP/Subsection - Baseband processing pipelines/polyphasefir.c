#include 
#include 
#include 

// Compute complex dot product: out_i + j*out_q = sum_{t=0}^{P-1} coeffs[t] * (buf_i[t] + j*buf_q[t])
// buf_i, buf_q: contiguous, 32-byte aligned arrays of length P
// coeffs: contiguous array length P (float)
// Produces scalar outputs via pointers.
static inline void fir_polyphase_dot_avx2(const float *restrict buf_i,
                                          const float *restrict buf_q,
                                          const float *restrict coeffs,
                                          size_t P,
                                          float *restrict out_i,
                                          float *restrict out_q)
{
    // Accumulators as AVX registers (4 floats per register)
    __m256 acc_i = _mm256_setzero_ps();
    __m256 acc_q = _mm256_setzero_ps();

    size_t t = 0;
    const size_t step = 8; // process 8 taps per iteration (8 floats)
    for (; t + step <= P; t += step) {
        // load 8 coeffs, 8 I samples, 8 Q samples
        __m256 c = _mm256_loadu_ps(&coeffs[t]);        // coeffs[t..t+7]
        __m256 xi = _mm256_loadu_ps(&buf_i[t]);       // buf_i[t..t+7]
        __m256 xq = _mm256_loadu_ps(&buf_q[t]);       // buf_q[t..t+7]

        // Multiply-accumulate: acc += c * xi  and acc_q += c * xq
        acc_i = _mm256_fmadd_ps(c, xi, acc_i);
        acc_q = _mm256_fmadd_ps(c, xq, acc_q);
    }
    // tail loop for remaining taps
    for (; t < P; ++t) {
        float c = coeffs[t];
        acc_i = _mm256_add_ps(acc_i, _mm256_set1_ps(c * buf_i[t]));
        acc_q = _mm256_add_ps(acc_q, _mm256_set1_ps(c * buf_q[t]));
    }

    // horizontal sum of acc_i and acc_q
    __m128 low_i  = _mm256_castps256_ps128(acc_i);
    __m128 high_i = _mm256_extractf128_ps(acc_i, 1);
    __m128 sum_i = _mm_add_ps(low_i, high_i);
    sum_i = _mm_hadd_ps(sum_i, sum_i);
    sum_i = _mm_hadd_ps(sum_i, sum_i);

    __m128 low_q  = _mm256_castps256_ps128(acc_q);
    __m128 high_q = _mm256_extractf128_ps(acc_q, 1);
    __m128 sum_q = _mm_add_ps(low_q, high_q);
    sum_q = _mm_hadd_ps(sum_q, sum_q);
    sum_q = _mm_hadd_ps(sum_q, sum_q);

    *out_i = _mm_cvtss_f32(sum_i);
    *out_q = _mm_cvtss_f32(sum_q);
}