// (c) Meta Platforms, Inc. and affiliates. Confidential and proprietary.

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
  CVideoDecoder(const VideoCodec& codec);
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
