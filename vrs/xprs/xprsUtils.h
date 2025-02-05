// (c) Meta Platforms, Inc. and affiliates. Confidential and proprietary.

//
// xprs Utilities
//

#pragma once

#include "FFmpegUtils.h"
#include "xprs.h"

#include <iostream>

namespace xprs {

#define ERR_LOG(_msg) std::cerr << "[XPRS][ERROR] " << _msg << std::endl;
#define WARN_LOG(_msg) std::cerr << "[XPRS][WARN] " << _msg << std::endl;

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
void convertAVFrameToFrame(const AVFrame* const avframe, Frame& frame);
const char* convertExceptionToError(const std::exception& exception, XprsResult& error);
bool checkFileType(std::string filename, std::string filetype);
uint32_t parseH26xHeaders(uint8_t* ptr, uint32_t size, bool isH264);
bool isHardwareCodec(const AVCodec* avCodec);
} // namespace xprs
