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

#include "FramePlayer.h"

#include <sstream>

#include <fmt/format.h>

#define DEFAULT_LOG_CHANNEL "FramePlayer"
#include <logging/Log.h>

#include <vrs/DiskFile.h>
#include <vrs/IndexRecord.h>

namespace vrsp {

using namespace std;

FramePlayer::FramePlayer(StreamId id, FrameWidget* widget)
    : QObject(nullptr), id_{id}, widget_{widget} {}

bool FramePlayer::processRecordHeader(
    const CurrentRecord& record,
    vrs::DataReference& outDataReference) {
  return visible_ ? VideoRecordFormatStreamPlayer::processRecordHeader(record, outDataReference)
                  : false;
}

bool FramePlayer::onDataLayoutRead(
    const CurrentRecord& record,
    size_t blockIndex,
    DataLayout& layout) {
  ostringstream buffer;
  layout.printLayoutCompact(buffer);
  string text = buffer.str();
  descriptions_.setDescription(record.recordType, blockIndex, text);
  widget_->setDescription(record.recordType, blockIndex, text);
  if (firstImage_ && record.recordType == Record::Type::CONFIGURATION) {
    vrs::DataPieceString* deviceType = layout.findDataPieceString("device_type");
    if (deviceType != nullptr) {
      widget_->setDeviceType(deviceType->get());
    }
  }
  return true; // read next blocks, if any
}

bool FramePlayer::onImageRead(
    const CurrentRecord& record,
    size_t /*blockIndex*/,
    const ContentBlock& contentBlock) {
  if (!saveNextFramePath_.empty()) {
    return saveFrame(record, contentBlock);
  }
  widget_->setDataFps(dataFps_.newFrame()); // fps counter for images read from file
  const auto& spec = contentBlock.image();
  shared_ptr<PixelFrame> frame = getFrame(true);
  bool frameValid = false;
  unique_ptr<ImageJob> job;
  imageFormat_ = spec.getImageFormat();
  if (imageFormat_ == vrs::ImageFormat::RAW) {
    // RAW images are read from disk directly, but pixel format conversion is asynchronous
    frameValid = PixelFrame::readRawFrame(frame, record.reader, spec);
    if (!firstImage_ && frameValid) {
      job = make_unique<ImageJob>(vrs::ImageFormat::RAW);
    }
  } else if (imageFormat_ == vrs::ImageFormat::VIDEO) {
    // Video codec decompression happens here, but pixel format conversion is asynchronous
    PixelFrame::init(frame, contentBlock.image());
    frameValid = (tryToDecodeFrame(*frame, record, contentBlock) == 0);
    if (!firstImage_ && frameValid) {
      job = make_unique<ImageJob>(vrs::ImageFormat::VIDEO);
    }
  } else {
    if (firstImage_) {
      frameValid = PixelFrame::readFrame(frame, record.reader, contentBlock);
    } else {
      // decoding & pixel format conversion happen asynchronously
      job = make_unique<ImageJob>(imageFormat_);
      job->buffer.resize(contentBlock.getBlockSize());
      frameValid = (record.reader->read(job->buffer) == 0);
    }
  }
  if (frameValid && job) {
    job->frame = move(frame);
    imageJobs_.startThreadIfNeeded(&FramePlayer::imageJobsThreadActivity, this);
    imageJobs_.sendJob(move(job));
    return true;
  }
  if (firstImage_) {
    fmt::print(
        "Found '{} - {}': {}, {}",
        record.streamId.getNumericName(),
        record.streamId.getTypeName(),
        getCurrentRecordFormatReader()->recordFormat.asString(),
        spec.asString());
    if (frameValid && spec.getImageFormat() != vrs::ImageFormat::RAW) {
      fmt::print(" - {}", frame->getSpec().asString());
    }
    blankMode_ = false;
  }
  if (frameValid) {
    convertFrame(frame);
    if (firstImage_) {
      if (needsConvertedFrame_) {
        fmt::print(" -> {}", frame->getSpec().asString());
      }
      if (estimatedFps_ != 0) {
        fmt::print(", {} fps", estimatedFps_);
      }
      frame->blankFrame();
      blankMode_ = true;
    }
    widget_->swapImage(frame);
  }
  recycle(frame, !needsConvertedFrame_);
  if (firstImage_) {
    fmt::print("\n");
    firstImage_ = false;
  }
  return true; // read next blocks, if any
}

void FramePlayer::setVisible(bool visible) {
  visible_ = visible;
  widget_->setVisible(visible_);
}

void FramePlayer::convertFrame(shared_ptr<PixelFrame>& frame) {
  if (blankMode_) {
    makeBlankFrame(frame);
  } else {
    shared_ptr<PixelFrame> convertedFrame = needsConvertedFrame_ ? getFrame(false) : nullptr;
    PixelFrame::normalizeFrame(frame, convertedFrame, false);
    needsConvertedFrame_ = (frame != convertedFrame); // for next time!
    if (needsConvertedFrame_) {
      recycle(frame, true);
      frame = move(convertedFrame);
    }
  }
}

void FramePlayer::makeBlankFrame(shared_ptr<PixelFrame>& frame) {
  frame->init(vrs::PixelFormat::GREY8, frame->getWidth(), frame->getHeight());
  frame->blankFrame();
}

shared_ptr<PixelFrame> FramePlayer::getFrame(bool inputNotConvertedFrame) {
  unique_lock<mutex> lock(frameMutex_);
  vector<shared_ptr<PixelFrame>>& frames = inputNotConvertedFrame ? inputFrames_ : convertedframes_;
  if (frames.empty()) {
    return nullptr;
  }
  shared_ptr<PixelFrame> frame = move(frames.back());
  frames.pop_back();
  return frame;
}

void FramePlayer::recycle(shared_ptr<PixelFrame>& frame, bool inputNotConvertedFrame) {
  if (frame) {
    {
      unique_lock<mutex> lock(frameMutex_);
      vector<shared_ptr<PixelFrame>>& frames =
          inputNotConvertedFrame ? inputFrames_ : convertedframes_;
      if (frames.size() < 10) {
        frames.emplace_back(move(frame));
      }
    }
    frame.reset();
  }
}

void FramePlayer::setBlankMode(bool blankOn) {
  if (blankMode_ != blankOn) {
    blankMode_ = blankOn;
    if (blankMode_) {
      widget_->blank();
    } else {
      // Descriptions are lost when we blank the widget, so we need to restore them, but we don't
      // need to restore DATA record descriptions, as they're present with every data record.
      widget_->setDescriptions(
          Record::Type::CONFIGURATION, descriptions_.getDescriptions(Record::Type::CONFIGURATION));
      widget_->setDescriptions(
          Record::Type::STATE, descriptions_.getDescriptions(Record::Type::STATE));
    }
  }
}

string FramePlayer::getFrameName(size_t index, const vrs::IndexRecord::RecordInfo& record) {
  string extension = toString(imageFormat_);
  return fmt::format(
      "{}-{:05}-{:.3f}.{}", record.streamId.getNumericName(), index, record.timestamp, extension);
}

void FramePlayer::mediaStateChanged(FileReaderState state) {
  if (state != state_) {
    dataFps_.reset();
    state_ = state;
  }
}

bool FramePlayer::saveFrameNowOrOnNextRead(const std::string& path) {
  if (imageFormat_ == vrs::ImageFormat::VIDEO) {
    // Save the frame visible in the Widget
    if (widget_->saveImage(path) == 0) {
      XR_LOGI("Saved video frame as '{}'", path);
    }
    return true;
  }
  // We need to read the record again to save the best data possible
  saveNextFramePath_ = path;
  return false;
}

bool FramePlayer::saveFrame(
    const vrs::CurrentRecord& record,
    const vrs::ContentBlock& contentBlock) {
  const auto& spec = contentBlock.image();
  if (spec.getImageFormat() == vrs::ImageFormat::RAW) {
    shared_ptr<PixelFrame> frame;
    if (PixelFrame::readRawFrame(frame, record.reader, spec)) {
      shared_ptr<PixelFrame> normalizedFrame;
      PixelFrame::normalizeFrame(frame, normalizedFrame, true);
      if (normalizedFrame->writeAsPng(saveNextFramePath_) == 0) {
        XR_LOGI("Saved raw frame as '{}'", saveNextFramePath_);
      }
    }
  } else {
    vector<uint8_t> buffer;
    buffer.resize(contentBlock.getBlockSize());
    vrs::AtomicDiskFile file;
    if (record.reader->read(buffer) == 0 && file.create(saveNextFramePath_) == 0) {
      if (file.write(buffer.data(), buffer.size()) != 0) {
        file.abort();
      } else {
        XR_LOGI("Saved {} frame as '{}'", toString(spec.getImageFormat()), saveNextFramePath_);
      }
    }
  }
  saveNextFramePath_.clear();
  return true;
}

void FramePlayer::imageJobsThreadActivity() {
  unique_ptr<ImageJob> job;
  while (imageJobs_.waitForJob(job)) {
    shared_ptr<PixelFrame> frame = move(job->frame);
    // if we're behind, we just drop images except the last one!
    while (imageJobs_.getJob(job)) {
      recycle(frame, true);
      frame = move(job->frame);
    }
    bool frameValid = false;
    if (job->imageFormat == vrs::ImageFormat::RAW || job->imageFormat == vrs::ImageFormat::VIDEO) {
      frameValid = (frame != nullptr);
    } else {
      if (!frame) {
        frame = make_shared<PixelFrame>();
      }
      frameValid = frame->readCompressedFrame(job->buffer, job->imageFormat);
    }
    if (frameValid) {
      convertFrame(frame);
      widget_->swapImage(frame);
    }
    recycle(frame, !frameValid || !needsConvertedFrame_);
  }
}

} // namespace vrsp
