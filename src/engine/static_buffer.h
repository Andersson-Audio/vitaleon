// SPDX-License-Identifier: GPL-3.0-or-later
// Andersson Audio, 2026-11-13: exact constant-buffer detection. No decimation.
#pragma once
#include "common.h"
namespace vital {
inline bool isExactConstantBuffer(const poly_float* buffer, int size) {
  for (int i = 1; i < size; ++i)
    if (poly_float::notEqual(buffer[0], buffer[i]).anyMask()) return false;
  return true;
}
}
