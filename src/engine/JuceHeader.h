#pragma once
// Minimal standard-library compatibility for Vital DSP, not the JUCE framework.
#include <algorithm>
#include <array>
#include <atomic>
#include <cassert>
#include <cmath>
#include <complex>
#include <cstring>
#include <functional>
#include <limits>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>
#define JUCE_LEAK_DETECTOR(Type)
#define JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Type) \
    Type(const Type&) = delete; Type& operator=(const Type&) = delete;
#if defined(__arm__) && !defined(__aarch64__)
#define NEON_ARM32 1
#endif
