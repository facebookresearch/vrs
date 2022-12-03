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

#include "RecordFormatStreamPlayer.h"

#include "RecordFileReader.h"

#define DEFAULT_LOG_CHANNEL "RecordFormatStreamPlayer"
#include <logging/Log.h>

using namespace std;

namespace vrs {

bool RecordFormatStreamPlayer::onUnsupportedBlock(
    const CurrentRecord& record,
    size_t /*blockIndex*/,
    const ContentBlock& contentBlock) {
  // read past the block, if we know its size
  size_t blockSize = contentBlock.getBlockSize();
  if (blockSize != ContentBlock::kSizeUnknown) {
    vector<uint8_t> data(blockSize);
    record.reader->read(data);
    return true;
  }
  return false;
}

void RecordFormatStreamPlayer::onAttachedToFileReader(RecordFileReader& fileReader, StreamId id) {
  recordFileReader_ = &fileReader;
  RecordFormatMap recordFormats;
  fileReader.getRecordFormats(id, recordFormats);
  for (auto& formats : recordFormats) {
    readers_[tuple<StreamId, Record::Type, uint32_t>(id, formats.first.first, formats.first.second)]
        .recordFormat = formats.second;
  }
}

bool RecordFormatStreamPlayer::processRecordHeader(
    const CurrentRecord& record,
    DataReference& ref) {
  const auto& decoders = readers_.find(tuple<StreamId, Record::Type, uint32_t>(
      record.streamId, record.recordType, record.formatVersion));
  if (decoders == readers_.end() || decoders->second.recordFormat.getUsedBlocksCount() == 0) {
    if (record.recordSize > 0) {
      XR_LOGE(
          "RecordFormat missing for {}, Type:{}, FormatVersion:{}",
          record.streamId.getName(),
          toString(record.recordType),
          record.formatVersion);
    }
    currentReader_ = nullptr;
    // no record format, give a chance to "classic" record reading methods
    return StreamPlayer::processRecordHeader(record, ref);
  }
  currentReader_ = &(decoders->second);
  lastReader_[{record.streamId, record.recordType}] = currentReader_;
  return true; // we will do all the reading in processRecord: we don't touch the DataReference
}

void RecordFormatStreamPlayer::processRecord(const CurrentRecord& record, uint32_t readSize) {
  if (currentReader_ == nullptr) {
    // "classic" style reading: delegate back to StreamPlayer's default implementation
    StreamPlayer::processRecord(record, readSize);
    return;
  }
  RecordFormat& recordFormat = currentReader_->recordFormat;
  vector<unique_ptr<ContentBlockReader>>& readers = currentReader_->contentReaders;

  bool keepReading = true;
  size_t usedBlocksCount = recordFormat.getUsedBlocksCount();
  readers.reserve(usedBlocksCount);
  for (size_t blockIndex = 0; blockIndex < usedBlocksCount && keepReading; ++blockIndex) {
    if (readers.size() <= blockIndex) {
      unique_ptr<DataLayout> blockLayout;
      if (recordFormat.getContentBlock(blockIndex).getContentType() == ContentType::DATA_LAYOUT &&
          recordFileReader_ != nullptr) {
        blockLayout = recordFileReader_->getDataLayout(
            record.streamId,
            ContentBlockId(
                record.streamId.getTypeId(), record.recordType, record.formatVersion, blockIndex));
        if (!blockLayout) {
          XR_LOGE(
              "DataLayout missing for {}, Type:{}, FormatVersion:{}, Block #{}",
              record.streamId.getName(),
              toString(record.recordType),
              record.formatVersion,
              blockIndex);
        }
      }
      readers.emplace_back(
          ContentBlockReader::build(recordFormat, blockIndex, std::move(blockLayout)));
    }
    ContentBlockReader* reader = readers[blockIndex].get();
    if (reader == nullptr) {
      keepReading = false;
    } else {
      keepReading = reader->readBlock(record, *this);
    }
  }
  currentReader_->lastReadRecordTimestamp = record.timestamp;
}

RecordFormatReader* RecordFormatStreamPlayer::getLastRecordFormatReader(
    StreamId id,
    Record::Type recordType) const {
  const auto& last = lastReader_.find({id, recordType});
  return last == lastReader_.end() ? nullptr : last->second;
}

} // namespace vrs
