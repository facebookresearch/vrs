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

//
// VideoDecode class is a C++ interface to FFmpeg video decoding.
// A subset of pixel formats and codec types is supported.

#pragma once

#include "FFmpegUtils.h"

#include "InternalDecoder.h"

namespace xprs {

class VideoDecode : public InternalDecoder {
 public:
  explicit VideoDecode(const char* avcodecName, bool disableHwAcceleration = false);
  virtual ~VideoDecode() override;
  virtual void open() override;
  virtual void decode(uint8_t* buffer, size_t size, Picture& pix) override;
  bool isHwAccelerated() override {
    return _hwEnabled;
  }

 private:
  const AVCodec* _avCodec;
  AVCodecContext* _avContext;
  AVPacket* _avPkt;
  bool _hwEnabled = false;
};

} // namespace xprs
