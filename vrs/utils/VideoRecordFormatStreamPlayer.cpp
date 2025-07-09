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

#include "VideoRecordFormatStreamPlayer.h"

#define DEFAULT_LOG_CHANNEL "VideoRecordFormatStreamPlayer"
#include <logging/Checks.h>

#include <vrs/IndexRecord.h>

#include "PixelFrame.h"

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

VideoFrameHandler& VideoRecordFormatStreamPlayer::getVideoFrameHandler(StreamId streamId) {
  return handlers_[streamId];
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

bool VideoRecordFormatStreamPlayer::readFrame(
    PixelFrame& outFrame,
    const CurrentRecord& record,
    const ContentBlock& cb) {
  if (cb.getContentType() != ContentType::IMAGE) {
    return false;
  }
  if (cb.image().getImageFormat() == ImageFormat::VIDEO) {
    return tryToDecodeFrame(outFrame, record, cb) == 0;
  }
  return outFrame.readFrame(record.reader, cb);
}

void VideoRecordFormatStreamPlayer::resetVideoFrameHandler(StreamId streamId) {
  if (streamId.isValid()) {
    handlers_[streamId].reset();
  } else {
    for (auto& handler : handlers_) {
      handler.second.reset();
    }
  }
}

} // namespace vrs::utils
