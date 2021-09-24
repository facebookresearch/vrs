// (c) Facebook, Inc. and its affiliates. Confidential and proprietary.

#include <cstddef>

#pragma once

namespace vrs::utils {

bool convertRaw10ToGrey10(
    void* dst,
    const void* src,
    std::size_t widthInPixels,
    std::size_t heightInPixels,
    std::size_t strideInBytes);

} // namespace vrs::utils
