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

#include "FileReader.h"

#ifdef Q_OS_WIN
#include <cstdio>
#endif

#include <algorithm>
#include <iomanip>
#include <iostream>
#include <sstream>

#include <qapplication.h>
#include <qboxlayout.h>
#include <qevent.h>
#include <qfiledialog.h>
#include <qjsonarray.h>
#include <qjsondocument.h>
#include <qjsonobject.h>
#include <qprogressdialog.h>
#include <qstandardpaths.h>
#include <qstring.h>

#if QT_VERSION < QT_VERSION_CHECK(5, 14, 0)
#include <qdesktopwidget.h>
#else
#include <qscreen.h>
#endif

#define DEFAULT_LOG_CHANNEL "FileReader"
#include <logging/Log.h>

#include <vrs/ErrorCode.h>
#include <vrs/helpers/EnumStringConverter.h>
#include <vrs/os/Utils.h>
#include <vrs/utils/FrameRateEstimator.h>
#include <vrs/utils/RecordFileInfo.h>

#include "AudioPlayer.h"
#include "PlayerUI.h"

#define FIXED_3 fixed << setprecision(3)

namespace {
const char* sStateNames[] = {"UNDEFINED", "NO_MEDIA", "PAUSED", "PLAYING", "ERROR"};

struct FileReaderStateConverter : public EnumStringConverter<
                                      FileReaderState,
                                      sStateNames,
                                      COUNT_OF(sStateNames),
                                      FileReaderState::Undefined,
                                      FileReaderState::Undefined> {
  static_assert(
      cNamesCount == vrs::enumCount<FileReaderState>(),
      "Missing FileReaderState name definitions");
};

QString kLastMaxPerRow("last_max_per_row");
QString kVisibleStreams("visible_streams");
QString kDefaultPreset("Default"); // name shown in the UI
QString kLastConfiguration("last_configuration");
QString kLayoutPresets("layout_presets");

class FlagKeeper {
 public:
  FlagKeeper(bool& flag) : flag_{flag}, finalValue_{flag_} {
    flag_ = false;
  }
  FlagKeeper(bool& flag, bool finalValue) : flag_{flag}, finalValue_{finalValue} {
    flag_ = false;
  }
  ~FlagKeeper() {
    flag_ = finalValue_;
  }

 private:
  bool& flag_;
  bool finalValue_;
};

} // namespace

using namespace std;
using namespace vrs;

namespace vrsp {

const int32_t kPageSize = 10;
const int32_t kBigPageSize = 100;

static int64_t kReadCurrentFrame = DispatchAction(Action::ShowFrame).bundle();
static int64_t kReadCurrentFrameFast = DispatchAction(Action::ShowFrameFast).bundle();
static int64_t kReadPreviousFrame = DispatchAction(Action::ChangeFrame, -1).bundle();
static int64_t kReadNextFrame = DispatchAction(Action::ChangeFrame, 1).bundle();
static int64_t kReadPreviousPage = DispatchAction(Action::ChangeFrame, -kPageSize).bundle();
static int64_t kReadNextPage = DispatchAction(Action::ChangeFrame, kPageSize).bundle();
static int64_t kReadPreviousBigPage = DispatchAction(Action::ChangeFrame, -kBigPageSize).bundle();
static int64_t kReadNextBigPage = DispatchAction(Action::ChangeFrame, kBigPageSize).bundle();

FileReader::FileReader(QObject* parent) : QObject(parent) {
  isSliderActive_ = false;
  slowTimer_.setSingleShot(false);
  connect(&slowTimer_, &QTimer::timeout, this, &FileReader::updatePosition);
  slowTimer_.start(33);

  // start background thread
  thread_ = std::thread{[this]() mutable { this->playThreadActivity(); }};
}

FileReader::~FileReader() {
  closeFile();
  if (thread_.joinable()) {
    runThread_ = false;
    waitEvent_.dispatchEvent();
    thread_.join();
  }
}

bool FileReader::isAtBegin() const {
  return fileReader_ != nullptr && nextRecord_ <= firstDataRecordIndex_;
}

bool FileReader::isAtEnd() const {
  return fileReader_ != nullptr && nextRecord_ >= fileReader_->getIndex().size();
}

void FileReader::closeFile() {
  stop();
  if (fileConfig_) {
    saveConfiguration();
    fileConfig_.reset();
  }
  unique_lock<recursive_mutex> guard{mutex_};
  fileReader_.reset();
  imageReaders_.clear();
  audioReaders_.clear();
  lastReadRecords_.clear();
  if (videoFrames_ != nullptr) {
    clearLayout(videoFrames_, true);
  }
  lastMaxPerRow_ = 0;
}

class OpenProgressDialog : public ProgressLogger {
  static const int kStepScale = 100;
  static constexpr double kCancelCheckDelaySec = 0.1;

 public:
  OpenProgressDialog(PlayerUI* playerUi, const FileSpec& spec)
      : progressDialog_("Opening...", "Cancel", 20, 100, playerUi) {
    progressDialog_.setMinimumWidth(350);
    progressDialog_.setWindowModality(Qt::WindowModal);
    progressDialog_.setRange(0, stepCount_ * kStepScale);
    if (!spec.isDiskFile()) {
      logMessage("Opening from " + spec.fileHandlerName + "...");
      progressDialog_.show();
    }
    nextCancelCheckTime_ = 0;
    keepGoing_ = true;
  }

  bool shouldKeepGoing() override {
    const double now = VideoTime::getRawTime();
    if (now > nextCancelCheckTime_) {
      qApp->processEvents(); // let the app breathe regularly, but not too frequently either
      if (keepGoing_ && progressDialog_.wasCanceled()) {
        keepGoing_ = false;
      }
      nextCancelCheckTime_ = now + kCancelCheckDelaySec;
    }
    return keepGoing_;
  }

