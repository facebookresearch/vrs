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
// See xprsUtils.h for details.

#include "xprsUtils.h"
#include "Codecs.h"

namespace xprs {

bool isMonochrome(PixelFormat format) {
  return format == PixelFormat::GRAY8 || format == PixelFormat::GRAY10LE ||
      format == PixelFormat::GRAY12LE;
}

// We always encode monochrome in YUV420 for better HW support, so here Gray is mapped to YUV420.
AVPixelFormat mapToAVPixelFormat(PixelFormat fmt) {
  switch (fmt) {
    case PixelFormat::GRAY8:
      return AV_PIX_FMT_YUV420P;
    case PixelFormat::GRAY10LE:
      return AV_PIX_FMT_YUV420P10LE;
    case PixelFormat::GRAY12LE:
      return AV_PIX_FMT_YUV420P12LE;
    case PixelFormat::YUV420P:
      return AV_PIX_FMT_YUV420P;
    case PixelFormat::NV12:
      return AV_PIX_FMT_NV12;
    case PixelFormat::YUV420P10LE:
      return AV_PIX_FMT_YUV420P10LE;
    case PixelFormat::YUV420P12LE:
      return AV_PIX_FMT_YUV420P12LE;
    case PixelFormat::YUV422P:
      return AV_PIX_FMT_YUV422P;
    case PixelFormat::YUV444P:
      return AV_PIX_FMT_YUV444P;
    case PixelFormat::RGB24:
      return AV_PIX_FMT_RGB24;
    case PixelFormat::GBRP:
      return AV_PIX_FMT_GBRP;
    case PixelFormat::GBRP10LE:
      return AV_PIX_FMT_GBRP10LE;
    case PixelFormat::GBRP12LE:
      return AV_PIX_FMT_GBRP12LE;
    default:
      return AV_PIX_FMT_NONE;
  }
}

PixelFormat mapToPixelFormat(AVPixelFormat fmt) {
  switch (fmt) {
    case AV_PIX_FMT_GRAY8:
      return PixelFormat::GRAY8;
    case AV_PIX_FMT_GRAY10LE:
      return PixelFormat::GRAY10LE;
    case AV_PIX_FMT_GRAY12LE:
      return PixelFormat::GRAY12LE;
    case AV_PIX_FMT_YUV420P:
    case AV_PIX_FMT_YUVJ420P:
      return PixelFormat::YUV420P;
    case AV_PIX_FMT_NV12:
      return PixelFormat::NV12;
    case AV_PIX_FMT_YUV420P10LE:
      return PixelFormat::YUV420P10LE;
    case AV_PIX_FMT_YUV420P12LE:
      return PixelFormat::YUV420P12LE;
    case AV_PIX_FMT_YUV422P:
      return PixelFormat::YUV422P;
    case AV_PIX_FMT_YUV444P:
      return PixelFormat::YUV444P;
    case AV_PIX_FMT_RGB24:
      return PixelFormat::RGB24;
    case AV_PIX_FMT_GBRP:
      return PixelFormat::GBRP;
    case AV_PIX_FMT_GBRP10LE:
      return PixelFormat::GBRP10LE;
    case AV_PIX_FMT_GBRP12LE:
      return PixelFormat::GBRP12LE;
    default:
      return PixelFormat::UNKNOWN;
  }
}

// Color space LUT
// This should match PixelFormat enum orders.
const XprsCsp s_cspInfo[] = {
    {}, /* UNKNOWN */
    {1, false, 8, {0}, {0}}, /* GRAY8 */
    {1, false, 10, {0}, {0}}, /* GRAY10LE */
    {1, false, 12, {0}, {0}}, /* GRAY12LE */
    {3, false, 8, {0, 1, 1}, {0, 1, 1}}, /* YUV420P */
    {2, false, 8, {0, 0}, {0, 1}}, /* NV12 */
    {3, false, 10, {0, 1, 1}, {0, 1, 1}}, /* YUV420P10LE */
    {3, false, 12, {0, 1, 1}, {0, 1, 1}}, /* YUV420P12LE */
    {3, false, 8, {0, 1, 1}, {0, 0, 0}}, /* YUV422P */
    {3, false, 8, {0}, {0}}, /* YUV444P */
    {3, true, 8, {0}, {0}}, /* RGB24 */
    {3, false, 8, {0}, {0}}, /* GBRP */
    {3, false, 10, {0}, {0}}, /* GBRP10LE */
    {3, false, 12, {0}, {0}}, /* GBRP12LE */
    {2, false, 10, {0, 0}, {0, 1}}, /* NV1210LE */
    {2, false, 12, {0, 0}, {0, 1}}, /* NV1212LE */
};

static_assert(sizeof(s_cspInfo) / sizeof(s_cspInfo[0]) == int(PixelFormat::COUNT));

XprsCsp getCsInfo(PixelFormat fmt) {
  return s_cspInfo[int(fmt)];
}

int getBitsPerPixel(PixelFormat fmt) {
  XprsCsp csInfo = getCsInfo(fmt);
  int bits = 0;
  for (int plane = 0; plane < csInfo.channels; plane++) {
    bits += (2 >> csInfo.wShift[plane]) * (2 >> csInfo.hShift[plane]) * csInfo.bitDepth;
  }
  return bits >> 2;
}

uint32_t getNumComponents(PixelFormat fmt) {
  return getCsInfo(fmt).channels;
}

uint32_t getPlaneWidth(const Frame& frame, int plane) {
  if (plane < frame.numPlanes) {
    return frame.width >> getCsInfo(frame.fmt).wShift[plane];
  }
  return 0;
}

AVCodecID mapToAVCodecID(VideoCodecFormat id) {
  switch (id) {
    case VideoCodecFormat::H264:
      return AV_CODEC_ID_H264;
    case VideoCodecFormat::H265:
      return AV_CODEC_ID_H265;
    case VideoCodecFormat::VP9:
      return AV_CODEC_ID_VP9;
    case VideoCodecFormat::AV1:
      return AV_CODEC_ID_AV1;
    default:
      return AV_CODEC_ID_NONE;
  }
}

VideoCodecFormat mapToVideoCodecFormat(AVCodecID id) {
  switch (id) {
    case AV_CODEC_ID_H264:
      return VideoCodecFormat::H264;
    case AV_CODEC_ID_H265:
      return VideoCodecFormat::H265;
    case AV_CODEC_ID_VP9:
      return VideoCodecFormat::VP9;
    case AV_CODEC_ID_AV1:
      return VideoCodecFormat::AV1;
    default:
      return VideoCodecFormat::LAST; // really UNKNOWN
  }
}

int64_t mapQualityToCRF(int quality, int64_t maxCRF, int64_t defaultCRF) {
  int64_t result = defaultCRF;

  if (quality >= 100) {
    result = 0; // use lossless mode
  } else if (quality > 0) {
    result = maxCRF - (quality * maxCRF / 100);
  }

  return int64_t(result);
}

void convertAVFrameToFrame(const AVFrame* const avframe, Frame& frame) {
  frame.ptsMs = avframe->pts;

  auto pixFmt = AVPixelFormat(avframe->format);
  frame.fmt = mapToPixelFormat(pixFmt);
  // set the number of planes
  const XprsCsp cs = getCsInfo(frame.fmt);
  frame.numPlanes = cs.channels;
  // copy the frame plane pointers and stride values (planes are managed by FFmpeg)
  for (int i = 0; i < frame.numPlanes; i++) {
    frame.planes[i] = avframe->data[i];
    frame.stride[i] = avframe->linesize[i];
  }

  frame.width = avframe->width;
  frame.height = avframe->height;
  frame.keyFrame =
      (avframe->pict_type == AV_PICTURE_TYPE_I); // true if this is an I frame; false otherwise
}

const char* convertExceptionToError(const std::exception& exception, XprsResult& error) {
  const auto* cErr = dynamic_cast<const CodecException*>(&exception);
  if (cErr) {
    error = XprsResult::ERR_FFMPEG;
    return cErr->what();
  } else {
    error = XprsResult::ERR_SYSTEM;
    return exception.what();
  }
}

bool checkFileType(std::string filename, std::string filetype) {
  return filename.substr(filename.find_last_of('.') + 1) == filetype;
}

bool isHardwareCodec(const AVCodec* avCodec) {
  bool isHardwareCodec =
      bool(avCodec->capabilities & (AV_CODEC_CAP_HARDWARE | AV_CODEC_CAP_HYBRID));
  // special case for libx264 and libvpx-vp9 since ffmpeg sets
  // AV_CODEC_CAP_CUSTOM_METRICS_CALLBACK bit on the codec capability which is the same as
  // AV_CODEC_CAP_HARDWARE so codec appears as hardware codec
  if (kx264EncoderName == avCodec->name || kVp9EncoderName == avCodec->name) {
    isHardwareCodec = false;
  }
  return isHardwareCodec;
}

} // namespace xprs
