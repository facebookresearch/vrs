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
// CVideoDecoder class is an implementation of the IVideoDecoder interface class.

#pragma once

#include <memory>

#include "FFmpegDecode.h"
#ifdef WITH_NVCODEC
#include "nvDecoder.h"
#endif
#include "xprs.h"

namespace xprs {

class CVideoDecoder : public IVideoDecoder, public VideoCodec {
 public:
  explicit CVideoDecoder(const VideoCodec& codec);
  ~CVideoDecoder() override;
  XprsResult init(bool disableHwAcceleration = false) override;
  XprsResult decodeFrame(Frame& frameOut, const Buffer& compressed) override;

 private:
  void convertAVFrame(const AVFrame* avframe, Frame& frame);

 private:
  std::unique_ptr<InternalDecoder> _decoder;
  Picture _pix;
  int64_t _timeStamp;
  std::vector<uint8_t> _buffer;
  bool _expectHwFrame = false;
};

} // namespace xprs
