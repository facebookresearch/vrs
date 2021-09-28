// Copyright (c) Facebook, Inc. and its affiliates. Confidential and proprietary.

#include "AsyncImageFilter.h"

#include <cmath>

#define DEFAULT_LOG_CHANNEL "AsyncImageFilter"
#include <logging/Log.h>
#include <logging/Verify.h>

#include "CopyHelpers.h"

using namespace std;
using namespace vrs;

namespace vrs::utils {

class AsyncRecordFilterCopier : public RecordFilterCopier {
 public:
  AsyncRecordFilterCopier(AsyncImageFilter& asyncImageFilter, vrs::StreamId id)
      : RecordFilterCopier(
            asyncImageFilter.getFilteredReader().reader,
            asyncImageFilter.getWriter(),
            id,
            asyncImageFilter.getCopyOptions()),
        asyncImageFilter_{asyncImageFilter} {}
  bool shouldCopyVerbatim(const CurrentRecord& /*record*/) override {
    imageChunk_ = nullptr;
    return false;
  }
  bool onImageRead(const CurrentRecord& record, size_t index, const ContentBlock& cb) override {
    size_t blockSize = cb.getBlockSize();
    if (blockSize == ContentBlock::kSizeUnknown) {
      return onUnsupportedBlock(record, index, cb);
    }
    unique_ptr<ContentBlockChunk> imageChunk = make_unique<ContentBlockChunk>(cb, record);
    imageChunk_ = imageChunk.get();
    chunks_.emplace_back(move(imageChunk));
    return true;
  }
  void finishRecordProcessing(const CurrentRecord& record) override {
    if (!skipRecord_) {
      if (copyVerbatim_) {
        writer_.createRecord(record, verbatimRecordData_);
      } else {
        if (imageChunk_ != nullptr) {
          asyncImageFilter_.processChunkedRecord(writer_, record, chunks_, imageChunk_);
        } else {
          FilteredChunksSource chunkedSource(chunks_);
          writer_.createRecord(record, chunkedSource);
        }
      }
    }
  }

