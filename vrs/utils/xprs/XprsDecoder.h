// (c) Facebook, Inc. and its affiliates. Confidential and proprietary.

#pragma once

#include <vrs/utils/DecoderFactory.h>

namespace vrs::vxprs {

std::unique_ptr<vrs::utils::DecoderI> xprsDecoderMaker(const std::string& codecName);

} // namespace vrs::vxprs
