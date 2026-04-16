#include 
#include 

// Process one radix-2 stage in-place.
// data: interleaved complex floats [Re,Im,...], length 2*N floats.
// tw: interleaved twiddles [Re,Im,...] for k=0..stride-1
// N: FFT size (number of complex samples)
// stride: half the butterfly separation (N >> stage)
// Requires: N % (stride*2) == 0, stride % 4 == 0
void fft_stage_avx2(float *data, const float *tw, int N, int stride) {
    const int step = stride * 2;           // distance between butterfly pairs
    for (int block = 0; block < N; block += step) {
        // process groups of 4 complex elements (vector width)
        for (int k = 0; k < stride; k += 4) {
            // load four twiddles: (wr0,wi0,wr1,wi1,wr2,wi2,wr3,wi3)
            __m256 tw_r = _mm256_load_ps(&tw[2*k]);      // loads 8 floats: wr0,wi0,wr1,wi1,...
            // rearrange twiddles into two vectors of real and imag interleaved per lane
            // twiddle real parts in low lanes, imag in high lanes after shuffle
            const __m256 tw_perm = _mm256_permute_ps(tw_r, _MM_SHUFFLE(3,1,2,0));
            // pointers to the two halves of the butterfly
            float *p0 = &data[2*(block + k)];           // base pair addresses
            float *p1 = &data[2*(block + k + stride)];
            // load four complex values from the upper half (b values)
            __m256 b_lo = _mm256_loadu_ps(p1);          // Re0,Im0,Re1,Im1,Re2,Im2,Re3,Im3
            // load corresponding u values
            __m256 a_lo = _mm256_loadu_ps(p0);
            // perform complex multiply v = b * tw
            // split b into real and imag vectors
            __m256 b_shuf = _mm256_shuffle_ps(b_lo, b_lo, _MM_SHUFFLE(2,3,0,1)); // swap Re/Im per pair
            __m256 ac = _mm256_mul_ps(b_lo, tw_r);     // elementwise: Re*wr, Im*wi, ...
            __m256 bd = _mm256_mul_ps(b_shuf, tw_perm);
            // compute complex multiply result interleaved: (ac - bd) and (ac + bd) variants need lane shuffles
            // combine to get v = (Re,Im,...)
            __m256 v = _mm256_addsub_ps(ac, bd);       // produces (Re*wr - Im*wi, Im*wr + Re*wi, ...) per pair
            // y0 = a + v ; y1 = a - v
            __m256 y0 = _mm256_add_ps(a_lo, v);
            __m256 y1 = _mm256_sub_ps(a_lo, v);
            // store results back
            _mm256_storeu_ps(p0, y0);
            _mm256_storeu_ps(p1, y1);
        }
    }
}