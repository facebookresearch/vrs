// Copyright (c) Facebook, Inc. and its affiliates. Confidential and proprietary.

#pragma once

#include <memory>
#include <string>

#include <vrs/utils/DecoderFactory.h>

namespace vrs::utils::test {

constexpr const char* kChessCodecName = "chess_codec";

/// This a pretty trival codec designed to demonstrate codec abilities without using ffmpeg
/// The compressed data just gives primitive instructions to the decoder.
/// We differentiate k-frames from i-frames simply by checking the size of the data to decode...

// Key frames set the entire image to a uniform value
struct IFrameData {
  uint8_t value;
};

// incremental frames set a rectangle within the image to a uniform value
// For a chess board, 8x8, xMax and yMax are always 8, x and y and always in [0, 7]
struct PFrameData {
  uint32_t x;
  uint32_t xMax;
  uint32_t y;
  uint32_t yMax;
  uint8_t incrementValue;
  uint8_t expectedValue;
};

std::unique_ptr<vrs::utils::DecoderI> chessDecoderMaker(const std::string& name);

} // namespace vrs::utils::test
