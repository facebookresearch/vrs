// (c) Meta Platforms, Inc. and affiliates. Confidential and proprietary.

#pragma once

#include <vrs/RecordFileReader.h>
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
      imageSpecs[record.streamId] = cb.image();
    }
    return false;
  }
  map<StreamId, ImageContentBlockSpec> imageSpecs;
};

} // namespace test
} // namespace vrs
