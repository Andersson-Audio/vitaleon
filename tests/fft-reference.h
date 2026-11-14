// SPDX-License-Identifier: GPL-3.0-or-later
// Real FFT adapter for the Vital engine, using its bundled BSD-3-Clause KISS FFT.
// Matches JUCE's interleaved positive-frequency layout and normalized inverse.
#pragma once
#include "JuceHeader.h"
#include "kissfft/kissfft.h"
namespace fft_reference {
class FourierTransform {
public:
    explicit FourierTransform(size_t bits) : size_(size_t(1) << bits),
        forward_(size_, false), inverse_(size_, true), input_(size_), output_(size_) {}
    void transformRealForward(float* data) {
        for (size_t i = 0; i < size_; ++i) input_[i] = {data[i], 0};
        forward_.transform(input_.data(), output_.data());
        std::memset(data, 0, 2 * size_ * sizeof(float));
        for (size_t i = 0; i <= size_ / 2; ++i) {
            data[2 * i] = output_[i].real();
            data[2 * i + 1] = output_[i].imag();
        }
        data[1] = data[size_ + 1] = 0;
    }
    void transformRealInverse(float* data) {
        input_[0] = {data[0], 0};
        input_[size_ / 2] = {data[size_], 0};
        for (size_t i = 1; i < size_ / 2; ++i) {
            input_[i] = {data[2 * i], data[2 * i + 1]};
            input_[size_ - i] = std::conj(input_[i]);
        }
        inverse_.transform(input_.data(), output_.data());
        const float scale = 1.0f / float(size_);
        for (size_t i = 0; i < size_; ++i) data[i] = output_[i].real() * scale;
        std::memset(data + size_, 0, size_ * sizeof(float));
    }
private:
    size_t size_;
    kissfft<float> forward_, inverse_;
    std::vector<std::complex<float>> input_, output_;
};
template <size_t bits> class FFT {
public:
    static FourierTransform* transform() {
        static thread_local FourierTransform transform(bits);
        return &transform;
    }
};
}

