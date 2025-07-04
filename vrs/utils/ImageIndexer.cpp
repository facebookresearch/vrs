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

#include <memory>
#include <vector>

#define DEFAULT_LOG_CHANNEL "ImageIndexer"
#include <logging/Log.h>
#include <logging/Verify.h>

#include <vrs/FileFormat.h>
#include <vrs/os/Utils.h>
#include <vrs/utils/FilteredFileReader.h>
#include <vrs/utils/PixelFrame.h>
#include <vrs/utils/VideoRecordFormatStreamPlayer.h>

#include "ImageIndexer.h"

using namespace std;
using namespace vrs;
using namespace vrs::utils;

namespace {

class ImageOffsetWriter : public utils::VideoRecordFormatStreamPlayer {
 public:
  explicit ImageOffsetWriter(
      vector<DirectImageReferencePlus>& images,
      int& counter,
      int& compressed)
      : images_{images}, counter_{counter}, compressed_{compressed} {}

  bool processRecordHeader(const CurrentRecord& record, DataReference& outDataReference) override {
    recordStartOffset_ = record.reader->getFileOffset();
    recordDiskSize_ = record.reader->getUnreadDiskBytes();
    return RecordFormatStreamPlayer::processRecordHeader(record, outDataReference);
  }

  bool onImageRead(const CurrentRecord& r, size_t /* id x*/, const ContentBlock& cb) override {
    if (r.recordType == Record::Type::DATA) {
      if (r.reader->getCompressionType() == CompressionType::None) {
        images_.emplace_back(
            r.streamId,
            dataRecordIndex_,
            r.reader->getFileOffset(),
            cb.getBlockSize(),
            cb.image().asString());
        counter_++;
      } else {
        images_.emplace_back(
            r.streamId,
            dataRecordIndex_,
            recordStartOffset_,
            recordDiskSize_,
            cb.image().asString(),
            r.reader->getCompressionType(),
            r.recordSize - r.reader->getUnreadBytes(),
            cb.getBlockSize());
        counter_++, compressed_++;
      }
      utils::PixelFrame frame;
      XR_VERIFY(readFrame(frame, r, cb));
    }
    return true;
  }

  int recordReadComplete(RecordFileReader& reader, const IndexRecord::RecordInfo& recordInfo)
      override {
    if (recordInfo.recordType == Record::Type::DATA) {
      dataRecordIndex_++;
    }
    return VideoRecordFormatStreamPlayer::recordReadComplete(reader, recordInfo);
  }

 private:
  vector<DirectImageReferencePlus>& images_;
  int& counter_;
  int& compressed_;
  uint32_t dataRecordIndex_{0};
  int64_t recordStartOffset_{-1};
  uint32_t recordDiskSize_{0};
};

} // namespace

namespace vrs::utils {

int indexImages(FilteredFileReader& reader, vector<DirectImageReferencePlus>& outImages) {
  outImages.clear();

  int uncompressed = 0;
  int compressed = 0;

  vector<unique_ptr<StreamPlayer>> streamPlayers;
  bool hasImages = false;
  size_t maxCounter = 0;
  map<StreamId, size_t> imageOffsets;
  for (auto id : reader.filter.streams) {
    if (reader.reader.mightContainImages(id)) {
      XR_LOGI("Found {} - {}...", id.getNumericName(), id.getTypeName());
      auto player = make_unique<ImageOffsetWriter>(outImages, uncompressed, compressed);
      reader.reader.setStreamPlayer(id, player.get());
      streamPlayers.emplace_back(std::move(player));
      maxCounter += reader.reader.getRecordCount(id, Record::Type::DATA);
      hasImages = true;
    }
  }
  if (!hasImages) {
    XR_LOGW("No image stream found in the file");
    return 0;
  }
  outImages.reserve(maxCounter);
  reader.iterateSafe();

  if (compressed == 0) {
    XR_LOGI("Found {} frames, none compressed!", uncompressed);
  } else {
    XR_LOGI("Found {} frames, {} compressed!", uncompressed, compressed);
  }

  return 0;
}

int indexImages(const string& path, vector<DirectImageReferencePlus>& outImages) {
  FilteredFileReader reader;
  int status = reader.setSource(path);
  if (status != 0 || (status = reader.openFile()) != 0) {
    return status;
  }
  return indexImages(reader, outImages);
}

} // namespace vrs::utils