  void setStepCount(int stepCount) override {
    ProgressLogger::setStepCount(stepCount + 1);
    progressDialog_.setRange(0, stepCount_ * kStepScale);
  }

 protected:
  void logMessage(const string& message) override {
    ProgressLogger::logMessage(message);
    progressDialog_.setLabelText(QString().fromStdString(message));
    qApp->processEvents();
  }
  void logError(const string& message) override {
    ProgressLogger::logError(message);
    progressDialog_.setLabelText(QString().fromStdString(message));
    qApp->processEvents();
  }
  void updateStep(size_t progress, size_t maxProgress) override {
    int v = stepNumber_ * kStepScale + static_cast<int>(progress * kStepScale / maxProgress);
    progressDialog_.setValue(v);
    qApp->processEvents();
  }

 private:
  QProgressDialog progressDialog_;
  double nextCancelCheckTime_;
  bool keepGoing_;
};

static QString getFileName(const FileSpec& spec) {
  if (!spec.fileName.empty()) {
    return QString::fromStdString(spec.fileName);
  }
  if (spec.isDiskFile() && !spec.chunks.empty()) {
    return QString::fromStdString(os::getFilename(spec.chunks.front()));
  }
  if (spec.chunks.size() == 1 && !spec.fileHandlerName.empty()) {
    return QString::fromStdString(
        os::getFilename(spec.chunks.front()) + " (" + spec.fileHandlerName + ")");
  }
  if (!spec.uri.empty()) {
    return QString::fromStdString(spec.uri);
  }
  return QString::fromStdString(spec.toJson());
}

vector<FrameWidget*>
FileReader::openFile(const QString& qpath, QVBoxLayout* videoFrame, QWidget* widget) {
  closeFile();
  string path = qpath.toStdString();
  FileSpec spec;
  if (spec.fromPathJsonUri(path) != 0) {
    setErrorText("Can't open " + path);
    return {};
  }
  unique_lock<recursive_mutex> guard{mutex_};
  OpenProgressDialog progressUi(playerUi_, spec);
  cout << "Loading " << path << "...\n";
  fileReader_ = make_unique<RecordFileReader>();
  widget->setWindowTitle(getFileName(spec));
  int error = 0;
  if (spec.isDiskFile()) {
    error = fileReader_->openFile(spec);
    if (error != 0) {
      setErrorText(errorCodeToMessage(error));
    }
    isLocalFile_ = true;
  } else {
    fileReader_->setOpenProgressLogger(&progressUi);
    double before = VideoTime::getRawTime();
    error = fileReader_->openFile(spec);
    if (progressUi.shouldKeepGoing()) {
      if (error != 0) {
        setErrorText(errorCodeToMessage(error));
      }
    } else {
      emit statusStateChanged("Open Cancelled.");
    }
    isLocalFile_ = false;
    cout << "Opened from " << spec.fileHandlerName << " in "
         << helpers::humanReadableDuration(VideoTime::getRawTime() - before) << ".\n";
  }
  if (error == 0) {
    fileChanged(widget, spec);
  } else {
    setState(FileReaderState::Error);
    videoFrames_ = nullptr;
    return {};
  }
  RecordFileInfo::printOverview(cout, *fileReader_);
  progressUi.logNewStep("Loading first frames");
  return openFile(videoFrame, widget);
}

static void statsCallback(const FileHandler::CacheStats& stats) {}

vector<FrameWidget*> FileReader::openFile(QVBoxLayout* videoFrames, QWidget* widget) {
  FlagKeeper disableLayoutUpdates(layoutUpdatesEnabled_, true);
  loadConfiguration();
  vector<FrameWidget*> frames;
  videoFrames_ = videoFrames;
  setState(FileReaderState::Paused);
  fileReader_->setStatsCallback(statsCallback);
  const auto& index = fileReader_->getIndex();
  if (!index.empty()) {
    double startTime = numeric_limits<double>::max();
    double endTime = numeric_limits<double>::lowest();
    uint32_t firstDataRecordIndex = static_cast<uint32_t>(index.size());
    const auto& ids = fileReader_->getStreams();
    for (StreamId id : ids) {
      if (imageReaders_.find(id) == imageReaders_.end()) {
        bool mightContainImagesOrAudio = false;
        if (fileReader_->mightContainImages(id)) {
          FrameWidget* frame = new FrameWidget();
          frame->setTypeToShow(recordType_);
          FramePlayer* player = new FramePlayer(id, frame);
          fileReader_->setStreamPlayer(id, player);
          player->setEstimatedFps(static_cast<int>(utils::frameRateEstimationFps(index, id) + 0.5));
          imageReaders_[id].reset(player);
          connect(frame, &FrameWidget::orientationChanged, [this]() {
            if (layoutUpdatesEnabled_) {
              relayout();
            }
          });
          connect(frame, &FrameWidget::shouldHideStream, [this, id]() { this->disableStream(id); });
          connect(
              frame, &FrameWidget::shouldMoveBefore, [this, id]() { this->moveStream(id, true); });
          connect(
              frame, &FrameWidget::shouldMoveAfter, [this, id]() { this->moveStream(id, false); });
          connect(frame, &FrameWidget::shouldSaveFrame, [this, id]() { this->saveFrame(id); });
          connect(this, &FileReader::mediaStateChanged, player, &FramePlayer::mediaStateChanged);
          // decode first config & data record, to init the image size
          readFirstRecord(id, Record::Type::CONFIGURATION);
          readFirstRecord(id, Record::Type::STATE);
          readFirstRecord(id, Record::Type::DATA);
          frame->blank();
          frame->setTags(fileReader_->getTags(id).user);
          frame->setDeviceName(getDeviceName(id));
          frames.push_back(frame);
          mightContainImagesOrAudio = true;
        } else if (fileReader_->mightContainAudio(id)) {
          AudioPlayer* player = new AudioPlayer();
          audioReaders_[id].reset(player);
          fileReader_->setStreamPlayer(id, player);
          connect(this, &FileReader::mediaStateChanged, player, &AudioPlayer::mediaStateChanged);
          readFirstRecord(id, Record::Type::CONFIGURATION);
          readFirstRecord(id, Record::Type::STATE);
          readFirstRecord(id, Record::Type::DATA);
          mightContainImagesOrAudio = true;
        }
        if (mightContainImagesOrAudio) {
          // Update the time range we're interested in
          const IndexRecord::RecordInfo* record = fileReader_->getRecord(id, Record::Type::DATA, 0);
          if (record != nullptr) {
            startTime = min<double>(startTime, record->timestamp);
            uint32_t idx = fileReader_->getRecordIndex(record);
            firstDataRecordIndex = min<uint32_t>(firstDataRecordIndex, idx);
          }
          record = fileReader_->getLastRecord(id, Record::Type::DATA);
          if (record != nullptr && record->timestamp > endTime) {
            endTime = record->timestamp;
          }
        }
      }
    }
    restoreDefaultConfig();
    sanitizeVisibleStreams();
    setTimeRange(startTime, endTime, firstDataRecordIndex);
    if (!imageReaders_.empty()) {
      if (lastMaxPerRow_ != 0) {
        relayout();
      } else {
        // Go over layout options, to find the one that matches the screen's aspect ratio the best
#if QT_VERSION < QT_VERSION_CHECK(5, 14, 0)
        QSize screenSize = QApplication::desktop()->screenGeometry(widget).size();
#else
        QSize screenSize = widget->screen()->size();
#endif
        float screenRatio = static_cast<float>(screenSize.width()) / screenSize.height();
        // screenRatio = 1.77; // 24" monitor simulation
        // screenRatio = 2560.0f / 1600; // 30" monitor simulation
        float bestFactor = numeric_limits<float>::max();
        int bestMaxHViews = 1;
        for (int maxHviews = 1; maxHviews <= static_cast<int>(imageReaders_.size()); maxHviews++) {
          int totalWidth = 0;
          int totalHeight = 0;
          int maxWidth = 0;
          int maxHeight = 0;
          int count = 0;
          for (auto& image : imageReaders_) {
            QSize imageSize = image.second->getWidget()->getImageSize();
            totalWidth += imageSize.width();
            maxHeight = max<int>(maxHeight, imageSize.height());
            if (++count % maxHviews == 0) {
              totalHeight += maxHeight;
              maxWidth = max<int>(maxWidth, totalWidth);
              maxHeight = 0;
              totalWidth = 0;
            }
          }
          int layoutHeight = totalHeight + maxHeight;
          int layoutWidth = max<int>(maxWidth, totalWidth);
          float ratio = static_cast<float>(layoutWidth) / layoutHeight;
          float newFactor = ratio > screenRatio ? ratio / screenRatio : screenRatio / ratio;
          // Give a boost if the last row has the same number of views as the previous rows
          if (count % maxHviews == 0) {
            newFactor = 1 + (newFactor - 1) * 0.2F;
          }
          if (newFactor < bestFactor) {
            bestFactor = newFactor;
            bestMaxHViews = maxHviews;
          }
        }
        layoutFrames(videoFrames, widget, bestMaxHViews);
      }
    }
  } else {
    setTimeRange(0, 0, 0);
  }
  time_.setTime(startTime_);
  nextRecord_ = firstDataRecordIndex_;
  lastReadRecords_.clear();
  emit mediaStateChanged(FileReaderState::Paused);
  return frames;
}

void FileReader::setOverlayColor(QColor color) {
  for (auto& image : imageReaders_) {
    image.second->getWidget()->setOverlayColor(color);
  }
}

void FileReader::setFontSize(int fontSize) {
  for (auto& image : imageReaders_) {
    image.second->getWidget()->setFontSize(fontSize);
  }
}

void FileReader::setSolidBackground(bool solid) {
  for (auto& image : imageReaders_) {
    image.second->getWidget()->setSolidBackground(solid);
  }
}

void FileReader::recordTypeChanged(const QString& type) {
  recordType_ = toEnum<Record::Type>(type.toStdString());
  for (auto& image : imageReaders_) {
    image.second->getWidget()->setTypeToShow(recordType_);
  }
}

void FileReader::setPosition(int position) {
  if (fileReader_ == nullptr) {
    return;
  }
  unique_lock<recursive_mutex> guard{mutex_};
  pause();
  IndexRecord::RecordInfo seekTime;
  seekTime.timestamp = positionToTime(position);
  time_.setTime(seekTime.timestamp);
  const auto& index = fileReader_->getIndex();
  nextRecord_ =
      static_cast<size_t>(lower_bound(index.begin(), index.end(), seekTime) - index.begin());
  cout << "Seek to " << FIXED_3 << seekTime.timestamp << ", record #" << nextRecord_ << "\n";
  waitEvent_.dispatchEvent(isSliderActive_ ? kReadCurrentFrameFast : kReadCurrentFrame);
}

void FileReader::sliderPressed() {
  isSliderActive_ = true;
  setBlankMode(false);
}

void FileReader::sliderReleased() {
  isSliderActive_ = false;
  waitEvent_.dispatchEvent(kReadCurrentFrame);
}

bool FileReader::eventFilter(QObject* obj, QEvent* event) {
  if (playerUi_ != nullptr && playerUi_->isActiveWindow() && event->type() == QEvent::KeyPress) {
    QKeyEvent* keyEvent = static_cast<QKeyEvent*>(event);
    switch (keyEvent->key()) {
      case Qt::Key_Space:
        if (state_ == FileReaderState::Playing) {
          pause();
        } else if (state_ == FileReaderState::Paused) {
          play();
        }
        return true;

      case Qt::Key_Left:
        if (state_ == FileReaderState::Paused) {
          previousFrame();
        }
        return true;

      case Qt::Key_Right:
        if (state_ == FileReaderState::Paused) {
          nextFrame();
        }
        return true;

      case Qt::Key_Home:
        if (state_ == FileReaderState::Paused || state_ == FileReaderState::Playing) {
          stop();
        }
        return true;

      case Qt::Key_Up:
        if (state_ == FileReaderState::Paused) {
          waitEvent_.dispatchEvent(kReadPreviousPage);
        }
        return true;

      case Qt::Key_Down:
        if (state_ == FileReaderState::Paused) {
          waitEvent_.dispatchEvent(kReadNextPage);
        }
        return true;

      case Qt::Key_PageUp:
        if (state_ == FileReaderState::Paused) {
          waitEvent_.dispatchEvent(kReadPreviousBigPage);
        }
        return true;

      case Qt::Key_PageDown:
        if (state_ == FileReaderState::Paused) {
          waitEvent_.dispatchEvent(kReadNextBigPage);
        }
        return true;

      case Qt::Key_Backspace:
        stop();
        return true;

      case Qt::Key_Plus:
        emit adjustSpeed(1);
        return true;

      case Qt::Key_Minus:
        emit adjustSpeed(-1);
        return true;

      case Qt::Key_Equal:
        emit adjustSpeed(0);
        return true;

      default:
        return QObject::eventFilter(obj, event);
    }
  }
  return QObject::eventFilter(obj, event);
}

void FileReader::stop() {
  nextRecord_ = firstDataRecordIndex_; // setState needs this to highlight buttons right
  if (state_ == FileReaderState::Playing) {
    setState(FileReaderState::Paused);
    unique_lock<recursive_mutex> guard{mutex_};
    if (fileReader_ != nullptr) {
      fileReader_->setCachingStrategy(CachingStrategy::Streaming);
    }
    lastReadRecords_.clear();
  } else {
    mediaStateChanged(state_);
  }
  time_.setTime(startTime_);
  nextRecord_ = firstDataRecordIndex_; // against races
  setBlankMode(true);
}

void FileReader::setBlankMode(bool blank) {
  for (auto& image : imageReaders_) {
    image.second->setBlankMode(blank);
  }
  unique_lock<recursive_mutex> guard{mutex_};
  lastReadRecords_.clear();
}

void FileReader::clearLayout(QLayout* layout, bool deleteWidgets) {
  QLayoutItem* child;
  while ((child = layout->takeAt(0)) != nullptr) {
    if (child->layout() != nullptr) {
      clearLayout(child->layout(), deleteWidgets);
    }
    if (deleteWidgets) {
      delete child->widget();
    }
    delete child;
  }
}

void FileReader::readFirstRecord(StreamId id, Record::Type recordType) {
  const IndexRecord::RecordInfo* record = fileReader_->getRecord(id, recordType, 0);
  if (record != nullptr && recordType == Record::Type::DATA) {
    const IndexRecord::RecordInfo* config =
        fileReader_->getRecord(id, Record::Type::CONFIGURATION, 0);
    if (config != nullptr && config->timestamp > record->timestamp) {
      record = fileReader_->getRecordByTime(id, recordType, config->timestamp);
    }
  }
  if (record != nullptr) {
    int error = fileReader_->readRecord(*record);
    if (error != 0) {
      setState(FileReaderState::Error);
      setErrorText(errorCodeToMessage(error));
    }
  }
}

void FileReader::setErrorText(const string& errorText) {
  statusStateChanged(errorText.empty() ? "" : "Error: " + errorText);
}

bool FileReader::isAudio(StreamId id) const {
  return audioReaders_.find(id) != audioReaders_.end();
}

bool FileReader::isVideo(StreamId id) const {
  return imageReaders_.find(id) != imageReaders_.end();
}

bool FileReader::isVisibleVideo(StreamId id) const {
  auto iter = imageReaders_.find(id);
  return iter != imageReaders_.end() && iter->second->isVisible();
}

string FileReader::getDeviceName(StreamId id) {
  string flavor = fileReader_->getFlavor(id);
  if (flavor.empty()) {
    return fmt::format("{} - {}", id.getNumericName(), id.getTypeName());
  }
  return fmt::format("{} - {}, {}", id.getNumericName(), id.getTypeName(), flavor);
}

void FileReader::sanitizeVisibleStreams(bool reset) {
  reset |= visibleStreams_.empty();
  set<StreamId> visibleIds;
  if (!reset) {
    for (auto id : visibleStreams_) {
      if (imageReaders_.find(id) == imageReaders_.end() ||
          visibleIds.find(id) != visibleIds.end()) {
        reset = true;
        break;
      }
      visibleIds.insert(id);
    }
  }
  if (reset) {
    visibleStreams_.clear();
    visibleStreams_.reserve(imageReaders_.size());
    for (const auto& r : imageReaders_) {
      visibleStreams_.push_back(r.first);
      r.second->setVisible(true);
    }
  } else {
    for (const auto& r : imageReaders_) {
      r.second->setVisible(visibleIds.find(r.first) != visibleIds.end());
    }
  }
}

static QString rotationName(const QString& streamName) {
  return streamName + "_rotation";
}

static QString flippedName(const QString& streamName) {
  return streamName + "_flipped";
}

QVariant FileReader::configurationAsVariant() {
  QJsonObject values;
  QJsonArray visibleStreams;
  for (auto id : visibleStreams_) {
    visibleStreams.append(QJsonValue(QString(fileToConfig_[id].getNumericName().c_str())));
  }
  values[kVisibleStreams] = visibleStreams;
  for (const auto& reader : imageReaders_) {
    QString streamName = fileToConfig_[reader.first].getNumericName().c_str();
    values[rotationName(streamName)] = reader.second->getWidget()->getRotation();
    values[flippedName(streamName)] = reader.second->getWidget()->getFlipped();
  }
  values[kLastMaxPerRow] = lastMaxPerRow_;
  return QJsonDocument(values).toVariant();
}

void FileReader::applyConfiguration(const QVariant& variant) {
  FlagKeeper disableRelayouts(layoutUpdatesEnabled_);
  QJsonDocument config(QJsonDocument::fromVariant(variant));
  map<StreamId, StreamId> settingsToFile;
  for (const auto& m : fileToConfig_) {
    settingsToFile[m.second] = m.first;
  }
  visibleStreams_.clear();
  const QJsonArray& visibleStreams = config[kVisibleStreams].toArray();
  visibleStreams_.reserve(visibleStreams.size());
  for (const auto& value : visibleStreams) {
    StreamId id = StreamId::fromNumericName(value.toString().toStdString());
    if (id.isValid() && settingsToFile.count(id) == 1) {
      visibleStreams_.push_back(settingsToFile[id]);
    }
  }
  for (const auto& reader : imageReaders_) {
    QString streamName = fileToConfig_[reader.first].getNumericName().c_str();
    reader.second->getWidget()->setRotation(config[rotationName(streamName)].toInt());
    reader.second->getWidget()->setFlipped(config[flippedName(streamName)].toBool());
  }
  lastMaxPerRow_ = config[kLastMaxPerRow].toInt();
}

void FileReader::play() {
  if (fileReader_ != nullptr && state_ == FileReaderState::Paused && !isAtEnd()) {
    setState(FileReaderState::Playing);
  }
}

double FileReader::getNextRecordDelay() {
  if (state_ == FileReaderState::Playing) {
    unique_lock<recursive_mutex> guard{mutex_};
    if (fileReader_ != nullptr && state_ == FileReaderState::Playing) {
      const auto& index = fileReader_->getIndex();
      // find the next record in a stream we care about
      while (nextRecord_ < index.size()) {
        if (isPlaying(index[nextRecord_].streamId)) {
          return index[nextRecord_].timestamp - time_.getTime();
        }
        nextRecord_++;
      }
    }
  }
  return 1; // just wait
}

void FileReader::playThreadActivity() {
  EventChannel::Event event;
  while (runThread_) {
    double delay = getNextRecordDelay();
    if (delay > 0) {
      if (waitEvent_.waitForEvent(event, delay) == EventChannel::Status::SUCCESS &&
          event.value != 0) {
        playAction(DispatchAction::fromBundle(event.value));
        continue;
      }
    }
    if (state_ == FileReaderState::Playing) {
      unique_lock<recursive_mutex> guard{mutex_};
      if (fileReader_ != nullptr && state_ == FileReaderState::Playing) {
        const auto& index = fileReader_->getIndex();
        const IndexRecord::RecordInfo& record = index[nextRecord_];
        double now = time_.getTime();
        bool mustPlay = isAudio(record.streamId) || record.recordType != Record::Type::DATA;
        if (mustPlay ||
            (now < record.timestamp + kMaxPlaybackAge && isVisibleVideo(record.streamId))) {
          if ((isLocalFile_ || fileReader_->isRecordAvailableOrPrefetch(record))) {
            readRecordIfNeeded(record, nextRecord_, false);
          }
          nextRecord_++;
        } else {
          while (++nextRecord_ < index.size()) {
            const IndexRecord::RecordInfo& next = index[nextRecord_];
            // skip irrelevant records & late images
            mustPlay = isAudio(next.streamId) || next.recordType != Record::Type::DATA;
            if (mustPlay || (now < next.timestamp && isVisibleVideo(next.streamId))) {
              break;
            }
          }
        }
      }
    }
  }
}

int FileReader::readRecordIfNeeded(
    const vrs::IndexRecord::RecordInfo& record,
    size_t recordIndex,
    bool log) {
  size_t& lastPlayed = lastReadRecords_[record.streamId];
  if (lastPlayed == recordIndex) {
    return 0;
  }
#ifdef Q_OS_WIN
  if (_fileno(stdout) == -2) { // _NO_CONSOLE_FILENO
    log = false;
  }
#endif
  if (log) {
    fmt::print(
        "Reading {} record #{}, {} - {}\n",
        toString(record.recordType),
        recordIndex,
        record.streamId.getNumericName(),
        record.streamId.getTypeName());
  }
  int error = fileReader_->readRecord(record);
  if (error != 0) {
    setState(FileReaderState::Error);
    setErrorText(errorCodeToMessage(error));
    lastPlayed = fileReader_->getIndex().size();
    return error;
  }
  lastPlayed = recordIndex;
  return 0;
}

void FileReader::playAction(DispatchAction action) {
  unique_lock<recursive_mutex> guard{mutex_};
  fileReader_->setCachingStrategy(CachingStrategy::StreamingBidirectional);
  set<size_t> frameSet;
  if (action.action == Action::ShowFrame) {
    if (getFrameSet(frameSet, nextRecord_, Direction::Backward)) {
      playFrameSet(frameSet, Seek::Accurate);
    }
  } else if (action.action == Action::ShowFrameFast) {
    if (getFrameSet(frameSet, nextRecord_, Direction::Backward)) {
      playFrameSet(frameSet, Seek::Fast);
    }
  } else if (action.action == Action::ChangeFrame) {
    setBlankMode(false);
    int32_t actionCount = action.frameCount;
    Direction direction = (action.frameCount > 0) ? Direction::Forward : Direction::Backward;
    if (direction == Direction::Backward) {
      actionCount = -actionCount + 1; // because we need to skip the last set read
    }
    while (actionCount-- > 0 && getFrameSet(frameSet, nextRecord_, direction)) {
      nextRecord_ = direction == Direction::Forward ? *frameSet.rbegin() + 1 : *frameSet.begin();
    }
    const auto& index = fileReader_->getIndex();
    if (frameSet.empty()) {
      if (direction == Direction::Forward) {
        // Read last possible set
        nextRecord_ = index.size();
        getFrameSet(frameSet, nextRecord_, Direction::Backward);
      } else {
        stop();
      }
    } else {
      // even going backwards, the next record is after what we read
      nextRecord_ = *frameSet.rbegin() + 1;
    }
    if (!frameSet.empty()) {
      playFrameSet(frameSet, Seek::Accurate);
      size_t lastPlayed = *frameSet.rbegin();
      time_.setTime(index[lastPlayed].timestamp);
    }
  }
  mediaStateChanged(state_);
}

// Helper to prefetch a frameset, making sure we cancel the sequence on exit
class Prefetcher {
 public:
  Prefetcher(RecordFileReader& reader, const set<size_t>& frameSet, bool isLocalFile)
      : reader_{reader} {
    if (!isLocalFile) {
      const auto& index = reader_.getIndex();
      records_.reserve(frameSet.size());
      for (size_t frameIndex : frameSet) {
        records_.emplace_back(&index[frameIndex]);
      }
      reader_.prefetchRecordSequence(records_);
    }
  }
  ~Prefetcher() {
    if (!records_.empty()) {
      records_.clear();
      reader_.prefetchRecordSequence(records_);
    }
  }

