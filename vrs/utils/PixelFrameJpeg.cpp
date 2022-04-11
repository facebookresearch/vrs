// Copyright (c) Meta Platforms, Inc. and affiliates.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "PixelFrame.h"

#include <jpeglib.h>

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

} // namespace vrs::utils
