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

#include <map>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

#include <QtCore/qglobal.h>
#include <qobject.h>
#include <qsettings.h>
#include <qtimer.h>

#include <vrs/RecordFileReader.h>
#include <vrs/RecordFormatStreamPlayer.h>
#include <vrs/os/Event.h>

#include "AudioPlayer.h"
#include "FramePlayer.h"
#include "FrameWidget.h"
#include "VideoTime.h"

QT_BEGIN_NAMESPACE
class QPixmap;
class QVBoxLayout;
QT_END_NAMESPACE

enum class FileReaderState { Undefined, NoMedia, Paused, Playing, Error, Count };
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
Q_DECLARE_METATYPE(FileReaderState)
#endif

namespace vrsp {

using ::vrs::ProgressLogger;
using ::vrs::RecordFileReader;

using vrs::os::EventChannel;

class PlayerUI;

enum class Action { ShowFrame = 1, ShowFrameFast, ChangeFrame };
enum class Direction { Forward, Backward };
enum class Seek { Accurate, Fast };

// Helper class to bundle an enum + frameCount as int64_t, for use in an event
struct DispatchAction {
  explicit DispatchAction(Action _action) : action{_action}, frameCount{0} {}
  explicit DispatchAction(Action _action, int32_t _frameCount)
      : action{_action}, frameCount{_frameCount} {}

  int64_t bundle() const {
    return static_cast<int64_t>(frameCount) << 32 | static_cast<int64_t>(action);
  }
  static DispatchAction fromBundle(int64_t bundle) {
    return DispatchAction(
        static_cast<Action>(bundle & 0xffffffff),
        static_cast<int32_t>((bundle >> 32) & 0xffffffff));
  }

  Action action;
  int32_t frameCount;
};

class FileReader : public QObject {
  Q_OBJECT

 public:
  static constexpr double kMaxPlaybackAge = 0.2;

  explicit FileReader(QObject* parent = nullptr);
  ~FileReader();

  void setPlayerUi(PlayerUI* ui) {
    playerUi_ = ui;
  }
  FileReaderState getState() const {
    return state_;
  }
  bool isAtBegin() const;
  bool isAtEnd() const;
  bool isLocalFile() const {
    return isLocalFile_;
  }
  int getImageCount() const {
    return static_cast<int>(imageReaders_.size());
  }
  void layoutFrames(QVBoxLayout* videoFrames, QWidget* parent, int maxPerRow);
  void saveConfiguration();
  void loadConfiguration();

 signals:
  void mediaStateChanged(FileReaderState state);
  void durationChanged(double start, double end, int duration);
  void timeChanged(double time, int position);
  void statusStateChanged(const std::string& status);
  void adjustSpeed(int change);
  void updateLayoutMenu(
      int frameCount,
      int visibleCount,
      int maxPerRowCount,
      const QVariantMap& presets,
      const QVariant& currentPreset);
  void fileChanged(QWidget* widget, const vrs::FileSpec& spec);

 public slots:
  std::vector<FrameWidget*> openFile(const QString& url, QVBoxLayout* videoFrame, QWidget* parent);
  void setOverlayColor(QColor color);
  void recordTypeChanged(const QString& type);
  void setPosition(int position);
  void sliderPressed();
  void sliderReleased();
  bool eventFilter(QObject* obj, QEvent* event);
  void stop();
  void play();
  void pause();
  void nextFrame();
  void previousFrame();
  void updatePosition();
  void relayout();
  void setPlaybackSpeed(double speed);
  void enableAllStreams();
  void toggleVisibleStreams();
  void resetOrientation();
  void disableStream(StreamId id);
  void moveStream(StreamId id, bool beforeNotAfter);
  void saveFrame(StreamId id);
  void saveFrames();
  void savePreset(const QString& preset);
  void recallPreset(const QString& preset);
  void deletePreset(const QString& preset);

 private:
  int timeToPosition(double time) const;
  double positionToTime(int position) const;
  void setState(FileReaderState newState);
  void setTimeRange(double start, double end, uint32_t firstDataRecordIndex);
  void playThreadActivity();
  void playAction(DispatchAction action);
  bool playFrameSet(const std::set<size_t>& frameSet, Seek strategy);
  bool getFrameSet(std::set<size_t>& outSet, size_t start, Direction direction) const;
  double getNextRecordDelay();
  void setBlankMode(bool setBlankMode);
  void clearLayout(QLayout* layout, bool deleteWidgets);
  void readFirstRecord(StreamId id, Record::Type recordType);
  void setErrorText(const std::string& errorText);
  bool isAudio(StreamId id) const;
  bool isVideo(StreamId id) const;
  bool isPlaying(StreamId id) const {
    return isAudio(id) || isVisibleVideo(id);
  }
  bool isVisibleVideo(StreamId id) const;
  std::string getDeviceName(StreamId id);
  void sanitizeVisibleStreams(bool reset = false);
  QVariant configurationAsVariant();
  void applyConfiguration(const QVariant& config);
  void layoutConfigChanged();
  void restoreCurrentConfig();
  int readRecordIfNeeded(const vrs::IndexRecord::RecordInfo& record, size_t recordIndex, bool log);

  std::vector<FrameWidget*> openFile(QVBoxLayout* videoFrames, QWidget* parent);
  void closeFile();

 private:
  PlayerUI* playerUi_;
  std::vector<StreamId> visibleStreams_;
  QVBoxLayout* videoFrames_{};
  int lastMaxPerRow_{0};
  std::map<StreamId, std::unique_ptr<FramePlayer>> imageReaders_;
  std::map<StreamId, std::unique_ptr<AudioPlayer>> audioReaders_;
  std::map<StreamId, size_t> lastReadRecords_;
  Record::Type recordType_{Record::Type::UNDEFINED};
  QTimer slowTimer_;
  FileReaderState state_;
  std::unique_ptr<RecordFileReader> fileReader_;
  bool isLocalFile_;
  bool isSliderActive_;
  bool layoutUpdatesEnabled_{true};
  double startTime_;
  double endTime_;
  uint32_t firstDataRecordIndex_;
  double lastShownTime_;
  size_t nextRecord_;
  VideoTime time_;
  bool runThread_{true};
  EventChannel waitEvent_{"video_wait", EventChannel::NotificationMode::UNICAST};
  std::recursive_mutex mutex_;
  std::thread thread_;

  // File specific configuration
  std::unique_ptr<QSettings> fileConfig_;
  std::map<StreamId, StreamId> fileToConfig_;
  QVariantMap layoutPresets_;
};

} // namespace vrsp
