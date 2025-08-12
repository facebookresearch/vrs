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

#include <vrs/RecordFileReader.h>
#include <vrs/utils/PixelFrame.h>
#include <vrs/utils/VideoRecordFormatStreamPlayer.h>

namespace vrs {
namespace test {

class ImageSpecCollector : public utils::VideoRecordFormatStreamPlayer {
 public:
  explicit ImageSpecCollector(RecordFileReader& reader) {
    for (auto id : reader.getStreams()) {
      reader.setStreamPlayer(id, this);
      auto config = reader.getRecord(id, Record::Type::CONFIGURATION, 0);
      if (config != nullptr) {
        reader.readRecord(*config);
      }
      auto data = reader.getRecord(id, Record::Type::DATA, 0);
      if (data != nullptr) {
        reader.readRecord(*data);
      }
    }
  }
  bool onImageRead(const CurrentRecord& record, size_t blockIndex, const ContentBlock& cb)
      override {
    if (record.recordType == Record::Type::DATA) {
      // Collect original content block image spec
      imageSpecs[record.streamId] = cb.image();

      // Decode the image and collect the decoded image spec
      utils::PixelFrame frame;
      if (frame.readFrame(record.reader, cb)) {
        decodedImageSpecs[record.streamId] = frame.getSpec();
      } else {
        // If decoding fails, use the original spec as fallback
        decodedImageSpecs[record.streamId] = cb.image();
      }
    }
    return false;
  }

  // Original image specs from content block metadata
  map<StreamId, ImageContentBlockSpec> imageSpecs;

  // Decoded image specs from actual decoded image data
  map<StreamId, ImageContentBlockSpec> decodedImageSpecs;
};

} // namespace test
} // namespace vrs
