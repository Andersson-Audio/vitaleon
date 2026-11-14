#include "fourier_transform.h"
#include "fft-reference.h"
#include <cstdio>
#include <chrono>
#include <random>
int main() {
    std::mt19937 rng(23);std::uniform_real_distribution<float> random(-1,1);
    float forward_error=0,inverse_error=0;
    for(int bits:{5,6,7,8,9,10,11,12}) {
        int n=1<<bits; vital::FourierTransform fast(bits); fft_reference::FourierTransform reference(bits);
        std::vector<float> a(n*2),b(n*2);
        for(int trial=0;trial<8;++trial) {
            for(int i=0;i<n;++i) a[i]=b[i]=random(rng);
            fast.transformRealForward(a.data());reference.transformRealForward(b.data());
            for(int i=0;i<n+2;++i) forward_error=std::max(forward_error,std::abs(a[i]-b[i])/n);
            // Independent arbitrary spectrum; cannot conceal a shared forward/inverse error.
            for(int i=0;i<n+2;++i) a[i]=b[i]=random(rng);
            fast.transformRealInverse(a.data());reference.transformRealInverse(b.data());
            for(int i=0;i<n;++i) inverse_error=std::max(inverse_error,std::abs(a[i]-b[i]));
        }
    }
    std::printf("FFT bins 32..4096: forward max absolute error/N=%g, inverse max absolute error=%g\n",forward_error,inverse_error);
    std::printf("SIMD backend=%s lanes=%d\n",pffft_simd_arch(),pffft_simd_size());
    return forward_error<1e-6 && inverse_error<1e-6 ? 0:1;
}
