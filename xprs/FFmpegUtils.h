// (c) Meta Platforms, Inc. and affiliates. Confidential and proprietary.

//

#pragma once
#include <string>
#include <vector>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/imgutils.h>
#include <libavutil/log.h>
#include <libavutil/opt.h>
}

namespace xprs {

using PacketList = std::vector<AVPacket*>;

/*
  Universal picture frame for input to video encoding or output from video
  decoding. Individual planes and line sizes must be filled in by caller.
*/
class Picture {
 public:
  Picture();
  Picture(AVFrame* avFrame) {
    _avFrame = avFrame;
  }
  ~Picture();
  int64_t& pts() {
    return _avFrame->pts;
  }
  int& format() {
    return _avFrame->format;
  }
  int& width() {
    return _avFrame->width;
  }
  int& height() {
    return _avFrame->height;
  }
  AVFrame* avFrame() {
    return _avFrame;
  }

 protected:
  AVFrame* _avFrame;
};

#define INVOKE_CODEC_EXCEPTION_MESSAGE(msg) CodecException(__FILE__, __LINE__, (msg))
#define INVOKE_CODEC_EXCEPTION_CODE(code) CodecException(__FILE__, __LINE__, (code))

class CodecException : public std::exception {
 public:
  CodecException(const char* file, int line, const char* msg);
  CodecException(const char* file, int line, int error);
  const char* what() const noexcept;
  int error() const noexcept;

 private:
  const char* fileName(const char* path);

  mutable std::string errString;
  std::string _file;
  int _line;
  std::string _msg;
  int _error;
};

union CodecPreset {
  const char* preset;
  int cpuUsed;
};

CodecPreset mapToCodecPreset(const char* preset, AVCodecID id, const char* avcodecName = nullptr);

} // namespace xprs
