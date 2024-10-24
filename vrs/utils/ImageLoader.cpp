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

#define DEFAULT_LOG_CHANNEL "ImageLoader"
#include <logging/Log.h>

#include <vrs/Compressor.h>
#include <vrs/RecordReaders.h>
#include <vrs/os/Utils.h>
#include <vrs/utils/BufferRecordReader.hpp>
#include <vrs/utils/PixelFrame.h>
#include <vrs/utils/VideoRecordFormatStreamPlayer.h>

#include "ImageLoader.h"

using namespace std;

namespace vrs::utils {

bool loadImage(
    const void* data,
    size_t length,
    PixelFrame& outFrame,
    const DirectImageReference& imageRef,
    ImageLoadType loadType) {
  BufferFileHandler file(data, length);
  DirectImageReference zeroImageRef = imageRef;
  zeroImageRef.dataOffset = 0;
  return loadImage(file, outFrame, zeroImageRef, loadType);
}

bool loadImage(
    FileHandler& file,
    PixelFrame& outFrame,
    const DirectImageReference& imageRef,
    ImageLoadType loadType) {
  int64_t fileSize = file.getTotalSize();
  if (imageRef.dataOffset < 0 || imageRef.dataOffset >= fileSize) {
    XR_LOGE("Invalid location offset: {} (file size: {})", imageRef.dataOffset, fileSize);
    return false;
  }
  if (imageRef.dataOffset + imageRef.dataSize > fileSize) {
    XR_LOGE("Invalid location length: {} (file size: {})", imageRef.dataSize, fileSize);
    return false;
  }

  if (imageRef.compressionType != CompressionType::None) {
    if (imageRef.compressedOffset >= imageRef.dataSize) {
      XR_LOGE(
          "Invalid compressed offset: {} (data size: {})",
          imageRef.compressedOffset,
          imageRef.dataSize);
      return false;
    }
  }

  file.setPos(imageRef.dataOffset);
  uint32_t uncompressedDataSize = 0;
  RecordReader* reader = nullptr;
  UncompressedRecordReader uncompressedRecordReader;
  CompressedRecordReader compressedRecordReader;
  switch (imageRef.compressionType) {
    case CompressionType::None:
      uncompressedDataSize = imageRef.dataSize;
      reader = uncompressedRecordReader.init(file, imageRef.dataSize, imageRef.dataSize);
      break;
    case CompressionType::Lz4:
    case CompressionType::Zstd: {
      uncompressedDataSize = imageRef.compressedLength;
      reader = compressedRecordReader.init(
          file, imageRef.dataSize, imageRef.compressedOffset + imageRef.compressedLength);
      compressedRecordReader.initCompressionType(imageRef.compressionType);
      if (imageRef.compressedOffset > 0) {
        uint32_t readSize = imageRef.compressedOffset;
        vector<uint8_t> compressedBuffer(readSize);
        DataReference dataReference(compressedBuffer);
        if (compressedRecordReader.read(dataReference, readSize) != 0) {
          XR_LOGE("Failed to read compressed offset data");
          return false;
        }
      }
    } break;
    default:
      XR_LOGE("Can interpret compressed data.");
      return false;
  }

  ImageContentBlockSpec spec(imageRef.imageFormat);
  size_t imageSpecSize = spec.getRawImageSize();
  if (imageSpecSize != ContentBlock::kSizeUnknown && imageSpecSize != uncompressedDataSize) {
    XR_LOGE(
        "Image size mismatch: {} => {} vs {}",
        imageRef.imageFormat,
        imageSpecSize,
        uncompressedDataSize);
    return false;
  }

  ContentBlock contentBlock(spec, uncompressedDataSize);
  if (loadType == ImageLoadType::Raw) {
    if (!outFrame.readDiskImageData(reader, contentBlock)) {
      XR_LOGE("Failed to read image data");
      return false;
    }
  } else {
    auto frame = make_shared<PixelFrame>();
    if (!frame->readFrame(reader, contentBlock)) {
      XR_LOGE("Failed to read and decode image data");
      return false;
    }
    if (loadType == ImageLoadType::Normalize8 || loadType == ImageLoadType::Normalize16) {
      shared_ptr<PixelFrame> normalizedFrame;
      PixelFrame::normalizeFrame(frame, normalizedFrame, loadType == ImageLoadType::Normalize16);
      frame = normalizedFrame;
    }
    outFrame.init(frame->getSpec(), std::move(frame->getBuffer()));
  }

  return true;
}

} // namespace vrs::utils
