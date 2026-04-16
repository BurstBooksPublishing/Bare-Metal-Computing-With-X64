#include 
/* Compute C[8 x 4] += A[8 x K] * B[K x 4]; K multiple of 4 recommended. */
void gemm_8x4_avx2(const float *restrict A, const float *restrict B,
                    float *restrict C, int K, int lda, int ldb, int ldc)
{
    // Accumulators: four columns, each holds 8 rows (two ymm per column if needed)
    __m256 c0 = _mm256_load_ps(&C[0*ldc + 0]); // column 0 rows 0..7
    __m256 c1 = _mm256_load_ps(&C[1*ldc + 0]); // column 1
    __m256 c2 = _mm256_load_ps(&C[2*ldc + 0]); // column 2
    __m256 c3 = _mm256_load_ps(&C[3*ldc + 0]); // column 3

    for (int k = 0; k < K; ++k) {
        // load A column (8 rows) broadcast into vector lanes as needed
        __m256 a = _mm256_load_ps(&A[k*lda + 0]); // A[:,k], aligned
        // load 4 scalars from B row k (B[k,0..3]) and broadcast each
        float b0 = B[k*ldb + 0];
        float b1 = B[k*ldb + 1];
        float b2 = B[k*ldb + 2];
        float b3 = B[k*ldb + 3];
        // fused multiply-add to update accumulators
        c0 = _mm256_fmadd_ps(a, _mm256_set1_ps(b0), c0);
        c1 = _mm256_fmadd_ps(a, _mm256_set1_ps(b1), c1);
        c2 = _mm256_fmadd_ps(a, _mm256_set1_ps(b2), c2);
        c3 = _mm256_fmadd_ps(a, _mm256_set1_ps(b3), c3);
        // optional prefetch next A/B lines
        _mm_prefetch((const char*)&A[(k+4)*lda], _MM_HINT_T0);
        _mm_prefetch((const char*)&B[(k+4)*ldb], _MM_HINT_T0);
    }

    // store back results
    _mm256_store_ps(&C[0*ldc + 0], c0);
    _mm256_store_ps(&C[1*ldc + 0], c1);
    _mm256_store_ps(&C[2*ldc + 0], c2);
    _mm256_store_ps(&C[3*ldc + 0], c3);
}