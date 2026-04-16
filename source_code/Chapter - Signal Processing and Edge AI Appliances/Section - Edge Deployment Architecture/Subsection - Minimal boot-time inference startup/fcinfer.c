/* Minimal, production-ready AVX2 GEMV for float32. Assumes long mode and AVX2 enabled. */
/* Model layout: weights base: row-major M x N floats, then bias M floats. */
#include 
#include 
#include 

#define MODEL_BASE ((uintptr_t)0x4000000) /* physical/identity-mapped model base */

/* Compute y[M] = W[M][N] * x[N] + b[M]. All pointers 32-byte aligned for best performance. */
void infer_fc_avx2(const float *W, const float *b, const float *x,
                   float *y, size_t M, size_t N)
{
    const size_t step = 8; /* AVX2 processes 8 float lanes */
    for (size_t i = 0; i < M; ++i) {
        const float *wrow = W + i * N;
        __m256 acc = _mm256_setzero_ps();
        size_t j = 0;
        /* Prefetch first cache lines of the row to reduce jitter */
        _mm_prefetch((const char*)(wrow), _MM_HINT_T0);
        for (; j + step <= N; j += step) {
            __m256 wx = _mm256_load_ps(x + j);          /* aligned load of x[j..j+7] */
            __m256 ww = _mm256_load_ps(wrow + j);      /* aligned load of W[i][j..j+7] */
            acc = _mm256_fmadd_ps(ww, wx, acc);        /* acc += ww * wx */
        }
        /* horizontal sum of acc */
        __m256 hi = _mm256_permute2f128_ps(acc, acc, 1);
        acc = _mm256_add_ps(acc, hi);
        acc = _mm256_hadd_ps(acc, acc);
        acc = _mm256_hadd_ps(acc, acc);
        float sum = _mm256_cvtss_f32(acc);
        /* tail processing (scalar) */
        for (; j < N; ++j) sum += wrow[j] * x[j];
        y[i] = sum + b[i];
    }
}

/* Boot-time invocation: model is at MODEL_BASE; sizes must be known at build or read from header. */
void boot_infer_startup(void) {
    /* Example header at base: uint32_t M, uint32_t N, followed by weights and bias. */
    const uint8_t *base = (const uint8_t *)MODEL_BASE;
    uint32_t M = *(const uint32_t *)(base + 0);
    uint32_t N = *(const uint32_t *)(base + 4);
    const float *weights = (const float *)(base + 8);
    const float *bias = weights + (size_t)M * (size_t)N;
    /* Allocate scratch vectors in statically reserved RAM (not shown). Use known addresses here. */
    float *x = (float *)0x2000000; /* input vector placed at known address */
    float *y = (float *)0x2001000; /* output vector */
    infer_fc_avx2(weights, bias, x, y, M, N);
    /* After inference, jump to control loop or signal completion (platform-specific). */
}