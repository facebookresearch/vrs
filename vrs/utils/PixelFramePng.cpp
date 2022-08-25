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

#include <png.h>
#include <cerrno>

#include <atomic>
#include <deque>
#include <vector>

#define DEFAULT_LOG_CHANNEL "PixelFramePng"
#include <logging/Log.h>
#include <logging/Verify.h>

#include <vrs/helpers/MemBuffer.h>
#include <vrs/os/Utils.h>

namespace {
constexpr size_t kPngSigBytes = 8;
}

namespace vrs::utils {

using namespace std;

struct SourceBuffer {
  SourceBuffer(const vector<uint8_t>& abuffer) : buffer{abuffer} {}
  const vector<uint8_t>& buffer;
  size_t readSize = 0;
};

static void pngStreamRead(png_structp pngPtr, png_bytep data, png_size_t length) {
  SourceBuffer* src = (SourceBuffer*)png_get_io_ptr(pngPtr);
  if (XR_VERIFY(src->readSize + length <= src->buffer.size())) {
    memcpy(data, src->buffer.data() + src->readSize, length);
    src->readSize += length;
  } else {
    memset(data, 0, length);
  }
}

bool PixelFrame::readPngFrame(RecordReader* reader, const uint32_t sizeBytes) {
  if (sizeBytes < kPngSigBytes) {
    return false; // empty image
  }
  // read PNG data
  vector<uint8_t> buffer(sizeBytes);
  if (!XR_VERIFY(reader->read(buffer.data(), sizeBytes) == 0)) {
    return false;
  }
  return readPngFrame(buffer);
}

bool PixelFrame::readPngFrame(const vector<uint8_t>& pngBuffer, bool decodePixels) {
  // Let LibPNG check the sig.
  SourceBuffer src(pngBuffer);
  if (png_sig_cmp(src.buffer.data(), 0, kPngSigBytes) != 0) {
    XR_LOGE("Payload isn't PNG data");
    return false;
  }
  src.readSize += kPngSigBytes;
  // Create the png read struct. The 3 NULL's at the end can be used
  // for custom error handling functions.
  png_structp pngPtr = png_create_read_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
  if (!pngPtr) {
    XR_LOGE("Could not initialize png read struct.");
    return false;
  }
  // Create the png info struct.
  png_infop infoPtr = png_create_info_struct(pngPtr);
  if (!infoPtr) {
    XR_LOGE("Could not initialize png info struct.");
    png_destroy_read_struct(&pngPtr, (png_infopp)0, (png_infopp)0);
    return false;
  }
  png_bytep* rowPtrs = nullptr;
  if (setjmp(png_jmpbuf(pngPtr))) {
    // An error occured, so clean up what we have allocated so far.
    png_destroy_read_struct(&pngPtr, &infoPtr, (png_infopp)0);
    if (rowPtrs != nullptr) {
      delete[] rowPtrs;
    }
    XR_LOGE("An error occured while reading the PNG file.");
    // libPNG will jump to here if something goes wrong.
    return false;
  }
  png_set_read_fn(pngPtr, (png_voidp)&src, pngStreamRead);

  // Set the amount signature bytes we've already read.
  png_set_sig_bytes(pngPtr, kPngSigBytes);

  // Now call png_read_info with pngPtr as image handle, and infoPtr to receive the file info.
  png_read_info(pngPtr, infoPtr);

  png_uint_32 imgWidth = png_get_image_width(pngPtr, infoPtr);
  png_uint_32 imgHeight = png_get_image_height(pngPtr, infoPtr);
  // bits per CHANNEL - not per pixel!
  png_uint_32 bitdepth = png_get_bit_depth(pngPtr, infoPtr);
  // Number of channels
  png_uint_32 channels = png_get_channels(pngPtr, infoPtr);
  // Color type. (GRAY, RGB, RGBA, Luminance, luminance alpha... palette... etc)
  png_uint_32 colorType = png_get_color_type(pngPtr, infoPtr);

  if (colorType == PNG_COLOR_TYPE_GRAY) {
    if (channels != 1) {
      XR_LOGE("Multi-channel grey images make no sense...");
      return false;
    }
    init(ImageContentBlockSpec(PixelFormat::GREY8, imgWidth, imgHeight));
  } else if (colorType == PNG_COLOR_TYPE_RGB) {
    if (channels != 3) {
      XR_LOGE("{} channels color images make no sense with PNG_COLOR_TYPE_RGB...", channels);
      return false;
    }
    init(ImageContentBlockSpec(PixelFormat::RGB8, imgWidth, imgHeight));
  } else if (colorType == PNG_COLOR_TYPE_RGB_ALPHA) {
    if (channels != 4) {
      XR_LOGE("{} channels color images make no sense with PNG_COLOR_TYPE_RGB_ALPHA...", channels);
      return false;
    }
    init(ImageContentBlockSpec(PixelFormat::RGBA8, imgWidth, imgHeight));
  } else {
    XR_LOGE("Only gray and rgb images are supported.");
    return false;
  }

  if (decodePixels) {
    if (bitdepth < 8) {
      png_set_expand_gray_1_2_4_to_8(pngPtr);
      // And the bitdepth info
      bitdepth = 8;
    }
    // We don't support 16 bit precision, so if the image Has 16 bits per channel
    // precision, round it down to 8.
    if (bitdepth == 16) {
      png_set_strip_16(pngPtr);
      bitdepth = 8;
    }

    // Update the information structs with the transformations we requested:
    png_read_update_info(pngPtr, infoPtr);

    // Array of row pointers. One for every row.
    vector<png_bytep> pngBytep(imgHeight);
    const size_t stride = imageSpec_.getStride();
    for (size_t i = 0; i < imgHeight; ++i) {
      // Set the pointer to the data pointer + i times the row stride.
      pngBytep[i] = reinterpret_cast<png_bytep>(frameBytes_.data()) + i * stride;
    }

    // Read the imagedata and write it to the rowptrs
    png_read_image(pngPtr, pngBytep.data());

    png_read_end(pngPtr, infoPtr);
  }

  // Clean up the read and info structs
  png_destroy_read_struct(&pngPtr, &infoPtr, (png_infopp)0);

  return true;
}

bool PixelFrame::readPngFrame(
    shared_ptr<PixelFrame>& frame,
    RecordReader* reader,
    const uint32_t sizeBytes) {
  if (!frame) {
    frame = make_shared<PixelFrame>();
  }
  return frame->readPngFrame(reader, sizeBytes);
}

namespace {

atomic<uint32_t> sAllocSize{128 * 1024};

void mem_png_write_data(png_structp png_ptr, png_bytep data, png_size_t length) {
  helpers::MemBuffer* buffer = reinterpret_cast<helpers::MemBuffer*>(png_get_io_ptr(png_ptr));
  buffer->addData(data, length);
}

void mem_png_flush(png_structp png_ptr) {}

} // namespace

int PixelFrame::writeAsPng(const string& filename, std::vector<uint8_t>* const outBuffer) {
  PixelFormat pixelFormat = this->getPixelFormat();
  if (!XR_VERIFY(
          pixelFormat == PixelFormat::GREY8 || pixelFormat == PixelFormat::GREY16 ||
          pixelFormat == PixelFormat::RGB8 || pixelFormat == PixelFormat::RGBA8)) {
    XR_LOGE("Pixel format {} not supported for PNG export.", toString(pixelFormat));
    return INVALID_REQUEST; // unsupported formats
  }

  png_structp pngStruct = png_create_write_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
  if (pngStruct == nullptr) {
    XR_LOGE("png_create_write_struct failed");
    return FAILURE;
  }

  png_infop pngInfo = png_create_info_struct(pngStruct);
  if (pngInfo == nullptr) {
    XR_LOGE("png_create_info_struct failed");
    return FAILURE;
  }

  if (setjmp(png_jmpbuf(pngStruct))) {
    XR_LOGE("png's setjmp(png_jmpbuf(png)) failed");
    return FAILURE;
  }

  FILE* file = nullptr;
  helpers::MemBuffer memBuffer;
  if (outBuffer == nullptr) {
    file = os::fileOpen(filename, "wb");
    if (file == nullptr) {
      XR_LOGE("Can't create file '{}'", filename);
      return errno != 0 ? errno : FAILURE;
    }
    png_init_io(pngStruct, file);
  } else {
    png_set_write_fn(pngStruct, &memBuffer, mem_png_write_data, mem_png_flush);
  }

  // determine the pixel type for PNG
  // NOTE: PNG images are written in RGB8 pixels, so for BGR8 format,
  // we flip the bytes (by png_set_bgr), and write RGB8 instead.
  int pngPixelFormat = 0;
  int pixelBitSize = 8;
  switch (pixelFormat) {
    case PixelFormat::RGB8:
      pngPixelFormat = PNG_COLOR_TYPE_RGB;
      break;
    case PixelFormat::RGBA8:
      pngPixelFormat = PNG_COLOR_TYPE_RGB_ALPHA;
      break;
    case PixelFormat::GREY8:
      pngPixelFormat = PNG_COLOR_TYPE_GRAY;
      break;
    case PixelFormat::GREY16:
      pngPixelFormat = PNG_COLOR_TYPE_GRAY;
      pixelBitSize = 16;
      break;
    default:
      XR_LOGE("Unsupported pixel format: {}", toString(pixelFormat));
  }

  uint32_t width = this->getWidth();
  uint32_t height = this->getHeight();

  png_set_IHDR(
      pngStruct,
      pngInfo,
      width,
      height,
      pixelBitSize,
      pngPixelFormat,
      PNG_INTERLACE_NONE,
      PNG_COMPRESSION_TYPE_DEFAULT,
      PNG_FILTER_TYPE_DEFAULT);

  if (pixelFormat == PixelFormat::BGR8) {
    png_set_bgr(pngStruct);
  }

  png_write_info(pngStruct, pngInfo);

  // Swap endian-ness
  if (pixelBitSize > 8) {
    png_set_swap(pngStruct);
  }

  vector<png_byte*> rows(height);
  // if provided, stride is the actual number of bytes used per line
  const uint8_t* bytes = this->rdata();
  const uint32_t lineByteLength = getStride();
  for (uint32_t h = 0; h < height; h++) {
    rows[h] = const_cast<png_byte*>(bytes + lineByteLength * h);
  }
  png_write_image(pngStruct, rows.data());
  png_write_end(pngStruct, nullptr);
  png_destroy_write_struct(&pngStruct, &pngInfo);

  if (outBuffer == nullptr) {
    os::fileClose(file);
  } else {
    memBuffer.getData(*outBuffer);
    size_t totalSize = outBuffer->size();
    if (totalSize > sAllocSize) {
      sAllocSize.store(totalSize + totalSize / 100, memory_order_relaxed);
    }
  }

  return SUCCESS;
}

} // namespace vrs::utils