 private:
  RecordFileReader& reader_;
  vector<const IndexRecord::RecordInfo*> records_;
};

bool FileReader::playFrameSet(const set<size_t>& frameSet, Seek strategy) {
  Prefetcher prefetcher(*fileReader_, frameSet, isLocalFile_);
  const auto& index = fileReader_->getIndex();
  for (size_t frame : frameSet) {
    const IndexRecord::RecordInfo& record = index[frame];
    if (strategy == Seek::Fast && !fileReader_->isRecordAvailableOrPrefetch(record)) {
      continue;
    }
    if (readRecordIfNeeded(record, frame, true) != 0) {
      return false;
    }
    FramePlayer* framePlayer = imageReaders_[record.streamId].get();
    if (framePlayer != nullptr && (isLocalFile_ || strategy == Seek::Accurate)) {
      int result = framePlayer->readMissingFrames(*fileReader_, record, strategy == Seek::Accurate);
      if (result != SUCCESS) {
        setState(FileReaderState::Error);
        if (result == FAILURE) {
          setErrorText("Can't find keyframe record for " + record.streamId.getName());
        } else {
          setErrorText(errorCodeToMessage(result));
        }
      }
    }
  }
  return true;
}

bool FileReader::getFrameSet(set<size_t>& outSet, size_t start, Direction direction) const {
  outSet.clear();
  set<StreamId> ids;
  const auto& index = fileReader_->getIndex();
  size_t nextFrame = start;
  while (true) {
    if (direction == Direction::Backward) {
      if (nextFrame == 0) {
        break;
      }
      nextFrame--;
    }
    if (nextFrame >= index.size()) {
      break;
    }
    const IndexRecord::RecordInfo& record = index[nextFrame];
    StreamId id = index[nextFrame].streamId;
    if (isVisibleVideo(id)) {
      if (record.recordType == Record::Type::DATA && !ids.insert(id).second) {
        break; // we've seen that device already: stop the set
      }
      outSet.insert(nextFrame);
    }
    if (direction == Direction::Forward) {
      nextFrame++;
    }
  }
  return !outSet.empty();
}

void FileReader::pause() {
  if (state_ == FileReaderState::Playing) {
    setState(FileReaderState::Paused);
    waitEvent_.dispatchEvent(kReadCurrentFrame);
  }
}

void FileReader::nextFrame() {
  setBlankMode(false);
  waitEvent_.dispatchEvent(kReadNextFrame);
}

void FileReader::previousFrame() {
  setBlankMode(false);
  waitEvent_.dispatchEvent(kReadPreviousFrame);
}

void FileReader::updatePosition() {
  unique_lock<recursive_mutex> guard{mutex_};
  if (fileReader_ == nullptr || (imageReaders_.empty() && audioReaders_.empty())) {
    lastShownTime_ = std::numeric_limits<double>::quiet_NaN();
    timeChanged(0, 0);
  } else {
    if (state_ == FileReaderState::Playing && nextRecord_ >= fileReader_->getIndex().size()) {
      cout << "End of file reached\n";
      pause();
    }
    double time = time_.getTime();
    if (time != lastShownTime_) {
      lastShownTime_ = time;
      timeChanged(time, timeToPosition(time));
    }
  }
}

void FileReader::setPlaybackSpeed(double speed) {
  if (state_ == FileReaderState::Playing) {
    setState(FileReaderState::Paused);
    VideoTime::setPlaybackSpeed(speed);
    setState(FileReaderState::Playing);
  } else {
    VideoTime::setPlaybackSpeed(speed);
  }
}

void FileReader::enableAllStreams() {
  FlagKeeper disableRelayouts(layoutUpdatesEnabled_);
  for (const auto& reader : imageReaders_) {
    if (!reader.second->isVisible()) {
      reader.second->setVisible(!reader.second->isVisible());
      visibleStreams_.push_back(reader.first);
    }
  }
  relayout();
  waitEvent_.dispatchEvent(kReadCurrentFrame);
}

void FileReader::resetOrientation() {
  FlagKeeper disableRelayouts(layoutUpdatesEnabled_);
  for (const auto& reader : imageReaders_) {
    reader.second->getWidget()->resetOrientation();
  }
  relayout();
}

void FileReader::toggleVisibleStreams() {
  FlagKeeper disableRelayouts(layoutUpdatesEnabled_);
  visibleStreams_.clear();
  for (const auto& reader : imageReaders_) {
    reader.second->setVisible(!reader.second->isVisible());
    if (reader.second->isVisible()) {
      visibleStreams_.push_back(reader.first);
    }
  }
  relayout();
  waitEvent_.dispatchEvent(kReadCurrentFrame);
}

void FileReader::disableStream(StreamId id) {
  assert(imageReaders_.find(id) != imageReaders_.end());
  for (auto iter = visibleStreams_.begin(); iter != visibleStreams_.end(); ++iter) {
    if (*iter == id) {
      visibleStreams_.erase(iter);
      imageReaders_[id]->setVisible(false);
      break;
    }
  }
  relayout();
}

void FileReader::moveStream(StreamId id, bool beforeNotAfter) {
  if (visibleStreams_.size() < 2) {
    return;
  }
  for (size_t p = 0; p < visibleStreams_.size(); p++) {
    if (visibleStreams_[p] == id) {
      visibleStreams_.erase(visibleStreams_.begin() + p);
      if (beforeNotAfter) {
        if (p == 0) {
          p = visibleStreams_.size();
        } else {
          p--;
        }
      } else {
        if (p >= visibleStreams_.size()) {
          p = 0;
        } else {
          p++;
        }
      }
      visibleStreams_.insert(visibleStreams_.begin() + p, id);
      break;
    }
  }
  relayout();
}

void FileReader::saveFrame(vrs::StreamId id) {
  pause();
  unique_lock<recursive_mutex> guard{mutex_};
  if (!fileReader_ || lastReadRecords_.find(id) == lastReadRecords_.end()) {
    return;
  }
  FramePlayer& framePlayer = *imageReaders_[id];
  size_t frameIndex = lastReadRecords_[id];
  const auto& record = fileReader_->getIndex()[frameIndex];
  string filename = framePlayer.getFrameName(frameIndex, record);
  QString path = QFileDialog::getSaveFileName(
      playerUi_, "Save Frame As...", getInitialSaveLocation() + '/' + filename.c_str());
  if (path.isEmpty()) {
    return;
  }
  lastSaveLocation_ = QFileInfo(path).absoluteDir().absolutePath();
  if (!framePlayer.saveFrameNowOrOnNextRead(path.toStdString())) {
    fileReader_->readRecord(record);
  }
}

void FileReader::saveFrames() {
  if (state_ == FileReaderState::Playing) {
    pause();
  } else if (state_ != FileReaderState::Paused) {
    return;
  }
  unique_lock<recursive_mutex> guard{mutex_};
  if (!fileReader_ || lastReadRecords_.empty()) {
    return;
  }
  QString dir = QFileDialog::getExistingDirectory(
      playerUi_,
      "Save Frames At...",
      getInitialSaveLocation(),
      QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
  if (dir.isEmpty()) {
    return;
  }
  lastSaveLocation_ = dir;
  for (auto id : visibleStreams_) {
    auto iter = lastReadRecords_.find(id);
    if (iter == lastReadRecords_.end()) {
      continue;
    }
    FramePlayer& framePlayer = *imageReaders_[id];
    size_t frameIndex = iter->second;
    const auto& record = fileReader_->getIndex()[frameIndex];
    string filename = framePlayer.getFrameName(frameIndex, record);
    QString path = dir + '/' + filename.c_str();
    if (!framePlayer.saveFrameNowOrOnNextRead(path.toStdString())) {
      fileReader_->readRecord(record);
    }
  }
}

QString FileReader::getInitialSaveLocation() {
  QFileInfo fileInfo(lastSaveLocation_);
  if (fileInfo.exists() && fileInfo.isDir()) {
    return lastSaveLocation_;
  }
  return QStandardPaths::standardLocations(QStandardPaths::DocumentsLocation)
      .value(0, QDir::homePath());
}

void FileReader::savePreset(const QString& preset) {
  layoutPresets_[preset] = configurationAsVariant();
  layoutConfigChanged();
}

void FileReader::recallPreset(const QString& preset) {
  if (layoutPresets_.contains(preset)) {
    applyConfiguration(layoutPresets_[preset]);
    sanitizeVisibleStreams();
    relayout();
    waitEvent_.dispatchEvent(kReadCurrentFrame);
  }
}

void FileReader::deletePreset(const QString& preset) {
  layoutPresets_.erase(layoutPresets_.find(preset));
  layoutConfigChanged();
}

void FileReader::relayout() {
  if (videoFrames_ != nullptr) {
    layoutFrames(videoFrames_, playerUi_, lastMaxPerRow_);
  }
}

void FileReader::layoutFrames(QVBoxLayout* videoFrames, QWidget* parent, int maxPerRow) {
  clearLayout(videoFrames, false);
  maxPerRow = max<int>(maxPerRow, 1);
  maxPerRow = min<int>(maxPerRow, static_cast<int>(imageReaders_.size()));
  lastMaxPerRow_ = maxPerRow;
  QHBoxLayout* innerHlayout = nullptr;
  int count = 0;
  for (auto id : visibleStreams_) {
    if (innerHlayout == nullptr) {
      innerHlayout = new QHBoxLayout();
      videoFrames->addLayout(innerHlayout);
    }
    innerHlayout->addWidget(imageReaders_[id]->getWidget());
    if (++count % maxPerRow == 0) {
      innerHlayout = nullptr;
    }
  }
  layoutConfigChanged();
}

void FileReader::layoutConfigChanged() {
  emit updateLayoutMenu(
      getImageCount(),
      visibleStreams_.size(),
      lastMaxPerRow_,
      layoutPresets_,
      configurationAsVariant());
}

void FileReader::restoreDefaultConfig() {
  if (layoutPresets_.contains(kDefaultPreset)) {
    applyConfiguration(layoutPresets_[kDefaultPreset]);
    return;
  }
  QVariant currentConfig = fileConfig_->value(kLastConfiguration);
  if (!currentConfig.isValid()) {
    // restore previous settings
    for (const auto& reader : imageReaders_) {
      StreamId configId{fileToConfig_[reader.first]};
      QString name = configId.getNumericName().c_str();
      reader.second->getWidget()->setRotation(
          fileConfig_->value(rotationName(name), int(0)).toInt());
      reader.second->getWidget()->setFlipped(fileConfig_->value(flippedName(name), false).toBool());
    }
    lastMaxPerRow_ = fileConfig_->value(kLastMaxPerRow, int(0)).toInt();
  } else {
    applyConfiguration(currentConfig);
  }
}

void FileReader::saveConfiguration() {
  if (fileReader_ == nullptr || fileConfig_ == nullptr) {
    return;
  }
  fileConfig_->clear();
  fileConfig_->setValue(kLastConfiguration, configurationAsVariant());
  fileConfig_->setValue(kLayoutPresets, layoutPresets_);
  fileConfig_->sync();
  //  QJsonDocument configJson(QJsonDocument::fromVariant(fileConfig_->value(kLastConfiguration)));
  //  QByteArray config = configJson.toJson();
  //  XR_LOGI("Saved config: {}", string(config.constData(), config.size()));
}

void FileReader::loadConfiguration() {
  if (fileReader_ == nullptr) {
    fileConfig_.reset();
    return;
  }
  fileToConfig_.clear();
  map<RecordableTypeId, uint16_t> instanceCounters;
  for (auto id : fileReader_->getStreams()) {
    uint16_t instance = ++instanceCounters[id.getTypeId()];
    StreamId configId(id.getTypeId(), instance);
    fileToConfig_[id] = configId;
  }
  stringstream ss;
  for (const auto& iter : instanceCounters) {
    ss << StreamId(iter.first, iter.second).getNumericName() << '_';
  }
  fileConfig_ = make_unique<QSettings>("VRSplayer", ss.str().c_str());
  layoutPresets_ = fileConfig_->value(kLayoutPresets).toMap();
  //  for (auto key : layoutPresets_.keys()) {
  //    QJsonDocument jdoc = QJsonDocument::fromVariant(layoutPresets_[key]);
  //    QByteArray config = jdoc.toJson();
  //    XR_LOGI("\nPreset {}: {}", key.toStdString(), string(config.constData(), config.size()));
  //  }
}

static int rawTimeToPosition(double time) {
  return static_cast<int>(time * 10000);
}

static double rawPositionToTime(int position) {
  return static_cast<double>(position) / 10000;
}

int FileReader::timeToPosition(double time) const {
  if (endTime_ <= startTime_ || time <= startTime_) {
    return 0;
  }
  if (time >= endTime_) {
    return rawTimeToPosition(endTime_ - startTime_);
  }
  return rawTimeToPosition(time - startTime_);
}

double FileReader::positionToTime(int position) const {
  return endTime_ <= startTime_ ? startTime_ : startTime_ + rawPositionToTime(position);
}

void FileReader::setState(FileReaderState newState) {
  if (newState == FileReaderState::Playing && fileReader_ == nullptr) {
    return; // no file: deny state change request!
  }
  {
    unique_lock<recursive_mutex> guard{mutex_};
    state_ = newState;
    if (state_ == FileReaderState::Playing) {
      fileReader_->setCachingStrategy(CachingStrategy::Streaming);
      setBlankMode(false);
      const auto& index = fileReader_->getIndex();
      if (nextRecord_ < index.size()) {
        time_.setTime(index[nextRecord_].timestamp);
      }
      time_.start();
      waitEvent_.dispatchEvent();
    } else {
      time_.pause();
    }
  }
  cout << "Video state: " << FileReaderStateConverter::toString(newState) << "\n";
  mediaStateChanged(state_);
}

void FileReader::setTimeRange(double start, double end, uint32_t firstDataRecordIndex) {
  startTime_ = start;
  endTime_ = end;
  firstDataRecordIndex_ = firstDataRecordIndex;
  cout << "Start: " << helpers::humanReadableTimestamp(start)
       << ", end: " << helpers::humanReadableTimestamp(end) << "\n";
  durationChanged(start, end, rawTimeToPosition(endTime_ - startTime_));
}

} // namespace vrsp
