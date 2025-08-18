/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include <cstdint>

namespace vrs::utils {

bool convertGrey10PackedToGrey16(
    void* dst,
    uint32_t dstSize,
    uint32_t dstStride,
    const void* src,
    uint32_t srcSize,
    uint32_t width,
    uint32_t height,
    uint32_t srcStride);

bool convertGrey10PackedToGrey8(
    void* dst,
    uint32_t dstSize,
    uint32_t dstStride,
    const void* src,
    uint32_t srcSize,
    uint32_t width,
    uint32_t height,
    uint32_t srcStride);

bool convertGrey10ToGrey10Packed(
    void* dst,
    uint32_t dstSize,
    uint32_t dstStride,
    const void* src,
    uint32_t srcSize,
    uint32_t width,
    uint32_t height,
    uint32_t srcStride);

} // namespace vrs::utils
