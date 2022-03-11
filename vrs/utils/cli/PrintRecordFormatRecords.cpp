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

#include "PrintRecordFormatRecords.h"

#include <iostream>

#include <vrs/RecordFormatStreamPlayer.h>

using namespace std;
using namespace vrs;
using namespace vrs::utils;

namespace {

class DataLayoutPrinter : public RecordFormatStreamPlayer {
 public:
  explicit DataLayoutPrinter(PrintoutType type) : printoutType_{type} {}
  bool processRecordHeader(const CurrentRecord& record, DataReference& outDataReference) override {
    if (printoutType_ == PrintoutType::Compact || printoutType_ == PrintoutType::Details) {
      const auto& decoder = readers_.find(tuple<StreamId, Record::Type, uint32_t>(
          record.streamId, record.recordType, record.formatVersion));
      string recordFormat;
      if (decoder != readers_.end()) {
        recordFormat = decoder->second.recordFormat.asString();
      } else {
        recordFormat = "<undefined>";
      }
      printf(
          "%.03f %s [%s], %s record, %s = %d bytes total.\n",
          record.timestamp,
          record.streamId.getName().c_str(),
          record.streamId.getNumericName().c_str(),
          Record::typeName(record.recordType),
          recordFormat.c_str(),
          record.recordSize);
    } else {
      printf(
          "{ \"record\": { \"timestamp\": %.03f, \"device\": \"%d-%hu\", \"type\": \"%s\", \"size\": %d } }\n",
          record.timestamp,
          static_cast<int>(record.streamId.getTypeId()),
          record.streamId.getInstanceId(),
          Record::typeName(record.recordType),
          record.recordSize);
    }
    return RecordFormatStreamPlayer::processRecordHeader(record, outDataReference);
  }

  bool onDataLayoutRead(const CurrentRecord& r, size_t blkIdx, DataLayout& datalayout) override {
    switch (printoutType_) {
      case PrintoutType::Details:
        cout << " - DataLayout:" << endl;
        datalayout.printLayout(cout, "   ");
        break;

      case PrintoutType::Compact:
        cout << " - DataLayout:" << endl;
        datalayout.printLayoutCompact(cout, "   ");
        break;

      case PrintoutType::JsonCompact:
        cout << datalayout.asJson(JsonFormatProfile::ExternalCompact) << endl;
        break;

      case PrintoutType::JsonPretty:
        cout << datalayout.asJson(JsonFormatProfile::ExternalPretty) << endl;
        break;

      case PrintoutType::None:
        break;
    }
    return true; // read next blocks, if any
  }
  bool onImageRead(const CurrentRecord& record, size_t blkIdx, const ContentBlock& cb) override {
    if (printoutType_ == PrintoutType::Compact || printoutType_ == PrintoutType::Details) {
      cout << " - Image block, " << cb.asString() << ", " << cb.getBlockSize() << " bytes." << endl;
    } else {
      printf("{ \"image\": \"%s\" }\n", cb.asString().c_str());
    }
    return readContentBlockData(record, cb);
  }
  bool onAudioRead(const CurrentRecord& record, size_t blkIdx, const ContentBlock& cb) override {
    if (printoutType_ == PrintoutType::Compact || printoutType_ == PrintoutType::Details) {
      cout << " - Audio block, " << cb.asString() << ", " << cb.getBlockSize() << " bytes." << endl;
    } else {
      printf("{ \"audio\": \"%s\" }\n", cb.asString().c_str());
    }
    return readContentBlockData(record, cb);
  }
  bool onCustomBlockRead(const CurrentRecord& r, size_t blkIdx, const ContentBlock& cb) override {
    if (printoutType_ == PrintoutType::Compact || printoutType_ == PrintoutType::Details) {
      cout << " - Custom block, " << cb.asString() << ", " << cb.getBlockSize() << " bytes."
           << endl;
    } else {
      printf("{ \"custom\": \"%s\" }\n", cb.asString().c_str());
    }
    return readContentBlockData(r, cb);
  }
  bool onUnsupportedBlock(const CurrentRecord& r, size_t blkIdx, const ContentBlock& cb) override {
    if (printoutType_ == PrintoutType::Compact || printoutType_ == PrintoutType::Details) {
      cout << " - Unsupported block, " << cb.asString();
      if (cb.getBlockSize() == ContentBlock::kSizeUnknown) {
        cout << ", size unknown." << endl;
      } else {
        cout << ", " << cb.getBlockSize() << " bytes." << endl;
      }
    } else {
      printf("{ \"unsupported\": \"%s\" }\n", cb.asString().c_str());
    }
    return readContentBlockData(r, cb);
  }
  bool readContentBlockData(const CurrentRecord& record, const ContentBlock& contentBlock) {
    size_t blockSize = contentBlock.getBlockSize();
    if (blockSize == ContentBlock::kSizeUnknown) {
      cerr << "  *** Content block size unknown! ***" << endl;
      return false;
    }
    vector<char> data(blockSize);
    int readStatus = record.reader->read(data);
    if (readStatus == 0) {
      return true; // read next blocks, if any
    }
    cerr << "  *** Failed to read content: " << errorCodeToMessage(readStatus) << " ***" << endl;
    return false;
  }

 private:
  PrintoutType printoutType_;
};

} // namespace

namespace vrs::utils {

void printRecordFormatRecords(FilteredFileReader& filteredReader, PrintoutType type) {
  DataLayoutPrinter lister(type);
  for (auto id : filteredReader.filter.streams) {
    filteredReader.reader.setStreamPlayer(id, &lister);
  }
  filteredReader.iterateSafe();
}

} // namespace vrs::utils
