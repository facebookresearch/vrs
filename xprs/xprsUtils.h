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
// xprs Utilities
//

#pragma once

#include "FFmpegUtils.h"
#include "xprs.h"

#include <iostream>

namespace xprs {

struct XprsCsp {
  int channels;
  bool packed;
  int bitDepth;
  int wShift[4];
  int hShift[4];
};

extern const XprsCsp s_cspInfo[];
XprsCsp getCsInfo(xprs::PixelFormat fmt);
int getBitsPerPixel(PixelFormat fmt);
uint32_t getNumComponents(PixelFormat fmt);
bool isMonochrome(PixelFormat format);

// Frame utilities
uint32_t getPlaneWidth(const Frame& frame, int plane);

AVPixelFormat mapToAVPixelFormat(PixelFormat fmt);
PixelFormat mapToPixelFormat(AVPixelFormat fmt);
AVCodecID mapToAVCodecID(VideoCodecFormat id);
VideoCodecFormat mapToVideoCodecFormat(AVCodecID id);
int64_t mapQualityToCRF(int quality, int64_t maxCRF, int64_t defaultCRF);
void convertAVFrameToFrame(const AVFrame* avframe, Frame& frame);
const char* convertExceptionToError(const std::exception& exception, XprsResult& error);
bool checkFileType(std::string filename, std::string filetype);
uint32_t parseH26xHeaders(uint8_t* ptr, uint32_t size, bool isH264);
bool isHardwareCodec(const AVCodec* avCodec);
} // namespace xprs
