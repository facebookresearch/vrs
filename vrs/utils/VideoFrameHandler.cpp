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

#include "VideoFrameHandler.h"

#define DEFAULT_LOG_CHANNEL "VideoFrameHandler"
#include <logging/Log.h>

#include <vrs/ErrorCode.h>
#include <vrs/RecordFileReader.h>
#include <vrs/helpers/FileMacros.h>
#include <vrs/utils/PixelFrame.h>

using namespace std;

namespace vrs::utils {

int VideoFrameHandler::tryToDecodeFrame(
    void* outDecodedFrame,
    RecordReader* reader,
    const ContentBlock& contentBlock) {
  const ImageContentBlockSpec& spec = contentBlock.image();
  isVideo_ = true;
  requestedKeyFrameTimestamp_ = spec.getKeyFrameTimestamp();
  requestedKeyFrameIndex_ = spec.getKeyFrameIndex();
  videoGoodState_ = requestedKeyFrameIndex_ == 0 ||
      (requestedKeyFrameTimestamp_ == decodedKeyFrameTimestamp_ &&
       requestedKeyFrameIndex_ == decodedKeyFrameIndex_ + 1);
  if (videoGoodState_) {
    decodedKeyFrameTimestamp_ = requestedKeyFrameTimestamp_;
    decodedKeyFrameIndex_ = requestedKeyFrameIndex_;
    // XR_LOGI("Reading frame {}/{}", requestedKeyFrameTimestamp_, requestedKeyFrameIndex_);
    encodedFrame_.resize(contentBlock.getBlockSize());
    IF_ERROR_LOG_AND_RETURN(reader->read(encodedFrame_));
    if (decoder_) {
      return decoder_->decode(encodedFrame_, outDecodedFrame, contentBlock.image());
    }
    decoder_ =
        DecoderFactory::get().makeDecoder(encodedFrame_, outDecodedFrame, contentBlock.image());
    return decoder_ ? SUCCESS : domainError(DecodeStatus::CodecNotFound);
  }
  if (requestedKeyFrameTimestamp_ == decodedKeyFrameTimestamp_) {
    XR_LOGW(
        "Video frame out of sequence. Expected frame {}, got frame {}",
        decodedKeyFrameIndex_ + 1,
        requestedKeyFrameIndex_);
  } else {
    XR_LOGW(
        "Video frame out of sequence. Unexpected jump to {}, frame {}",
        requestedKeyFrameTimestamp_,
        requestedKeyFrameIndex_);
  }
  return domainError(DecodeStatus::FrameSequenceError);
}

int VideoFrameHandler::tryToDecodeFrame(
    PixelFrame& outFrame,
    RecordReader* reader,
    const ContentBlock& contentBlock) {
  outFrame.init(contentBlock.image());
  return tryToDecodeFrame(outFrame.getBuffer().data(), reader, contentBlock);
}

int VideoFrameHandler::readMissingFrames(
    RecordFileReader& fileReader,
    const IndexRecord::RecordInfo& record,
    bool exactFrame) {
  if (isMissingFrames() && getRequestedKeyFrameIndex() != kInvalidFrameIndex &&
      (exactFrame || getFramesToSkip() == 0)) {
    const IndexRecord::RecordInfo* keyframe = fileReader.getRecordByTime(
        record.streamId, Record::Type::DATA, getRequestedKeyFrameTimestamp());
    if (keyframe == nullptr || keyframe->timestamp != getRequestedKeyFrameTimestamp()) {
      return FAILURE;
    }
    uint32_t recordIndex = fileReader.getRecordStreamIndex(keyframe);
    const vector<const IndexRecord::RecordInfo*>& streamIndex =
        fileReader.getIndex(record.streamId);
    uint32_t keyFrameIndex = getRequestedKeyFrameIndex();
    uint32_t frameIndex = 0;
    uint32_t framesToSkip = getFramesToSkip();
    while (recordIndex < streamIndex.size() && frameIndex++ <= keyFrameIndex &&
           streamIndex[recordIndex]->recordType == Record::Type::DATA) {
      if (framesToSkip > 0) {
        framesToSkip--, recordIndex++;
      } else {
        int error = fileReader.readRecord(*streamIndex[recordIndex++]);
        if (error != 0) {
          return error;
        }
        if (isMissingFrames()) {
          return FAILURE;
        }
        if (!exactFrame) {
          break;
        }
      }
    }
  }
  return SUCCESS;
}

uint32_t VideoFrameHandler::getFramesToSkip() const {
  return isVideo_ && decodedKeyFrameTimestamp_ == requestedKeyFrameTimestamp_ &&
          decodedKeyFrameIndex_ + 1 < requestedKeyFrameIndex_
      ? decodedKeyFrameIndex_ + 1
      : 0;
}

void VideoFrameHandler::reset() {
  decodedKeyFrameIndex_ = kInvalidFrameIndex;
  decodedKeyFrameTimestamp_ = 0;
  requestedKeyFrameIndex_ = kInvalidFrameIndex;
  requestedKeyFrameTimestamp_ = 0;
  videoGoodState_ = false;
}

} // namespace vrs::utils
