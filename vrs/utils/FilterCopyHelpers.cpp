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

#include "FilterCopyHelpers.h"

#define DEFAULT_LOG_CHANNEL "FilterCopyHelpers"
#include <logging/Log.h>

#include <vrs/RecordFileReader.h>
#include <vrs/RecordFileWriter.h>

using namespace std;
using namespace vrs;

namespace vrs::utils {

const Record* Writer::createStateRecord() {
  return nullptr;
}

const Record* Writer::createConfigurationRecord() {
  return nullptr;
}

const Record* Writer::createRecord(const CurrentRecord& record, vector<int8_t>& data) {
  return Recordable::createRecord(
      record.timestamp, record.recordType, record.formatVersion, DataSource(data));
}

const Record* Writer::createRecord(const CurrentRecord& record, DataSource& source) {
  return Recordable::createRecord(
      record.timestamp, record.recordType, record.formatVersion, source);
}

const Record*
Writer::createRecord(double timestamp, Record::Type type, uint32_t formatVersion, DataSource& src) {
  return Recordable::createRecord(timestamp, type, formatVersion, src);
}

CopyOptions::CopyOptions(const CopyOptions& rhs)
    : compressionPoolSize{rhs.compressionPoolSize},
      showProgress{rhs.showProgress},
      graceWindow{rhs.graceWindow},
      jsonOutput{rhs.jsonOutput},
      maxChunkSizeMB{rhs.maxChunkSizeMB},
      mergeStreams{rhs.mergeStreams} {
  if (rhs.tagOverrider) {
    TagOverrider& thisTagOverrider = getTagOverrider();
    thisTagOverrider.fileTags = rhs.tagOverrider->fileTags;
    thisTagOverrider.streamTags = rhs.tagOverrider->streamTags;
  }
}

void TagOverrider::overrideTags(RecordFileWriter& writer) const {
  writer.addTags(fileTags);
  if (!streamTags.empty()) {
    for (Recordable* recordable : writer.getRecordables()) {
      auto iter = streamTags.find(recordable->getStreamId());
      if (iter != streamTags.end()) {
        recordable->addTags(iter->second);
      }
    }
  }
}

TagOverrider& CopyOptions::getTagOverrider() {
  if (!tagOverrider) {
    tagOverrider = make_unique<TagOverrider>();
  }
  return *tagOverrider;
}

Copier::Copier(
    RecordFileReader& fileReader,
    RecordFileWriter& fileWriter,
    StreamId id,
    const CopyOptions& copyOptions)
    : writer_(id.getTypeId(), fileReader.getFlavor(id)),
      fileWriter_{fileWriter},
      options_{copyOptions} {
  fileReader.setStreamPlayer(id, this);
  fileWriter.addRecordable(&writer_);
  // copy the tags of that stream
  writer_.addTags(fileReader.getTags(id));
  writer_.setCompression(options_.getCompression());
}

bool Copier::processRecordHeader(const CurrentRecord& record, DataReference& outDataRef) {
  rawRecordData_.resize(record.recordSize);
  outDataRef.useRawData(rawRecordData_.data(), record.recordSize);
  return true;
}

void Copier::processRecord(const CurrentRecord& record, uint32_t /*bytesWrittenCount*/) {
  writer_.createRecord(record, rawRecordData_);
  ++options_.outRecordCopiedCount;
}

ContentChunk::ContentChunk(DataLayout& layout) {
  buffer_.resize(layout.getFixedData().size() + layout.getVarData().size());
  uint8_t* buffer = buffer_.data();
  DataSourceChunk fixedSizeChunk(layout.getFixedData());
  fixedSizeChunk.fillAndAdvanceBuffer(buffer);
  DataSourceChunk varSizeChunk(layout.getVarData());
  varSizeChunk.fillAndAdvanceBuffer(buffer);
}

void ContentChunk::fillAndAdvanceBuffer(uint8_t*& buffer) const {
  DataSourceChunk dataSourceChunk(buffer_);
  dataSourceChunk.fillAndAdvanceBuffer(buffer);
}

ContentBlockChunk::ContentBlockChunk(const ContentBlock& contentBlock, const CurrentRecord& record)
    : ContentChunk(contentBlock.getBlockSize()), contentBlock_{contentBlock} {
  int status = record.reader->read(getBuffer());
  if (status != 0) {
    XR_LOGW("Failed to read image block: {}", errorCodeToMessage(status));
  }
}

ContentBlockChunk::ContentBlockChunk(const ContentBlock& contentBlock, vector<uint8_t>&& buffer)
    : ContentChunk(move(buffer)), contentBlock_{contentBlock} {}

void FilteredChunksSource::copyTo(uint8_t* buffer) const {
  for (auto& chunk : chunks_) {
    chunk->fillAndAdvanceBuffer(buffer);
  }
}

size_t FilteredChunksSource::getFilteredChunksSize(const deque<unique_ptr<ContentChunk>>& chunks) {
  size_t total = 0;
  for (const auto& chunk : chunks) {
    total += chunk->filterBuffer();
  }
  return total;
}

RecordFilterCopier::RecordFilterCopier(
    RecordFileReader& fileReader,
    RecordFileWriter& fileWriter,
    StreamId id,
    const CopyOptions& copyOptions)
    : writer_(id.getTypeId(), fileReader.getFlavor(id)),
      fileWriter_{fileWriter},
      options_{copyOptions} {
  fileReader.setStreamPlayer(id, this);
  fileWriter.addRecordable(&writer_);
  // copy the tags of that stream
  writer_.addTags(fileReader.getTags(id));
  writer_.setCompression(options_.getCompression());
}

bool RecordFilterCopier::processRecordHeader(const CurrentRecord& rec, DataReference& outDataRef) {
  copyVerbatim_ = (rec.recordSize == 0) || shouldCopyVerbatim(rec);
  skipRecord_ = false;
  if (copyVerbatim_) {
    verbatimRecordData_.resize(rec.recordSize);
    outDataRef.useRawData(verbatimRecordData_.data(), rec.recordSize);
    return true;
  } else {
    return RecordFormatStreamPlayer::processRecordHeader(rec, outDataRef);
  }
}

void RecordFilterCopier::processRecord(const CurrentRecord& record, uint32_t readSize) {
  if (!copyVerbatim_) {
    // Read all the parts, which will result in multiple onXXXRead() callbacks
    chunks_.clear();
    RecordFormatStreamPlayer::processRecord(record, readSize);
  }
  finishRecordProcessing(record);
  ++options_.outRecordCopiedCount;
}

void RecordFilterCopier::finishRecordProcessing(const CurrentRecord& record) {
  if (!skipRecord_) {
    if (copyVerbatim_) {
      writer_.createRecord(record, verbatimRecordData_);
    } else {
      // filter & flush the collected data, in the order collected
      FilteredChunksSource chunkedSource(chunks_);
      CurrentRecord modifiedHeader(record);
      doHeaderEdits(modifiedHeader);
      writer_.createRecord(modifiedHeader, chunkedSource);
    }
  }
}

bool RecordFilterCopier::onDataLayoutRead(const CurrentRecord& rec, size_t index, DataLayout& dl) {
  vector<int8_t> fixedData = dl.getFixedData();
  vector<int8_t> varData = dl.getVarData();
  dl.stageCurrentValues();
  doDataLayoutEdits(rec, index, dl);
  pushDataLayout(dl);
  // restore datalayout state, so decoding the file isn't affected
  fixedData.swap(dl.getFixedData());
  varData.swap(dl.getVarData());
  return true;
}

bool RecordFilterCopier::onImageRead(const CurrentRecord& rec, size_t idx, const ContentBlock& cb) {
  size_t blockSize = cb.getBlockSize();
  if (blockSize == ContentBlock::kSizeUnknown) {
    return onUnsupportedBlock(rec, idx, cb);
  }
  unique_ptr<ContentBlockChunk> imageChunk = make_unique<ContentBlockChunk>(cb, rec);
  filterImage(rec, idx, cb, imageChunk->getBuffer());
  chunks_.emplace_back(move(imageChunk));
  return true;
}

bool RecordFilterCopier::onAudioRead(const CurrentRecord& rec, size_t idx, const ContentBlock& cd) {
  size_t blockSize = cd.getBlockSize();
  if (blockSize == ContentBlock::kSizeUnknown) {
    return onUnsupportedBlock(rec, idx, cd);
  }
  unique_ptr<ContentBlockChunk> audioChunk = make_unique<ContentBlockChunk>(cd, rec);
  filterAudio(rec, idx, cd, audioChunk->getBuffer());
  chunks_.emplace_back(move(audioChunk));
  return true;
}

bool RecordFilterCopier::onUnsupportedBlock(
    const CurrentRecord& record,
    size_t idx,
    const ContentBlock& cb) {
  bool readNext = true;
  size_t blockSize = cb.getBlockSize();
  if (blockSize == ContentBlock::kSizeUnknown) {
    // just read everything left, without trying to analyse content
    blockSize = record.reader->getUnreadBytes();
    readNext = false;
  }
  unique_ptr<ContentChunk> bufferSourceChunk = make_unique<ContentChunk>(blockSize);
  int status = record.reader->read(bufferSourceChunk->getBuffer());
  if (status != 0) {
    XR_LOGW("Failed to read {} block: {}", cb.asString(), errorCodeToMessage(status));
  }
  chunks_.emplace_back(move(bufferSourceChunk));
  return readNext;
}

void RecordFilterCopier::pushDataLayout(DataLayout& datalayout) {
  datalayout.collectVariableDataAndUpdateIndex();
  chunks_.emplace_back(make_unique<ContentChunk>(datalayout));
}

} // namespace vrs::utils
