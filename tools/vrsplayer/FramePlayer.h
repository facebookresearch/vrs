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

#pragma once

#include <memory>
#include <mutex>
#include <thread>

#include <QtCore/qglobal.h>
#include <qobject.h>

#include <vrs/RecordFormat.h>
#include <vrs/helpers/JobQueue.h>
#include <vrs/utils/PixelFrame.h>
#include <vrs/utils/VideoFrameHandler.h>
#include <vrs/utils/VideoRecordFormatStreamPlayer.h>

#include "FrameWidget.h"
#include "MetaDataCollector.h"

QT_BEGIN_NAMESPACE
class QPixmap;
QT_END_NAMESPACE

enum class FileReaderState;

namespace vrsp {

using ::std::shared_ptr;
using ::std::unique_ptr;
using ::std::vector;
using ::vrs::ContentBlock;
using ::vrs::CurrentRecord;
using ::vrs::DataLayout;
using ::vrs::StreamId;
using ::vrs::StreamPlayer;
using ::vrs::utils::PixelFrame;
using ::vrs::utils::VideoRecordFormatStreamPlayer;

struct ImageJob {
  ImageJob(vrs::ImageFormat imageFormat) : imageFormat{imageFormat} {}
  vrs::ImageFormat imageFormat;
  shared_ptr<PixelFrame> frame;
  vector<uint8_t> buffer;
};

class FramePlayer : public QObject, public VideoRecordFormatStreamPlayer {
  Q_OBJECT
 public:
  explicit FramePlayer(StreamId id, FrameWidget* widget_);

  bool processRecordHeader(const CurrentRecord& r, vrs::DataReference& outDataReference) override;
  bool onDataLayoutRead(const CurrentRecord& r, size_t blockIndex, DataLayout&) override;
  bool onImageRead(const CurrentRecord& r, size_t blockIndex, const ContentBlock&) override;

  StreamId getId() const {
    return id_;
  }
  FrameWidget* getWidget() const {
    return widget_;
  }

  void setVisible(bool visible);
  bool isVisible() const {
    return visible_;
  }
  void setBlankMode(bool blankOn);

  string getFrameName(size_t index, const vrs::IndexRecord::RecordInfo& record);
  // Try to save. Returns true if the frame was saved, false if save happens on next frame read
  bool saveFrameNowOrOnNextRead(const std::string& path);

  void imageJobsThreadActivity();

 public slots:
  void mediaStateChanged(FileReaderState state);

 private:
  std::mutex frameMutex_;
  vector<shared_ptr<PixelFrame>> inputFrames_;
  vector<shared_ptr<PixelFrame>> convertedframes_;
  bool needsConvertedFrame_{false};
  vrs::ImageFormat imageFormat_{vrs::ImageFormat::UNDEFINED};
  StreamId id_;
  FrameWidget* widget_;
  MetaDataCollector descriptions_;
  bool visible_{true};
  bool blankMode_{true};
  bool firstImage_{true};
  string saveNextFramePath_;
  Fps dataFps_;
  FileReaderState state_{};

  vrs::JobQueueWithThread<std::unique_ptr<ImageJob>> imageJobs_;

  void convertFrame(shared_ptr<PixelFrame>& frame);
  void makeBlankFrame(shared_ptr<PixelFrame>& frame);
  shared_ptr<PixelFrame> getFrame(bool inputNotConvertedFrame);
  void recycle(shared_ptr<PixelFrame>& frame, bool inputNotConvertedFrame);
  bool saveFrame(const CurrentRecord& record, const ContentBlock& contentBlock);
};

} // namespace vrsp
