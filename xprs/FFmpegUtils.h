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
  explicit Picture(AVFrame* avFrame) {
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
  const char* what() const noexcept override;
  int error() const noexcept;

 private:
  static const char* fileName(const char* path);

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