 protected:
  AsyncImageFilter& asyncImageFilter_;
  ContentBlockChunk* imageChunk_;
};

AsyncImageFilter::AsyncImageFilter(FilteredVRSFileReader& filteredReader)
    : filteredReader_{filteredReader}, throttledWriter_{copyOptions_} {}

int AsyncImageFilter::createOutputFile(
    const string& outputFilePath,
    std::unique_ptr<UploadMetadata>&& uploadMetadata) {
  if (!filteredReader_.reader.isOpened()) {
    int status = filteredReader_.openFile();
    if (status != 0) {
      XR_LOGE("Can't open input file, error #{}: {}", status, errorCodeToMessage(status));
      return status;
    }
  }
  RecordFileWriter& writer = throttledWriter_.getWriter();
  writer.addTags(filteredReader_.reader.getTags());
  for (auto id : filteredReader_.filter.streams) {
    unique_ptr<StreamPlayer> streamPlayer;
    // Use ImageFilter if we find at least one image block defined
    RecordFormatMap formats;
    if (filteredReader_.reader.getRecordFormats(id, formats) > 0) {
      for (const auto& format : formats) {
        if (format.second.getBlocksOfTypeCount(ContentType::IMAGE) > 0) {
          streamPlayer = make_unique<AsyncRecordFilterCopier>(*this, id);
          break;
        }
      }
    }
    if (streamPlayer) {
      copiers_.emplace_back(move(streamPlayer));
    } else {
      copiers_.emplace_back(make_unique<Copier>(filteredReader_.reader, writer, id, copyOptions_));
    }
  }

  double startTimestamp, endTimestamp;
  filteredReader_.getConstrainedTimeRange(startTimestamp, endTimestamp);
  if (!uploadMetadata) {
    writer.preallocateIndex(filteredReader_.buildIndex());
  }
  fileHelper_ = make_unique<ThrottledFileHelper>(throttledWriter_);
  int result = fileHelper_->createFile(outputFilePath, uploadMetadata);
  if (result == 0) {
    filteredReader_.preRollConfigAndState();
    throttledWriter_.initTimeRange(startTimestamp, endTimestamp);
    function<bool(RecordFileReader&, const IndexRecord::RecordInfo&)> f =
        [this](RecordFileReader& reader, const IndexRecord::RecordInfo& record) {
          records_.emplace_back(&record);
          return true;
        };
    filteredReader_.iterate(f);
  } else {
    XR_LOGE("Can't create output file, error #{}: {}", result, errorCodeToMessage(result));
  }
  nextRecordIndex_ = 0;
  return result;
}

const IndexRecord::RecordInfo* AsyncImageFilter::getRecordInfo(size_t recordIndex) const {
  if (recordIndex < records_.size()) {
    return records_[recordIndex];
  }
  return nullptr;
}

bool AsyncImageFilter::getNextImage(
    size_t& outRecordIndex,
    ImageContentBlockSpec& outImageSpec,
    std::vector<uint8_t>& outFrame) {
  while (nextRecordIndex_ < records_.size()) {
    pendingRecord_.clear();
    filteredReader_.reader.readRecord(*records_[nextRecordIndex_]);
    outRecordIndex = nextRecordIndex_++;
    if (pendingRecord_.needsImageProcessing()) {
      outImageSpec = pendingRecord_.imageChunk->getContentBlock().image();
      outFrame = move(pendingRecord_.imageChunk->getBuffer());
      pendingRecords_[outRecordIndex] = move(pendingRecord_);
      return true;
    }
  }
  return false;
}

bool AsyncImageFilter::writeProcessedImage(
    size_t recordIndex,
    std::vector<uint8_t>&& processedImage) {
  const auto& iter = pendingRecords_.find(recordIndex);
  if (!XR_VERIFY(iter != pendingRecords_.end(), "Invalid record index ({})", recordIndex) ||
      !XR_VERIFY(iter->second.needsImageProcessing(), "Image {} already processed", recordIndex)) {
    return false;
  }
  iter->second.setBuffer(move(processedImage));
  // try to write out all possibly ready records...
  double timestamp = nan("");
  while (pendingRecords_.begin() != pendingRecords_.end()) {
    auto prIter = pendingRecords_.begin();
    if (prIter->second.needsImageProcessing()) {
      break;
    }
    const auto& record = *records_[prIter->first];
    timestamp = record.timestamp;
    FilteredChunksSource chunkedSource(prIter->second.recordChunks);
    prIter->second.writer->createRecord(
        timestamp, record.recordType, prIter->second.formatVersion, chunkedSource);
    pendingRecords_.erase(prIter);
  }
  if (!isnan(timestamp)) {
    throttledWriter_.onRecordDecoded(timestamp);
  }
  return true;
}

int AsyncImageFilter::closeFile() {
  if (!fileHelper_) {
    XR_LOGE("No file to close.");
    return FAILURE;
  }
  if (!pendingRecords_.empty()) {
    XR_LOGE("Can't close filter: {} images still need processing.", pendingRecords_.size());
    return FAILURE;
  }
  int result = fileHelper_->closeFile();
  if (throttledWriter_.getWriter().getBackgroundThreadQueueByteSize() != 0) {
    XR_LOGE("Unexpected count of bytes left in queue after image filtering!");
  }
  copiers_.clear();
  copyOptions_.outGaiaId = fileHelper_->getGaiaId();
  fileHelper_.reset();
  return result;
}

void AsyncImageFilter::processChunkedRecord(
    Writer& writer,
    const CurrentRecord& hdr,
    deque<unique_ptr<ContentChunk>>& chunks,
    ContentBlockChunk* imageChunk) {
  pendingRecord_.set(move(chunks), imageChunk, &writer, hdr.formatVersion);
}

} // namespace vrs::utils
