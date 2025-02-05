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
  virtual ~CVideoDecoder();
  virtual XprsResult init(const std::string& logFolderPath);
  virtual XprsResult decodeFrame(Frame& frameOut, const Buffer& compressed);

 private:
  std::unique_ptr<InternalDecoder> _decoder;
  Picture _pix;
  int64_t _timeStamp;
};

} // namespace xprs
