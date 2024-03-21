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

#define DEFAULT_LOG_CHANNEL "PixelFramePng"
#include <logging/Log.h>
#include <logging/Verify.h>

#include <vrs/helpers/MemBuffer.h>
#include <vrs/os/Utils.h>

#ifdef USE_WUFFS

#define WUFFS_IMPLEMENTATION
#define WUFFS_CONFIG__STATIC_FUNCTIONS
#define WUFFS_CONFIG__MODULES
#define WUFFS_CONFIG__MODULE__ADLER32
#define WUFFS_CONFIG__MODULE__BASE
#define WUFFS_CONFIG__MODULE__CRC32
#define WUFFS_CONFIG__MODULE__DEFLATE
#define WUFFS_CONFIG__MODULE__PNG
#define WUFFS_CONFIG__MODULE__ZLIB
#include <wuffs-v0.3.c>

#endif

namespace {
constexpr size_t kPngSigBytes = 8;
}

namespace vrs::utils {

using namespace std;

#ifdef USE_WUFFS

static bool readPngFrameWithWuff(PixelFrame& frame, const vector<uint8_t>& pngBuffer) {
  wuffs_base__io_buffer src =
      wuffs_base__ptr_u8__reader(const_cast<uint8_t*>(pngBuffer.data()), pngBuffer.size(), true);

  wuffs_png__decoder dec;
  memset((void*)&dec, 0, sizeof(wuffs_png__decoder));
  if (wuffs_png__decoder__initialize(
          &dec, sizeof(wuffs_png__decoder), WUFFS_VERSION, WUFFS_INITIALIZE__ALREADY_ZEROED)
          .repr) {
    XR_LOGE("Failed to initialize wuffs png decoder!");
    return false;
  }

  wuffs_base__image_config ic;
  wuffs_base__status status = wuffs_png__decoder__decode_image_config(&dec, &ic, &src);
  if (status.repr) {
    XR_LOGE("Invalid png!");
    return false;
  }

  uint32_t dim_x = wuffs_base__pixel_config__width(&ic.pixcfg);
  uint32_t dim_y = wuffs_base__pixel_config__height(&ic.pixcfg);
  size_t num_pixels = dim_x * dim_y;
  if (num_pixels > (SIZE_MAX / 4)) {
    XR_LOGE("Too large png!");
    return false;
  }
  auto pixel_format = wuffs_base__pixel_config__pixel_format(&ic.pixcfg);

  if (!wuffs_base__pixel_format__is_interleaved(&pixel_format)) {
    // XR_LOGE("PNG must be interleaved!");
    return false;
  }

  int bpp = wuffs_base__pixel_format__bits_per_pixel(&pixel_format);
  int channels = 0;
  uint32_t pixfmt_repr = 0;
  switch (pixel_format.repr) {
    case WUFFS_BASE__PIXEL_FORMAT__A:
      pixfmt_repr = WUFFS_BASE__PIXEL_FORMAT__A;
      channels = 1;
      break;
    case WUFFS_BASE__PIXEL_FORMAT__Y:
      pixfmt_repr = WUFFS_BASE__PIXEL_FORMAT__Y;
      channels = 1;
      break;
    case WUFFS_BASE__PIXEL_FORMAT__INDEXED__BGRA_PREMUL:
      pixfmt_repr = WUFFS_BASE__PIXEL_FORMAT__RGBA_PREMUL;
      channels = 4;
      break;
    case WUFFS_BASE__PIXEL_FORMAT__INDEXED__BGRA_BINARY:
      pixfmt_repr = WUFFS_BASE__PIXEL_FORMAT__RGBA_BINARY;
      channels = 4;
      break;
    case WUFFS_BASE__PIXEL_FORMAT__BGR_565:
      pixfmt_repr = WUFFS_BASE__PIXEL_FORMAT__RGB;
      channels = 3;
      break;
    case WUFFS_BASE__PIXEL_FORMAT__BGR:
      pixfmt_repr = WUFFS_BASE__PIXEL_FORMAT__RGB;
      channels = 3;
      break;
    case WUFFS_BASE__PIXEL_FORMAT__BGRA_NONPREMUL:
      pixfmt_repr = WUFFS_BASE__PIXEL_FORMAT__RGBA_NONPREMUL;
      channels = 4;
      break;
    case WUFFS_BASE__PIXEL_FORMAT__BGRA_NONPREMUL_4X16LE:
      pixfmt_repr = WUFFS_BASE__PIXEL_FORMAT__RGBA_NONPREMUL;
      channels = 4;
      break;
    case WUFFS_BASE__PIXEL_FORMAT__BGRA_PREMUL:
      pixfmt_repr = WUFFS_BASE__PIXEL_FORMAT__RGBA_PREMUL;
      channels = 4;
      break;
    case WUFFS_BASE__PIXEL_FORMAT__BGRA_PREMUL_4X16LE:
      pixfmt_repr = WUFFS_BASE__PIXEL_FORMAT__RGBA_PREMUL;
      channels = 4;
      break;
    case WUFFS_BASE__PIXEL_FORMAT__BGRA_BINARY:
      pixfmt_repr = WUFFS_BASE__PIXEL_FORMAT__RGBA_BINARY;
      channels = 4;
      break;
    case WUFFS_BASE__PIXEL_FORMAT__BGRX:
      pixfmt_repr = WUFFS_BASE__PIXEL_FORMAT__RGBX;
      channels = 4;
      break;
    case WUFFS_BASE__PIXEL_FORMAT__RGB:
      pixfmt_repr = WUFFS_BASE__PIXEL_FORMAT__RGB;
      channels = 3;
      break;
    case WUFFS_BASE__PIXEL_FORMAT__RGBA_NONPREMUL:
      pixfmt_repr = WUFFS_BASE__PIXEL_FORMAT__RGBA_NONPREMUL;
      channels = 4;
      break;
    case WUFFS_BASE__PIXEL_FORMAT__RGBA_NONPREMUL_4X16LE:
      pixfmt_repr = WUFFS_BASE__PIXEL_FORMAT__RGBA_NONPREMUL;
      channels = 4;
      break;
    case WUFFS_BASE__PIXEL_FORMAT__RGBA_PREMUL:
      pixfmt_repr = WUFFS_BASE__PIXEL_FORMAT__RGBA_PREMUL;
      channels = 4;
      break;
    case WUFFS_BASE__PIXEL_FORMAT__RGBA_PREMUL_4X16LE:
      pixfmt_repr = WUFFS_BASE__PIXEL_FORMAT__RGBA_PREMUL;
      channels = 4;
      break;
    case WUFFS_BASE__PIXEL_FORMAT__RGBA_BINARY:
      pixfmt_repr = WUFFS_BASE__PIXEL_FORMAT__RGBA_BINARY;
      channels = 4;
      break;
    case WUFFS_BASE__PIXEL_FORMAT__RGBX:
      pixfmt_repr = WUFFS_BASE__PIXEL_FORMAT__RGBX;
      channels = 4;
      break;

    default: {
      if (bpp % 8 != 0) {
        // XR_LOGE("Wrong bit ber pixel is not divisible by 8 (12 bit PNG?)");
        return false;
      }

      // TODO: figure out more reliable way to get channel count from wuffs
      channels = bpp / 8;
      switch (channels) {
        case 1:
          pixfmt_repr = WUFFS_BASE__PIXEL_FORMAT__A;
          break;
        case 3:
          pixfmt_repr = WUFFS_BASE__PIXEL_FORMAT__RGB;
          break;
        case 4:
          pixfmt_repr = WUFFS_BASE__PIXEL_FORMAT__RGBA_NONPREMUL;
          break;
        default:
          // XR_LOGE("Number of channels is not supported");
          return false;
      }
    }
  }

  wuffs_base__pixel_config__set(
      &ic.pixcfg, pixfmt_repr, WUFFS_BASE__PIXEL_SUBSAMPLING__NONE, dim_x, dim_y);

  switch (channels) {
    case 1:
      frame.init(ImageContentBlockSpec(PixelFormat::GREY8, dim_x, dim_y));
      break;
    case 3:
      frame.init(ImageContentBlockSpec(PixelFormat::RGB8, dim_x, dim_y));
      break;
    case 4:
      frame.init(ImageContentBlockSpec(PixelFormat::RGBA8, dim_x, dim_y));
      break;
    default:
      XR_LOGE("PNG pixel format not supported");
      return false;
  }
  uint8_t* dst_ptr = frame.wdata();

  size_t workbuf_len = wuffs_png__decoder__workbuf_len(&dec).max_incl;
  if (workbuf_len > SIZE_MAX) {
    XR_LOGE("Invalid png! workbuf_len > SIZE_MAX");
    return false;
  }

  vector<uint8_t> workbuf_slice_buffer(workbuf_len);
  wuffs_base__slice_u8 workbuf_slice =
      wuffs_base__make_slice_u8(workbuf_slice_buffer.data(), workbuf_len);

  wuffs_base__slice_u8 pixbuf_slice = wuffs_base__make_slice_u8(dst_ptr, num_pixels * channels);
  if (!pixbuf_slice.ptr) {
    XR_LOGE("Failed to create pixbuf_slice");
    return false;
  }

  wuffs_base__pixel_buffer pb;
  status = wuffs_base__pixel_buffer__set_from_slice(&pb, &ic.pixcfg, pixbuf_slice);
  if (status.repr) {
    XR_LOGE("Failed to setup pixel_buffer");
    return false;
  }

  while (true) {
    wuffs_base__frame_config fc;
    status = wuffs_png__decoder__decode_frame_config(&dec, &fc, &src);
    if (status.repr == wuffs_base__note__end_of_data) {
      break;
    }
    status = wuffs_png__decoder__decode_frame(
        &dec, &pb, &src, WUFFS_BASE__PIXEL_BLEND__SRC, workbuf_slice, nullptr);
    if (status.repr) {
      XR_LOGE("Failed to decode png!");
      return false;
    }
  }

  return true;
}

#endif

struct SourceBuffer {
  explicit SourceBuffer(const vector<uint8_t>& abuffer) : buffer{abuffer} {}
  const vector<uint8_t>& buffer;
  size_t readSize = 0;
};

bool PixelFrame::readPngFrame(RecordReader* reader, uint32_t sizeBytes) {
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

static void pngStreamRead(png_structp pngPtr, png_bytep data, png_size_t length) {
  SourceBuffer* src = (SourceBuffer*)png_get_io_ptr(pngPtr);
  if (XR_VERIFY(src->readSize + length <= src->buffer.size())) {
    memcpy(data, src->buffer.data() + src->readSize, length);
    src->readSize += length;
  } else {
    memset(data, 0, length);
  }
}

bool PixelFrame::readPngFrame(const vector<uint8_t>& pngBuffer, bool decodePixels) {
#ifdef USE_WUFFS
  if (decodePixels && readPngFrameWithWuff(*this, pngBuffer)) {
    return true;
  }
#endif
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
    png_destroy_read_struct(&pngPtr, nullptr, nullptr);
    return false;
  }
  png_bytep* rowPtrs = nullptr;
  if (setjmp(png_jmpbuf(pngPtr))) {
    // An error occurred, so clean up what we have allocated so far.
    png_destroy_read_struct(&pngPtr, &infoPtr, nullptr);
    delete[] rowPtrs;
    XR_LOGE("An error occurred while reading the PNG file.");
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
    if (bitdepth == 16) {
      init(ImageContentBlockSpec(PixelFormat::GREY16, imgWidth, imgHeight));
      png_set_swap(pngPtr);
    } else {
      // for any other possible bitdepth we will convert to 8 bit (see below)
      init(ImageContentBlockSpec(PixelFormat::GREY8, imgWidth, imgHeight));
    }
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
  png_destroy_read_struct(&pngPtr, &infoPtr, nullptr);

  return true;
}

bool PixelFrame::readPngFrame(
    shared_ptr<PixelFrame>& frame,
    RecordReader* reader,
    uint32_t sizeBytes) {
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

int PixelFrame::writeAsPng(const string& filename, vector<uint8_t>* outBuffer) const {
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
