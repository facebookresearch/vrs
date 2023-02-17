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
#include <deque>
#include <map>
#include <memory>
#include <mutex>

#include <QtCore/qglobal.h>
#include <QtWidgets/QWidget>

#include <vrs/Record.h>

#include "MetaDataCollector.h"
#include "VideoTime.h"

namespace vrs::utils {
class PixelFrame;
}

namespace vrsp {

using std::map;
using std::shared_ptr;
using std::string;
using vrs::utils::PixelFrame;

// Frame Per Second estimator class
class Fps {
 public:
  void reset() {
    lastTimestamps_.clear();
  }
  // Call when there is a new frame, and get an updated fps estimation
  int newFrame() {
    double time = VideoTime::getRawTime();
    // When switching to PortAudio, during init the time source might change.
    if (!lastTimestamps_.empty() && lastTimestamps_.back() >= time) {
      lastTimestamps_.clear();
    }
    lastTimestamps_.push_back(time);
    return value(time);
  }
  int value(double time = VideoTime::getRawTime()) {
    double timeLimit = time - 1.5; // only keep recent data
    while (!lastTimestamps_.empty() && lastTimestamps_.front() < timeLimit) {
      lastTimestamps_.pop_front();
    }
    size_t count = lastTimestamps_.size();
    if (count < 2) {
      return 0;
    }
    return static_cast<int>(
        (count - 1) / static_cast<float>(lastTimestamps_.back() - lastTimestamps_.front()) + 0.5F);
  }

 private:
  std::deque<double> lastTimestamps_;
};

class FrameWidget : public QWidget {
  Q_OBJECT
 public:
  explicit FrameWidget(QWidget* parent = 0);

  void paintEvent(QPaintEvent* evt) override;
  QSize sizeHint() const override;
  int heightForWidth(int w) const override;
  bool hasHeightForWidth() const override {
    return true;
  }

  void setNeedsUpdate() {
    needsUpdate_.store(true);
  }
  bool getAndClearNeedsUpdate() {
    return needsUpdate_.exchange(false);
  }

  QSize rotate(QSize size) const;
  QSize getImageSize() const;

  void setDataFps(int dataFps) {
    dataFps_ = dataFps;
  }
  void setDeviceName(const string& deviceName);
  void setDeviceType(const string& deviceType);
  void setDescription(Record::Type recordType, size_t blockIndex, const string& description);
  void setDescriptions(Record::Type recordType, const map<size_t, QString>& descriptions);
  void setTags(const map<string, string>& tags);

  void resetOrientation();
  void setRotation(int rotation);
  int getRotation() const {
    return rotation_;
  }

  void setFlipped(bool flipped);
  bool getFlipped() const {
    return flipped_;
  }

  void swapImage(shared_ptr<PixelFrame>& image);
  int saveImage(const string& path);

  void setTypeToShow(Record::Type type) {
    typeToShow_ = type;
    setNeedsUpdate();
  }
  void setOverlayColor(QColor color) {
    overlayColor_ = color;
    setNeedsUpdate();
  }
  void setFontSize(int fontSize) {
    fontSize_ = fontSize;
    setNeedsUpdate();
  }
  void setSolidBackground(bool solid) {
    solidBackground_ = solid;
    setNeedsUpdate();
  }
  void blank();
  void updateMinMaxSize();

 signals:
  void orientationChanged();
  void shouldHideStream();
  void shouldMoveBefore();
  void shouldMoveAfter();
  void shouldSaveFrame();

 public slots:
  void ShowContextMenu(const QPoint& pos);

 private:
  std::mutex mutex_;
  shared_ptr<PixelFrame> image_;
  string deviceTypeTag_;
  string deviceTypeConfig_;
  QSize imageSize_{640, 480};
  MetaDataCollector descriptions_;
  Record::Type typeToShow_{Record::Type::DATA};
  std::atomic<bool> needsUpdate_{true};
  std::atomic<int> dataFps_{0};
  Fps imageFps_;
  Fps drawFps_;
  QColor overlayColor_{Qt::yellow};
  int fontSize_{14};
  bool solidBackground_{false};
  int rotation_{0};
  bool flipped_{false};
  bool hasFrame_{false};
};

} // namespace vrsp
