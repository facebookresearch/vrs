// (c) Meta Platforms, Inc. and affiliates. Confidential and proprietary.

//
// VideoDecode class is a C++ interface to FFmpeg video decoding.
// A subset of pixel formats and codec types is supported.

#pragma once

#include <fstream>
#include <string>

#include "FFmpegUtils.h"

#include "InternalDecoder.h"

namespace xprs {

class VideoDecode : public InternalDecoder {
 public:
  explicit VideoDecode(const char* avcodecName);
  virtual ~VideoDecode() override;
  virtual void open() override;
  virtual void decode(uint8_t* buffer, size_t size, Picture& pix) override;

 private:
  const AVCodec* _avCodec;
  AVCodecContext* _avContext;
  AVPacket* _avPkt;
};

} // namespace xprs
