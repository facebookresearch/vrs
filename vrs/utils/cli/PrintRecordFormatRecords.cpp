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

#include "PrintRecordFormatRecords.h"

#include <fmt/format.h>

#include <vrs/RecordFormatStreamPlayer.h>

using namespace std;
using namespace vrs;
using namespace vrs::utils;

namespace {

class DataLayoutPrinter : public RecordFormatStreamPlayer {
 public:
  explicit DataLayoutPrinter(PrintoutType type) : printoutType_{type} {}
  bool processRecordHeader(const CurrentRecord& record, DataReference& outDataReference) override {
    if (printing_) {
      recordCount_++;
      if (printoutType_ == PrintoutType::Compact || printoutType_ == PrintoutType::Details) {
        const auto& decoder = readers_.find(tuple<StreamId, Record::Type, uint32_t>(
            record.streamId, record.recordType, record.formatVersion));
        string recordFormat;
        if (decoder != readers_.end()) {
          recordFormat = decoder->second.recordFormat.asString();
        } else {
          recordFormat = "<no RecordFormat definition>";
        }
        fmt::print(
            "{:.3f} {} [{}], {} record, {} = {} bytes total.\n",
            record.timestamp,
            record.streamId.getName(),
            record.streamId.getNumericName(),
            Record::typeName(record.recordType),
            recordFormat,
            record.recordSize);
      } else if (printoutType_ != PrintoutType::None) {
        fmt::print(
            "{{\"record\":{{\"timestamp\":{:.3f},\"device\":\"{}\",\"type\":\"{}\",\"size\":{}}}}}\n",
            record.timestamp,
            record.streamId.getNumericName(),
            Record::typeName(record.recordType),
            record.recordSize);
      }
    }
    return RecordFormatStreamPlayer::processRecordHeader(record, outDataReference);
  }

  bool onDataLayoutRead(const CurrentRecord& r, size_t blkIdx, DataLayout& datalayout) override {
    if (printing_) {
      datalayoutCount_++;
      switch (printoutType_) {
        case PrintoutType::Details:
          fmt::print(" - DataLayout:\n");
          datalayout.printLayout(cout, "   ");
          break;

        case PrintoutType::Compact:
          fmt::print(" - DataLayout:\n");
          datalayout.printLayoutCompact(cout, "   ");
          break;

        case PrintoutType::JsonCompact:
          fmt::print("{}\n", datalayout.asJson(JsonFormatProfile::ExternalCompact));
          break;

        case PrintoutType::JsonPretty:
          fmt::print("{}\n", datalayout.asJson(JsonFormatProfile::ExternalPretty));
          break;

        case PrintoutType::None:
          break;
      }
    }
    return true; // read next blocks, if any
  }
  bool onImageRead(const CurrentRecord& record, size_t blkIdx, const ContentBlock& cb) override {
    if (printing_) {
      imageCount_++;
      if (printoutType_ == PrintoutType::Compact || printoutType_ == PrintoutType::Details) {
        fmt::print(" - Image block, {}, {} bytes.\n", cb.asString(), cb.getBlockSize());
      } else if (printoutType_ != PrintoutType::None) {
        fmt::print("{{\"image\":\"{}\"}}\n", cb.asString());
      }
    }
    return readContentBlockData(record, cb);
  }
  bool onAudioRead(const CurrentRecord& record, size_t blkIdx, const ContentBlock& cb) override {
    if (printing_) {
      audioCount_++;
      if (printoutType_ == PrintoutType::Compact || printoutType_ == PrintoutType::Details) {
        fmt::print(" - Audio block, {}, {} bytes.\n", cb.asString(), cb.getBlockSize());
      } else if (printoutType_ != PrintoutType::None) {
        fmt::print("{{\"audio\":\"{}\"}}\n", cb.asString());
      }
    }
    return readContentBlockData(record, cb);
  }
  bool onCustomBlockRead(const CurrentRecord& r, size_t blkIdx, const ContentBlock& cb) override {
    if (printing_) {
      customCount_++;
      if (printoutType_ == PrintoutType::Compact || printoutType_ == PrintoutType::Details) {
        fmt::print(" - Custom block, {}, {} bytes.\n", cb.asString(), cb.getBlockSize());
      } else if (printoutType_ != PrintoutType::None) {
        fmt::print("{{\"custom\":\"{}\"}}\n", cb.asString());
      }
    }
    return readContentBlockData(r, cb);
  }
  bool onUnsupportedBlock(const CurrentRecord& r, size_t blkIdx, const ContentBlock& cb) override {
    if (printing_) {
      unsupportedCount_++;
      if (printoutType_ == PrintoutType::Compact || printoutType_ == PrintoutType::Details) {
        fmt::print(" - Unsupported block, {}", cb.asString());
        if (cb.getBlockSize() == ContentBlock::kSizeUnknown) {
          fmt::print(", size unknown.\n");
        } else {
          fmt::print(", {} bytes.\n", cb.getBlockSize());
        }
      } else if (printoutType_ != PrintoutType::None) {
        fmt::print("{{\"unsupported\":\"{}\"}}\n", cb.asString());
      }
    }
    return readContentBlockData(r, cb);
  }
  bool readContentBlockData(const CurrentRecord& record, const ContentBlock& contentBlock) {
    size_t blockSize = contentBlock.getBlockSize();
    if (blockSize == ContentBlock::kSizeUnknown) {
      fmt::print(stderr, "  *** Content block size unknown! ***\n");
      return false;
    }
    vector<char> data(blockSize);
    int readStatus = record.reader->read(data);
    if (readStatus == 0) {
      return true; // read next blocks, if any
    }
    fmt::print(stderr, "  *** Failed to read content: {} ***\n", errorCodeToMessage(readStatus));
    return false;
  }
  void enablePrinting() {
    printing_ = true;
  }
  void printSummary() const {
    fmt::print("Decoded {} records", recordCount_);
    if (datalayoutCount_ != 0) {
      fmt::print(", {} datalayouts", datalayoutCount_);
    }
    if (imageCount_ != 0) {
      fmt::print(", {} images", imageCount_);
    }
    if (audioCount_ != 0) {
      fmt::print(", {} audio content blocks", audioCount_);
    }
    if (customCount_ != 0) {
      fmt::print(", {} custom content blocks", customCount_);
    }
    if (unsupportedCount_ != 0) {
      fmt::print(", {} unsupported content blocks", unsupportedCount_);
    }
    fmt::print(".\n");
  }

 private:
  PrintoutType printoutType_;
  bool printing_{false};
  uint32_t recordCount_{0};
  uint32_t datalayoutCount_{0};
  uint32_t imageCount_{0};
  uint32_t audioCount_{0};
  uint32_t customCount_{0};
  uint32_t unsupportedCount_{0};
};

} // namespace

namespace vrs::utils {

void printRecordFormatRecords(FilteredFileReader& filteredReader, PrintoutType type) {
  DataLayoutPrinter lister(type);
  for (auto id : filteredReader.filter.streams) {
    filteredReader.reader.setStreamPlayer(id, &lister);
  }
  double startTimestamp, endTimestamp;
  filteredReader.getConstrainedTimeRange(startTimestamp, endTimestamp);
  // Required to Load RecordFormat & DataLayout definitions from filtered-out configuration records!
  filteredReader.preRollConfigAndState();
  // do not print records during pre-roll!
  lister.enablePrinting();
  filteredReader.iterateAdvanced();
  if (type == PrintoutType::None) {
    lister.printSummary();
  }
}

} // namespace vrs::utils
