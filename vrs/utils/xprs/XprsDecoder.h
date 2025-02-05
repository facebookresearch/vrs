// (c) Meta Platforms, Inc. and affiliates. Confidential and proprietary.

#pragma once

#include <vrs/utils/DecoderFactory.h>

namespace vrs::vxprs {

std::unique_ptr<vrs::utils::DecoderI> xprsDecoderMaker(
    const vector<uint8_t>& encodedFrame,
    void* outDecodedFrame,
    const ImageContentBlockSpec& outputImageSpec);

} // namespace vrs::vxprs
