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

#include <memory>
#include <vector>

#include <vrs/helpers/JobQueue.h>
#include <vrs/utils/PixelFrame.h>
#include <vrs/utils/VideoRecordFormatStreamPlayer.h>

namespace vrs {
namespace utils {

class ImageExtractor;

/// Optional helper class so ImageExtractor's image naming can be customized
class ImageNamer {
 public:
  virtual ~ImageNamer() = default;

  /// Before reading any record, after the file is open, use this callback to know what is read.
  virtual void init(RecordFileReader& reader) {}

  /// For each record in image streams, get their datalayouts.
  /// Use extractor.getExpectedLayout<T>() as needed.
  virtual bool onDataLayoutRead(
      const CurrentRecord& /*record*/,
      size_t /*blockIndex*/,
      DataLayout& /*datalayout*/,
      ImageExtractor& /*extractor*/) {
    return true;
  }

  /// Name image files saved in png format (default)
  virtual string namePngImage(StreamId id, uint32_t imageCounter, double timestamp);

  /// Name image files saved in raw format
  virtual string nameRawImage(
      const ImageContentBlockSpec& imageSpec,
      StreamId id,
      uint32_t imageCounter,
      double timestamp);

 protected:
  static string getRawImageFormatAsString(const ImageContentBlockSpec& imageSpec);
};

class ImageExtractor : public utils::VideoRecordFormatStreamPlayer {
 public:
  ImageExtractor(const string& folderPath, uint32_t& counter, bool extractImagesRaw);
  ImageExtractor(
      ImageNamer& imageNamer,
      const string& folderPath,
      uint32_t& counter,
      bool extractImagesRaw);
  ~ImageExtractor() override;

  bool onDataLayoutRead(const CurrentRecord& r, size_t idx, DataLayout& dl) override;
  bool onImageRead(const CurrentRecord& record, size_t idx, const ContentBlock& ib) override;
  bool onUnsupportedBlock(const CurrentRecord& record, size_t idx, const ContentBlock& cb) override;

  void saveImagesThreadActivity();

  template <class T>
  inline T& getExpectedLayout(DataLayout& layout, size_t blockIndex) {
    return RecordFormatStreamPlayer::getExpectedLayout<T>(layout, blockIndex);
  }

 protected:
  ImageNamer& imageNamer_;
  const string& folderPath_;
  uint32_t& imageFileCounter_;
  uint32_t imageCounter_ = 0;
  const bool extractImagesRaw_;
};

} // namespace utils
} // namespace vrs
