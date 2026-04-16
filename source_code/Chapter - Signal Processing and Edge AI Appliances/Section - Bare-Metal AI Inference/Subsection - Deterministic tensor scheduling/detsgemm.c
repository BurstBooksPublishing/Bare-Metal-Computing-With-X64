#include 
#include 
/* Preconditions: A,B,C buffers page-locked, 64-byte aligned, caller disables interrupts. */
void tiled_sgemm_det(const float *A, const float *B, float *C,
                     size_t M, size_t N, size_t K,
                     size_t MR, size_t NR, size_t KR) {
    const size_t s = sizeof(float);
    for (size_t mi = 0; mi < M; mi += MR) {
        size_t mlen = (M - mi < MR) ? (M - mi) : MR;
        for (size_t ni = 0; ni < N; ni += NR) {
            size_t nlen = (N - ni < NR) ? (N - ni) : NR;
            /* Initialize C-tile deterministically */
            for (size_t i = 0; i < mlen; ++i) {
                for (size_t j = 0; j < nlen; ++j)
                    C[(mi + i) * N + (ni + j)] = 0.0f;
            }
            /* K-tiling with prefetch of next K-tile */
            for (size_t ki = 0; ki < K; ki += KR) {
                size_t klen = (K - ki < KR) ? (K - ki) : KR;
                /* Prefetch next K-tile deterministically */
                if (ki + KR < K) {
                    const float *A_next = A + (mi * K) + (ki + KR);
                    const float *B_next = B + ((ki + KR) * N) + ni;
                    for (size_t p = 0; p < klen; p += 64 / s) {
                        __builtin_prefetch(A_next + p, 0, 3);
                        __builtin_prefetch(B_next + p * N / klen, 0, 3);
                    }
                }
                /* Micro-kernel: simple blocked multiply for determinism */
                for (size_t i = 0; i < mlen; ++i) {
                    for (size_t k = 0; k < klen; ++k) {
                        __m256 a_vec = _mm256_set1_ps(A[(mi + i) * K + (ki + k)]);
                        size_t bj = 0;
                        for (; bj + 8 <= nlen; bj += 8) {
                            __m256 b_vec = _mm256_load_ps(B + (ki + k) * N + (ni + bj));
                            __m256 c_vec = _mm256_load_ps(C + (mi + i) * N + (ni + bj));
                            c_vec = _mm256_fmadd_ps(a_vec, b_vec, c_vec);
                            _mm256_store_ps(C + (mi + i) * N + (ni + bj), c_vec);
                        }
                        for (; bj < nlen; ++bj) {
                            C[(mi + i) * N + (ni + bj)] +=
                                A[(mi + i) * K + (ki + k)] * B[(ki + k) * N + (ni + bj)];
                        }
                    }
                }
            }
        }
    }
}