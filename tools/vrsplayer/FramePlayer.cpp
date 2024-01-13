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
  imageFormat_ = spec.getImageFormat();
  if (imageFormat_ == vrs::ImageFormat::VIDEO) {
    // Video codec decompression happens here, but pixel format conversion is asynchronous
    if (spec.getKeyFrameIndex() > 0) {
      iframesOnly_.store(false, memory_order_relaxed);
    }
    if (firstImage_ || !iframesOnly_.load(memory_order_relaxed)) {
      PixelFrame::init(frame, contentBlock.image());
      unique_lock<mutex> lock(videoDecodingMutex_);
      frameValid = (tryToDecodeFrame(*frame, record, contentBlock) == 0);
    } else {
      frameValid = PixelFrame::readDiskImageData(frame, record.reader, contentBlock);
    }
  } else {
    if (firstImage_) {
      frameValid = PixelFrame::readFrame(frame, record.reader, contentBlock);
    } else {
      frameValid = PixelFrame::readDiskImageData(frame, record.reader, contentBlock);
    }
  }
  if (frameValid && !firstImage_) {
    imageJobs_.startThreadIfNeeded(&FramePlayer::imageJobsThreadActivity, this);
    imageJobs_.sendJob(make_unique<ImageJob>(std::move(frame)));
    return true;
  }
  // Processing was not sent in the background, complete here!
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
    if (frameValid) {
      convertFrame(frame);
      if (needsConvertedFrame_) {
        fmt::print(" -> {}", frame->getSpec().asString());
      }
      if (estimatedFps_ != 0) {
        fmt::print(", {} fps", estimatedFps_);
      }
      frame->blankFrame();
      blankMode_ = true;
      widget_->swapImage(frame);
    }
    fmt::print("\n");
    firstImage_ = false;
  } else {
    // !firstImage_
    if (frameValid) {
      convertFrame(frame);
      widget_->swapImage(frame);
    }
  }
  recycle(frame, !needsConvertedFrame_);
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
      frame = std::move(convertedFrame);
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
  shared_ptr<PixelFrame> frame = std::move(frames.back());
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
        frames.emplace_back(std::move(frame));
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
  string extension =
      toString(imageFormat_ == vrs::ImageFormat::RAW ? vrs::ImageFormat::PNG : imageFormat_);
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
    // if we're behind, we just drop images except the last one!
    while (imageJobs_.getJob(job)) {
      ; // just skip!
    }
    shared_ptr<PixelFrame>& frame = *job;
    bool frameValid = false;
    vrs::ImageFormat imageFormat = frame->getSpec().getImageFormat();
    if (imageFormat == vrs::ImageFormat::RAW) {
      frameValid = (frame != nullptr);
    } else if (imageFormat == vrs::ImageFormat::VIDEO) {
      unique_lock<mutex> lock(videoDecodingMutex_);
      frameValid = frame->decompressImage(&getVideoFrameHandler(id_));
    } else {
      frameValid = frame->decompressImage();
    }
    if (frameValid) {
      convertFrame(frame);
      widget_->swapImage(frame);
    }
    if (imageFormat != vrs::ImageFormat::VIDEO) {
      recycle(frame, !frameValid || !needsConvertedFrame_);
    }
  }
}

} // namespace vrsp
