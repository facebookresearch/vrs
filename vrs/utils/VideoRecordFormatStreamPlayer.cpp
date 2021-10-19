// Copyright (c) Facebook, Inc. and its affiliates. Confidential and proprietary.

#include "VideoRecordFormatStreamPlayer.h"

#define DEFAULT_LOG_CHANNEL "VideoRecordFormatStreamPlayer"
#include <logging/Checks.h>

#include <vrs/IndexRecord.h>

namespace vrs::utils {

bool VideoRecordFormatStreamPlayer::isMissingFrames(StreamId streamId) const {
  auto iter = handlers_.find(streamId);
  return iter != handlers_.end() && iter->second.isMissingFrames();
}

bool VideoRecordFormatStreamPlayer::isMissingFrames() const {
  XR_CHECK(handlers_.size() <= 1);
  return !handlers_.empty() && handlers_.begin()->second.isMissingFrames();
}

int VideoRecordFormatStreamPlayer::readMissingFrames(
    RecordFileReader& fileReader,
    const IndexRecord::RecordInfo& recordInfo,
    bool exactFrame) {
  int result = 0;
  VideoFrameHandler& handler = handlers_[recordInfo.streamId];
  if (!whileReadingMissingFrames_ && handler.isMissingFrames()) {
    whileReadingMissingFrames_ = true;
    result = handler.readMissingFrames(fileReader, recordInfo, exactFrame);
    whileReadingMissingFrames_ = false;
  }
  return result;
}

int VideoRecordFormatStreamPlayer::tryToDecodeFrame(
    void* outBuffer,
    const CurrentRecord& record,
    const ContentBlock& contentBlock) {
  return handlers_[record.streamId].tryToDecodeFrame(outBuffer, record.reader, contentBlock);
}

int VideoRecordFormatStreamPlayer::tryToDecodeFrame(
    PixelFrame& outFrame,
    const CurrentRecord& record,
    const ContentBlock& contentBlock) {
  return handlers_[record.streamId].tryToDecodeFrame(outFrame, record.reader, contentBlock);
}

void VideoRecordFormatStreamPlayer::resetVideoFrameHandler(const StreamId& streamId) {
  if (streamId.isValid()) {
    handlers_[streamId].reset();
  } else {
    for (auto& handler : handlers_) {
      handler.second.reset();
    }
  }
}

} // namespace vrs::utils
