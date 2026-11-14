// SPDX-License-Identifier: GPL-3.0-or-later
// SIMD real FFT adapter for the Vital engine, using PFFFT (ARM NEON / x86 SSE).
// Matches JUCE's interleaved positive-frequency layout and normalized inverse.
#pragma once
#include "JuceHeader.h"
#define PFFFT_STATIC_DEFINE
#include "pffft/pffft.h"
namespace vital {
class FourierTransform {
public:
    explicit FourierTransform(size_t bits) : size_(size_t(1) << bits),
        setup_(pffft_new_setup(int(size_), PFFFT_REAL), pffft_destroy_setup),
        buffer_(static_cast<float*>(pffft_aligned_malloc(size_ * sizeof(float))), pffft_aligned_free),
        scratch_(static_cast<float*>(pffft_aligned_malloc(size_ * sizeof(float))), pffft_aligned_free) {
        if (!setup_ || !buffer_ || !scratch_) throw std::bad_alloc();
    }
    void transformRealForward(float* data) {
        std::memcpy(buffer_.get(), data, size_ * sizeof(float));
        pffft_transform_ordered(setup_.get(), buffer_.get(), buffer_.get(), scratch_.get(), PFFFT_FORWARD);
        std::memcpy(data, buffer_.get(), size_ * sizeof(float));
        std::memset(data + size_, 0, size_ * sizeof(float));
        data[size_] = data[1]; // PFFFT packs Nyquist into the DC imaginary slot.
        data[1] = 0;
    }
    void transformRealInverse(float* data) {
        std::memcpy(buffer_.get(), data, size_ * sizeof(float));
        buffer_.get()[1] = data[size_];
        pffft_transform_ordered(setup_.get(), buffer_.get(), buffer_.get(), scratch_.get(), PFFFT_BACKWARD);
        const float scale = 1.0f / float(size_);
        for (size_t i = 0; i < size_; ++i) data[i] = buffer_.get()[i] * scale;
        std::memset(data + size_, 0, size_ * sizeof(float));
    }
private:
    size_t size_;
    std::unique_ptr<PFFFT_Setup, decltype(&pffft_destroy_setup)> setup_;
    std::unique_ptr<float, decltype(&pffft_aligned_free)> buffer_, scratch_;
};
template <size_t bits> class FFT {
public:
    static FourierTransform* transform() {
        static thread_local FourierTransform transform(bits);
        return &transform;
    }
};
}
