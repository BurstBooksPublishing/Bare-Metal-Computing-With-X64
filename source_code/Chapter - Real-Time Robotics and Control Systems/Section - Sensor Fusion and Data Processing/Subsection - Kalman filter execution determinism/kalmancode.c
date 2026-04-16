#include <stdint.h>
#include <stddef.h>
#include <emmintrin.h>

/* Fixed sizes */
enum { N = 4, M = 2 };

/* Aligned, statically allocated state */
static double __attribute__((aligned(64))) F[N*N];
static double __attribute__((aligned(64))) P[N*N];
static double __attribute__((aligned(64))) Q[N*N];
static double __attribute__((aligned(64))) B[N];
static double __attribute__((aligned(64))) H[M*N];
static double __attribute__((aligned(64))) R[M*M];
static double __attribute__((aligned(64))) x[N];

/* Deterministic 2x2 inverse */
static inline void inv2x2(const double A[4], double invA[4]) {
    double a=A[0], b=A[1], c=A[2], d=A[3];
    double invdet = 1.0 / (a*d - b*c);
    invA[0]= d*invdet; invA[1]=-b*invdet;
    invA[2]=-c*invdet; invA[3]= a*invdet;
}

void kalman_step(const double z[M], const double u) {
    unsigned int csr = _mm_getcsr();
    csr = (csr & ~_MM_ROUND_MASK) | _MM_ROUND_NEAREST;
    _mm_setcsr(csr);

    double xpred[N], Ppred[N*N];
    double y[M], S[M*M], Sinv[M*M], K[N*M];

    /* xpred = F*x + B*u */
    for (size_t i=0;i<N;i++) {
        double acc = 0.0;
        for (size_t j=0;j<N;j++)
            acc += F[i*N+j] * x[j];
        xpred[i] = acc + B[i] * u;
    }

    /* Ppred = F*P*F^T + Q (naive fixed loops) */
    for (size_t i=0;i<N;i++)
        for (size_t j=0;j<N;j++) {
            double acc = 0.0;
            for (size_t k=0;k<N;k++)
                acc += F[i*N+k] * P[k*N+j];
            Ppred[i*N+j] = acc + Q[i*N+j];
        }

    /* y = z - H*xpred */
    for (size_t i=0;i<M;i++) {
        double acc = 0.0;
        for (size_t j=0;j<N;j++)
            acc += H[i*N+j] * xpred[j];
        y[i] = z[i] - acc;
    }

    /* S = H*Ppred*H^T + R (compact form) */
    for (size_t i=0;i<M;i++)
        for (size_t j=0;j<M;j++) {
            double acc = 0.0;
            for (size_t k=0;k<N;k++)
                acc += H[i*N+k] * Ppred[k*N+j];
            S[i*M+j] = acc + R[i*M+j];
        }

    inv2x2(S, Sinv);

    /* K = Ppred*H^T*Sinv (condensed) */
    for (size_t i=0;i<N;i++)
        for (size_t j=0;j<M;j++) {
            double acc = 0.0;
            for (size_t k=0;k<N;k++)
                acc += Ppred[i*N+k] * H[j*N+k];
            K[i*M+j] = acc * Sinv[j*M+j];
        }

    /* x = xpred + K*y */
    for (size_t i=0;i<N;i++) {
        double acc = 0.0;
        for (size_t j=0;j<M;j++)
            acc += K[i*M+j] * y[j];
        x[i] = xpred[i] + acc;
    }
}