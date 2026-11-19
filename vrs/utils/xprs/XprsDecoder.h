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
#include <vector>

#include <vrs/utils/DecoderFactory.h>

namespace xprs {
struct Frame;
}

namespace vrs::vxprs {

namespace detail {
// Decoder-produced LE samples are LSB-aligned, including NV1210LE (not P010).
bool normalizeMain10ForRgb8(
    const xprs::Frame& source,
    std::vector<uint8_t>& inOutStorage,
    xprs::Frame& outNormalized);

int convertMain10ForRgb8(
    const xprs::Frame& source,
    void* outDecodedFrame,
    const ImageContentBlockSpec& outputImageSpec,
    std::vector<uint8_t>& inOutStorage);
} // namespace detail

using DecoderOptions = vrs::utils::DecoderOptions;

std::unique_ptr<vrs::utils::DecoderI> xprsDecoderMaker(
    const vector<uint8_t>& encodedFrame,
    void* outDecodedFrame,
    const ImageContentBlockSpec& outputImageSpec);

std::unique_ptr<vrs::utils::DecoderI> xprsDecoderMaker(
    const vector<uint8_t>& encodedFrame,
    void* outDecodedFrame,
    const ImageContentBlockSpec& outputImageSpec,
    const DecoderOptions& options);

} // namespace vrs::vxprs
