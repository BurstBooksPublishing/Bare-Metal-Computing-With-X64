#include 
#include 
#include 

// CPUID helper (leaf 1)
static inline void cpuid1(uint32_t *eax, uint32_t *ebx, uint32_t *ecx, uint32_t *edx) {
    uint32_t a=1, b, c, d;
    __asm__ volatile("cpuid"
                     : "=a"(a), "=b"(b), "=c"(c), "=d"(d)
                     : "a"(1));
    *eax = a; *ebx = b; *ecx = c; *edx = d;
}

// Enable XSAVE/AVX at ring0. Caller must be running in long mode with privileges.
static inline void enable_xsave_avx(void) {
    uint32_t eax, ebx, ecx, edx;
    cpuid1(&eax, &ebx, &ecx, &edx);
    // Check OSXSAVE (bit 27) and AVX (bit 28)
    if ((ecx & (1u<<27)) == 0 || (ecx & (1u<<28)) == 0) return; // not supported

    // Set CR4.OSXSAVE (bit 18)
    unsigned long cr4;
    __asm__ volatile("mov %%cr4, %0" : "=r"(cr4));
    cr4 |= (1UL<<18);
    __asm__ volatile("mov %0, %%cr4" :: "r"(cr4));

    // Enable x87, SSE and AVX state in XCR0 by writing 0x7 to XCR0 (EAX=0x7, EDX=0)
    uint32_t a = 0x7, d = 0;
    __asm__ volatile("xsetbv" :: "c"(0), "a"(a), "d"(d));
}

// Complex MAC: zr += ar*b r - ai*b i; zi += ar*b i + ai*b r
void cplx_mac_avx(float *zr, float *zi,
                  const float *ar, const float *ai,
                  const float *br, const float *bi,
                  size_t n_complex) {
    size_t i = 0;
    const size_t stride = 8; // 8 floats per __m256
    for (; i + stride <= n_complex; i += stride) {
        __m256 a_r = _mm256_loadu_ps(ar + i);
        __m256 a_i = _mm256_loadu_ps(ai + i);
        __m256 b_r = _mm256_loadu_ps(br + i);
        __m256 b_i = _mm256_loadu_ps(bi + i);

        // real = a_r*b_r - a_i*b_i
        __m256 real = _mm256_sub_ps(_mm256_mul_ps(a_r, b_r),
                                    _mm256_mul_ps(a_i, b_i));
        // imag = a_r*b_i + a_i*b_r
        __m256 imag = _mm256_add_ps(_mm256_mul_ps(a_r, b_i),
                                    _mm256_mul_ps(a_i, b_r));

        __m256 z_r = _mm256_loadu_ps(zr + i);
        __m256 z_i = _mm256_loadu_ps(zi + i);

        z_r = _mm256_add_ps(z_r, real);
        z_i = _mm256_add_ps(z_i, imag);

        _mm256_storeu_ps(zr + i, z_r);
        _mm256_storeu_ps(zi + i, z_i);
    }
    // Scalar tail
    for (; i < n_complex; ++i) {
        float arv = ar[i], aiv = ai[i], brv = br[i], biv = bi[i];
        float r = arv*brv - aiv*biv;
        float im = arv*biv + aiv*brv;
        zr[i] += r; zi[i] += im;
    }
}