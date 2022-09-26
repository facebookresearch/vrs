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

#include "PixelFrame.h"

#include <jpeglib.h>
#include <turbojpeg.h>

#define DEFAULT_LOG_CHANNEL "PixelFrameJpeg"
#include <logging/Log.h>
#include <logging/Verify.h>

namespace vrs::utils {

using namespace std;

bool PixelFrame::readJpegFrame(RecordReader* reader, const uint32_t sizeBytes) {
  if (sizeBytes == 0) {
    return false; // empty image
  }
  // read JPEG data
  vector<uint8_t> jpegBuf;
  jpegBuf.resize(sizeBytes);
  if (!XR_VERIFY(reader->read(jpegBuf.data(), sizeBytes) == 0)) {
    return false;
  }
  return readJpegFrame(jpegBuf);
}

bool PixelFrame::readJpegFrame(const vector<uint8_t>& jpegBuf, bool decodePixels) {
  // setup libjpeg
  struct jpeg_decompress_struct cinfo;
  struct jpeg_error_mgr jerr;
  cinfo.err = jpeg_std_error(&jerr);
  jpeg_create_decompress(&cinfo);
  jpeg_mem_src(&cinfo, jpegBuf.data(), jpegBuf.size());
  jpeg_read_header(&cinfo, TRUE);
  jpeg_start_decompress(&cinfo);
  if (cinfo.num_components == 1) {
    cinfo.out_color_space = JCS_GRAYSCALE;
    init(PixelFormat::GREY8, cinfo.image_width, cinfo.image_height);
  } else {
    cinfo.out_color_space = JCS_RGB;
    init(PixelFormat::RGB8, cinfo.image_width, cinfo.image_height);
  }
  if (decodePixels) {
    // decompress row by row
    uint8_t* rowPtr = frameBytes_.data();
    while (cinfo.output_scanline < cinfo.output_height) {
      jpeg_read_scanlines(&cinfo, &rowPtr, 1);
      rowPtr += imageSpec_.getStride();
    }
  }
  jpeg_finish_decompress(&cinfo);
  jpeg_destroy_decompress(&cinfo);
  return true;
}

bool PixelFrame::readJpegFrame(
    shared_ptr<PixelFrame>& frame,
    RecordReader* reader,
    const uint32_t sizeBytes) {
  if (!frame) {
    frame = make_shared<PixelFrame>();
  }
  return frame->readJpegFrame(reader, sizeBytes);
}

bool PixelFrame::jpgCompress(vector<uint8_t>& outBuffer, uint32_t quality) {
  return jpgCompress(imageSpec_, frameBytes_, outBuffer, quality);
}

bool PixelFrame::jpgCompress(
    const ImageContentBlockSpec& pixelSpec,
    const vector<uint8_t>& pixels,
    vector<uint8_t>& outBuffer,
    uint32_t quality) {
  if (!XR_VERIFY(pixelSpec.getImageFormat() == ImageFormat::RAW) ||
      !XR_VERIFY(
          pixelSpec.getPixelFormat() == PixelFormat::RGB8 ||
          pixelSpec.getPixelFormat() == PixelFormat::GREY8)) {
    return false;
  }
  const bool isGrey8 = (pixelSpec.getChannelCountPerPixel() == 1);
  long unsigned int jpegDataSize = 0;
  unsigned char* jpegData = nullptr;
  const uint8_t* input = pixels.data();

  tjhandle _jpegCompressor = tjInitCompress();

  if (!XR_VERIFY(
          tjCompress2(
              _jpegCompressor,
              input,
              pixelSpec.getWidth(),
              pixelSpec.getStride(),
              pixelSpec.getHeight(),
              isGrey8 ? TJPF_GRAY : TJPF_RGB,
              &jpegData,
              &jpegDataSize,
              isGrey8 ? TJSAMP_GRAY : TJSAMP_444,
              quality,
              TJFLAG_FASTDCT) == 0)) {
    tjDestroy(_jpegCompressor);
    return false;
  }

  XR_VERIFY(tjDestroy(_jpegCompressor) == 0);

  if (XR_VERIFY(jpegData != nullptr)) {
    outBuffer.resize(jpegDataSize);
    memcpy(outBuffer.data(), jpegData, jpegDataSize);
    tjFree(jpegData);
    return true;
  }
  outBuffer.clear();
  return false;
}

} // namespace vrs::utils
