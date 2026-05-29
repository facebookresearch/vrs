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

#include <atomic>
#include <memory>
#include <mutex>

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

using namespace vrs;

using utils::PixelFrame;
using utils::VideoRecordFormatStreamPlayer;

using ImageJob = unique_ptr<PixelFrame>;

class FramePlayer : public QObject, public VideoRecordFormatStreamPlayer {
  Q_OBJECT
 public:
  explicit FramePlayer(StreamId id, FrameWidget* widget_);

  bool processRecordHeader(const CurrentRecord& r, DataReference& outDataReference) override;
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

  string getFrameName(size_t index, const IndexRecord::RecordInfo& record);
  // Try to save. Returns true if the frame was saved, false if save happens on next frame read
  bool saveFrameNowOrOnNextRead(const string& path);

  void setEstimatedFps(int estimatedFps) {
    estimatedFps_ = estimatedFps;
  }

  void imageJobsThreadActivity();

 signals:
  /// Emitted on the FramePlayer's image-decoding background thread, after pixel-format
  /// normalization (`convertFrame`) and before the frame is handed to the widget for display.
  /// Slots MUST be connected with Qt::DirectConnection so they run synchronously in the
  /// emitting thread; the frame is mutated in place and any latency directly stalls display.
  ///
  /// Mutation contract:
  ///   - The slot MAY mutate the PixelFrame in place (e.g., overlay annotations).
  ///   - The slot MAY std::move out of `frame` and replace it with a different PixelFrame
  ///     of any pixel format/size; the new frame is what gets displayed.
  ///   - The slot MAY reset `frame` to nullptr to suppress display of this frame entirely.
  ///   - The slot MUST NOT retain the unique_ptr beyond the call; ownership stays with
  ///     FramePlayer.
  void frameReadyForPostProcessing(
      vrs::StreamId streamId,
      std::unique_ptr<vrs::utils::PixelFrame>& frame);

 public slots:
  void mediaStateChanged(FileReaderState state);

 private:
  std::mutex videoDecodingMutex_;
  std::mutex frameMutex_;
  std::deque<unique_ptr<PixelFrame>> recycledFrames_;
  utils::NormalizeOptions normalizeOptions_;
  bool needsConvertedFrame_{false};
  ImageFormat imageFormat_{ImageFormat::UNDEFINED};
  StreamId id_;
  FrameWidget* widget_;
  MetaDataCollector descriptions_;
  bool visible_{true};
  bool blankMode_{true};
  bool firstImage_{true};
  std::atomic<bool> iframesOnly_{true};
  string saveNextFramePath_;
  int estimatedFps_{};
  Fps dataFps_;
  FileReaderState state_{};

  JobQueueWithThread<unique_ptr<ImageJob>> imageJobs_;

  void convertFrame(unique_ptr<PixelFrame>& frame);
  static void makeBlankFrame(unique_ptr<PixelFrame>& frame);
  unique_ptr<PixelFrame> getFrame(PixelFormat format);
  void recycle(unique_ptr<PixelFrame>& frame);
  bool saveFrame(const CurrentRecord& record, const ContentBlock& contentBlock);
};

} // namespace vrsp
