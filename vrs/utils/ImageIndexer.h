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

#pragma once

#include <vrs/utils/FilteredFileReader.h>

namespace vrs::utils {

/// Helper class to reference images in a VRS file, optimized for reading.
struct DirectImageReference {
  DirectImageReference(int64_t dataOffset, uint32_t dataSize, string imageFormat)
      : dataOffset{dataOffset}, dataSize{dataSize}, imageFormat{std::move(imageFormat)} {}

  DirectImageReference(
      int64_t dataOffset,
      uint32_t dataSize,
      string imageFormat,
      CompressionType compressionType,
      uint32_t compressedOffset,
      uint32_t compressedLength)
      : dataOffset{dataOffset},
        dataSize{dataSize},
        imageFormat{std::move(imageFormat)},
        compressionType{compressionType},
        compressedOffset{compressedOffset},
        compressedLength{compressedLength} {}

  void setCompression(
      CompressionType _compressionType,
      uint32_t _compressedOffset,
      uint32_t _compressedLength) {
    this->compressionType = _compressionType;
    this->compressedOffset = _compressedOffset;
    this->compressedLength = _compressedLength;
  }

  bool operator==(const DirectImageReference& other) const {
    return dataOffset == other.dataOffset && dataSize == other.dataSize &&
        imageFormat == other.imageFormat && compressionType == other.compressionType &&
        compressedOffset == other.compressedOffset && compressedLength == other.compressedLength;
  }

  int64_t dataOffset{};
  uint32_t dataSize{};
  string imageFormat;
  CompressionType compressionType{CompressionType::None};
  uint32_t compressedOffset{};
  uint32_t compressedLength{};
};

/// Helper class to reference images in a VRS file, with additional details when indexing.
struct DirectImageReferencePlus : public DirectImageReference {
  DirectImageReferencePlus(
      StreamId streamId,
      uint32_t dataRecordIndex,
      int64_t dataOffset,
      uint32_t dataSize,
      string imageFormat)
      : DirectImageReference(dataOffset, dataSize, std::move(imageFormat)),
        streamId{streamId},
        dataRecordIndex{dataRecordIndex} {}

  DirectImageReferencePlus(
      StreamId streamId,
      uint32_t dataRecordIndex,
      int64_t dataOffset,
      uint32_t dataSize,
      string imageFormat,
      CompressionType compressionType,
      uint32_t compressedOffset,
      uint32_t compressedLength)
      : DirectImageReference(
            dataOffset,
            dataSize,
            std::move(imageFormat),
            compressionType,
            compressedOffset,
            compressedLength),
        streamId{streamId},
        dataRecordIndex{dataRecordIndex} {}

  StreamId streamId;
  uint32_t dataRecordIndex; // data record index within the stream
};

/// Get the list of references for all the images found in a VRS file.
/// @param path: path to the VRS file to index.
/// @param outImages: on exit, a list of image references.
/// @return 0 on success, or an error code.
int indexImages(const string& path, vector<DirectImageReferencePlus>& outImages);

/// Get a list of references for the images found in a VRS file, using a FilteredFileReader
/// that may restrict the streams and time range considered. Useful for a CLI tool.
/// @param reader: an open FilteredFileReader, with or without filters.
/// @param outImages: on exit, a list of image references.
/// @return 0 on success, or an error code.
int indexImages(FilteredFileReader& reader, vector<DirectImageReferencePlus>& outImages);

} // namespace vrs::utils
