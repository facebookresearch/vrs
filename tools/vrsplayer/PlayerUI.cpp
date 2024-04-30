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

#include "PlayerUI.h"

#include <cctype>

#include <fmt/format.h>

#include <qmessagebox.h>
#include <QtWidgets>

#define DEFAULT_LOG_CHANNEL "PlayerUI"
#include <logging/Verify.h>

#include "VideoTime.h"

#include <vrs/helpers/Strings.h>

namespace vrsp {

using namespace std;

namespace {

const double kDefaultScreenOccupationRatio = 0.8; // use most of the screen (arbitrary)

const QString kLastFilePathSetting("last_file_path");
const QString kLastRecordTypeSetting("last_record_type");
const QString kOverlayColorSetting("overlay_color");
const QString kOverlayFontSizeSetting("overlay_font_size");
const QString kOverlaySolidBackgroundSetting("overlay_solid_background");

const QString kDefaultSpeed("1x");

const vector<QString> kRecordTypeLabels = {
    "Hide",
    Record::typeName(Record::Type::TAGS),
    Record::typeName(Record::Type::CONFIGURATION),
    Record::typeName(Record::Type::STATE),
    Record::typeName(Record::Type::DATA),
};

const vector<pair<QString, double>> kSpeeds = {
    {"0.125x", 0.125},
    {"0.25x", 0.25},
    {"0.50x", 0.50},
    {"0.75x", 0.75},
    {kDefaultSpeed, 1.00},
    {"1.25x", 1.25},
    {"1.50x", 1.50},
    {"2.00x", 2.00},
    {"3.00x", 3.00},
    {"4.00x", 4.00},
    {"6.00x", 6.00},
    {"8.00x", 8.00}};

// Slider that jumps to the position, when you click the slider's bar but not the thumb itself
class JumpSlider : public QSlider {
 public:
  JumpSlider() : QSlider(Qt::Horizontal) {}

 protected:
  void mousePressEvent(QMouseEvent* event) override {
    QStyleOptionSlider opt;
    initStyleOption(&opt);
    QRect sr = style()->subControlRect(QStyle::CC_Slider, &opt, QStyle::SC_SliderHandle, this);
    if (event->button() == Qt::LeftButton && !sr.contains(event->pos())) {
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
      int newVal = minimum() + ((maximum() - minimum()) * event->x()) / width();
#else
      int newVal = minimum() + ((maximum() - minimum()) * event->position().x()) / width();
#endif
      event->accept();
      sliderMoved(newVal);
    }
    QSlider::mousePressEvent(event);
  }
};

} // namespace

PlayerUI::PlayerUI(PlayerWindow* playerWindow)
    : QWidget(nullptr),
      playerWindow_{playerWindow},
      time_{nullptr},
      positionSlider_{nullptr},
      statusLabel_{nullptr} {
  fileReader_.setPlayerUi(this);
  connect(
      this,
      &PlayerUI::selectedAudioChannelsChanged,
      &fileReader_,
      &FileReader::selectedAudioChannelsChanged);
  VideoTime::setPlayerUI(this);
  QAbstractButton* openPathButton = new QPushButton(tr("Open..."));
  connect(openPathButton, &QAbstractButton::clicked, this, &PlayerUI::openPathChooser);
  QAbstractButton* openButton = new QPushButton(tr("Select..."));
  connect(openButton, &QAbstractButton::clicked, this, &PlayerUI::openFileChooser);

  backwardButton_ = new QPushButton;
  backwardButton_->setEnabled(false);
  backwardButton_->setIcon(style()->standardIcon(QStyle::SP_MediaSkipBackward));
  connect(backwardButton_, &QAbstractButton::clicked, this, &PlayerUI::backwardPressed);

  playPauseButton_ = new QPushButton;
  playPauseButton_->setEnabled(false);
  playPauseButton_->setIcon(style()->standardIcon(QStyle::SP_MediaPlay));
  connect(playPauseButton_, &QAbstractButton::clicked, this, &PlayerUI::playPausePressed);

  stopButton_ = new QPushButton;
  stopButton_->setEnabled(false);
  stopButton_->setIcon(style()->standardIcon(QStyle::SP_MediaStop));
  connect(stopButton_, &QAbstractButton::clicked, this, &PlayerUI::stopPressed);

  forwardButton_ = new QPushButton;
  forwardButton_->setEnabled(false);
  forwardButton_->setIcon(style()->standardIcon(QStyle::SP_MediaSkipForward));
  connect(forwardButton_, &QAbstractButton::clicked, this, &PlayerUI::forwardPressed);

  time_ = new QLabel;
  time_->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding);
  time_->setText("0.000");
  time_->setMinimumWidth(80);

  positionSlider_ = new JumpSlider();
  positionSlider_->setRange(0, 0);
  connect(
      positionSlider_, &QAbstractSlider::sliderPressed, &fileReader_, &FileReader::sliderPressed);
  connect(positionSlider_, &QAbstractSlider::sliderMoved, &fileReader_, &FileReader::setPosition);
  connect(
      positionSlider_, &QAbstractSlider::sliderReleased, &fileReader_, &FileReader::sliderReleased);

  statusLabel_ = new QLabel;
  statusLabel_->setWordWrap(true);
  statusLabel_->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding);

  videoFrames_ = new QVBoxLayout;

  QComboBox* overlayControl = new QComboBox;
  for (const auto& recordType : kRecordTypeLabels) {
    overlayControl->addItem(recordType);
  }

  speedControl_ = new QComboBox;
  for (const auto& speed : kSpeeds) {
    speedControl_->addItem(speed.first);
  }

  QBoxLayout* controlLayout = new QHBoxLayout;
  controlLayout->addWidget(overlayControl);
  controlLayout->addStretch();

  controlLayout->addWidget(speedControl_);
  controlLayout->addWidget(backwardButton_);
  controlLayout->addWidget(stopButton_);
  controlLayout->addWidget(playPauseButton_);
  controlLayout->addWidget(forwardButton_);
  controlLayout->addWidget(time_);

  controlLayout->addWidget(openPathButton);
  controlLayout->addWidget(openButton);

  QBoxLayout* layout = new QVBoxLayout;
  layout->addStretch();
  layout->addLayout(videoFrames_);
  layout->addLayout(controlLayout);
  layout->addWidget(positionSlider_);
  layout->addWidget(statusLabel_);
  layout->addStretch();

  setLayout(layout);

  connect(&fileReader_, &FileReader::mediaStateChanged, this, &PlayerUI::mediaStateChanged);
  connect(&fileReader_, &FileReader::timeChanged, this, &PlayerUI::timeChanged);
  connect(&fileReader_, &FileReader::durationChanged, this, &PlayerUI::durationChanged);
  connect(&fileReader_, &FileReader::statusStateChanged, this, &PlayerUI::setStatusText);
  connect(
      overlayControl,
      static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
      [this](int index) { this->recordTypeChanged(index); });
  connect(
      speedControl_,
      static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
      [this](int index) { this->speedControlChanged(index); });
  connect(&fileReader_, &FileReader::adjustSpeed, this, &PlayerUI::adjustSpeed);

  setFocusPolicy(Qt::StrongFocus);

  // Restore the last selected record type (default: none);
  QVariant lastRecordType =
      settings_.value(kLastRecordTypeSetting, QVariant(Record::typeName(Record::Type::UNDEFINED)));
  QString typeName = lastRecordType.toString();
  overlayControl->setCurrentText(typeName);
  speedControl_->setCurrentText(kDefaultSpeed);
  fileReader_.recordTypeChanged(typeName);

  // Check which image frames need to be updated, rather than have them call update()
  // in the decoding thread...
  checkForUpdatesTimer_.setSingleShot(false);
  checkForUpdatesTimer_.setTimerType(Qt::TimerType::PreciseTimer);
  connect(&checkForUpdatesTimer_, &QTimer::timeout, this, &PlayerUI::checkForUpdates);
  checkForUpdatesTimer_.start(1000 / 90); // delay between update checks
}

void PlayerUI::openFileChooser() {
  setStatusText({});
  QString dialogStartDir;
  QFileInfo fileInfo(settings_.value(kLastFilePathSetting).toString());
  if (fileInfo.exists()) {
    dialogStartDir = fileInfo.isFile() ? fileInfo.absolutePath() : fileInfo.absoluteFilePath();
  } else {
    dialogStartDir = QStandardPaths::standardLocations(QStandardPaths::DocumentsLocation)
                         .value(0, QDir::homePath());
  }
  fileReader_.stop();
  QFileDialog fileDialog(window());
  fileDialog.setAcceptMode(QFileDialog::AcceptOpen);
  fileDialog.setWindowTitle(tr("Open VRS File"));
  fileDialog.setNameFilter("VRS files (*.vrs *.vrs-0)");
  fileDialog.setDirectory(dialogStartDir);
  if (fileDialog.exec() == QDialog::Accepted) {
    openPath(fileDialog.selectedUrls().constFirst().toLocalFile());
  }
}

void PlayerUI::openPathChooser() {
  setStatusText({});
  fileReader_.stop();
  bool ok{};
  QString text = QInputDialog::getText(
                     nullptr,
                     "Open VRS File",
                     "Path or URI:",
                     QLineEdit::Normal,
                     "",
                     &ok,
                     Qt::WindowFlags(),
                     Qt::ImhMultiLine)
                     .trimmed();
  if (ok && !text.isEmpty()) {
    openPath(text);
  }
}

void PlayerUI::saveFrames() {
  fileReader_.saveFrames();
}

void PlayerUI::openLastFile() {
  QString lastPath = settings_.value(kLastFilePathSetting).toString();
  // Clear setting to avoid crash loops
  settings_.setValue(kLastFilePathSetting, {});
  if (!lastPath.isEmpty()) {
    openPath(lastPath);
  } else {
    openFileChooser();
  }
}

void PlayerUI::stopPressed() {
  fileReader_.stop();
}

void PlayerUI::openPath(const QString& path) {
  QString preparedPath = pathPreparer_ ? pathPreparer_(path) : path;
  setStatusText({});
  window()->setWindowTitle({});
  frames_.clear();
  frames_ = fileReader_.openFile(preparedPath, videoFrames_, window());
  settings_.setValue(kLastFilePathSetting, frames_.empty() ? "" : preparedPath);
  QVariant overlayColor = settings_.value(kOverlayColorSetting);
  if (overlayColor.isValid()) {
    setOverlayColor(overlayColor.value<QColor>());
  }
  QVariant fontSize = settings_.value(kOverlayFontSizeSetting);
  if (fontSize.isValid()) {
    fontSize_ = fontSize.value<int>();
    adjustOverlayFontSize(0);
  }
  QVariant solidBackground = settings_.value(kOverlaySolidBackgroundSetting);
  if (solidBackground.isValid()) {
    setSolidBackground(solidBackground.value<bool>());
  }
  resizeIfNecessary();
}

void PlayerUI::resizeToDefault() {
#if QT_VERSION < QT_VERSION_CHECK(5, 14, 0)
  QSize fullSize = QApplication::desktop()->screenGeometry(this).size();
#else
  QSize fullSize = screen()->size();
#endif
  window()->setMaximumSize(fullSize);
  // resize to default size & center on the current screen
  window()->resize(fullSize * kDefaultScreenOccupationRatio);
  window()->setGeometry(QStyle::alignedRect(
      Qt::LeftToRight,
      Qt::AlignCenter,
      window()->size(),
#if QT_VERSION < QT_VERSION_CHECK(5, 14, 0)
      qApp->desktop()->availableGeometry(window())));
#else
      screen()->geometry()));
#endif
}

void PlayerUI::resizeIfNecessary(bool maxSizeOnly) {
#if QT_VERSION < QT_VERSION_CHECK(5, 14, 0)
  QRect screenRect = QApplication::desktop()->screenGeometry(this);
#else
  QRect screenRect = screen()->geometry();
#endif
  QRect windowRect = geometry();
  QRect windowInScreen(mapToGlobal(windowRect.topLeft()), mapToGlobal(windowRect.bottomRight()));
  if (screenRect.contains(windowInScreen)) {
    window()->setMaximumSize(screenRect.size());
  } else if (!maxSizeOnly) {
    resizeToDefault();
  }
}

void PlayerUI::playPausePressed() {
  switch (fileReader_.getState()) {
    case FileReaderState::Playing:
      fileReader_.pause();
      break;
    case FileReaderState::Paused:
      fileReader_.play();
      break;
    default:
      break;
  }
}

void PlayerUI::backwardPressed() {
  fileReader_.previousFrame();
}

void PlayerUI::forwardPressed() {
  fileReader_.nextFrame();
}

void PlayerUI::checkForUpdates() {
  for (auto frame : frames_) {
    if (frame->getAndClearNeedsUpdate()) {
      frame->update();
    }
  }
}

void PlayerUI::relayout(int framesPerRow) {
  fileReader_.layoutFrames(videoFrames_, window(), framesPerRow);
  resizeIfNecessary();
}

void PlayerUI::resetOrientation() {
  emit fileReader_.resetOrientation();
  resizeIfNecessary();
}

void PlayerUI::showAllStreams() {
  emit fileReader_.enableAllStreams();
  resizeIfNecessary();
}

void PlayerUI::toggleVisibleStreams() {
  emit fileReader_.toggleVisibleStreams();
  resizeIfNecessary();
}

void PlayerUI::savePreset() {
  bool ok{};
  QString text = QInputDialog::getText(
                     nullptr,
                     "Save Preset",
                     "Preset Name:",
                     QLineEdit::Normal,
                     "",
                     &ok,
                     Qt::WindowFlags(),
                     Qt::ImhPreferLatin)
                     .trimmed();
  if (ok && !text.isEmpty()) {
    fileReader_.savePreset(text);
  }
}

void PlayerUI::recallPreset(const QString& preset) {
  fileReader_.recallPreset(preset);
  resizeIfNecessary();
}

void PlayerUI::deletePreset(const QString& preset) {
  fileReader_.deletePreset(preset);
}

void PlayerUI::reportError(const QString& errorTitle, const QString& errorMessage) {
  emit fileReader_.stop();
  QMessageBox messageBox(QMessageBox::Critical, "", errorMessage, QMessageBox::Ok);
  messageBox.QDialog::setWindowTitle(errorTitle); // Backdoor for MacOS
  messageBox.setText(errorMessage);
  messageBox.exec();
}

void PlayerUI::setOverlayColor(QColor color) {
  overlayColor_ = color;
  fileReader_.setOverlayColor(overlayColor_);
  settings_.setValue(kOverlayColorSetting, overlayColor_);
  overlaySettingChanged();
}

void PlayerUI::adjustOverlayFontSize(int sizeChange) {
  const int kDefaultFontSize = 14;
  const int kMinFontSize = 7;
  const int kMaxFontSize = 50;
  fontSize_ += sizeChange;
  if (fontSize_ == 0) {
    fontSize_ = kDefaultFontSize;
  }
  fontSize_ = max<int>(fontSize_, kMinFontSize);
  fontSize_ = min<int>(fontSize_, kMaxFontSize);
  fileReader_.setFontSize(fontSize_);
  settings_.setValue(kOverlayFontSizeSetting, fontSize_);
  overlaySettingChanged();
}

void PlayerUI::setSolidBackground(bool solid) {
  solidBackground_ = solid;
  fileReader_.setSolidBackground(solidBackground_);
  settings_.setValue(kOverlaySolidBackgroundSetting, solidBackground_);
  overlaySettingChanged();
}

void PlayerUI::adjustSpeed(int change) {
  if (change == 0) {
    speedControl_->setCurrentText(kDefaultSpeed);
  } else {
    int count = speedControl_->count();
    int currentSpeed = speedControl_->currentIndex();
    int newSpeed = currentSpeed + change;
    if (newSpeed >= 0 && newSpeed < count) {
      speedControl_->setCurrentIndex(newSpeed);
    }
  }
}

void PlayerUI::mediaStateChanged(FileReaderState state) {
  switch (state) {
    case FileReaderState::NoMedia:
      stopButton_->setEnabled(false);
      playPauseButton_->setIcon(style()->standardIcon(QStyle::SP_MediaPlay));
      playPauseButton_->setEnabled(false);
      backwardButton_->setEnabled(false);
      forwardButton_->setEnabled(false);
      break;
    case FileReaderState::Paused:
      stopButton_->setEnabled(!fileReader_.isAtBegin());
      playPauseButton_->setIcon(style()->standardIcon(QStyle::SP_MediaPlay));
      playPauseButton_->setEnabled(!fileReader_.isAtEnd());
      backwardButton_->setEnabled(!fileReader_.isAtBegin());
      forwardButton_->setEnabled(!fileReader_.isAtEnd());
      break;
    case FileReaderState::Playing:
      stopButton_->setEnabled(true);
      playPauseButton_->setIcon(style()->standardIcon(QStyle::SP_MediaPause));
      playPauseButton_->setEnabled(true);
      backwardButton_->setEnabled(false);
      forwardButton_->setEnabled(false);
      break;
    default:
      stopButton_->setEnabled(false);
      playPauseButton_->setIcon(style()->standardIcon(QStyle::SP_MediaPlay));
      playPauseButton_->setEnabled(false);
      backwardButton_->setEnabled(false);
      forwardButton_->setEnabled(false);
      break;
  }
}

void PlayerUI::timeChanged(double time, int position) {
  time_->setText(fmt::format("{:.3f}", time).c_str());
  positionSlider_->setValue(position);
}

void PlayerUI::recordTypeChanged(int index) {
  if (XR_VERIFY(index >= 0 && index < kRecordTypeLabels.size())) {
    const QString& typeName = kRecordTypeLabels[index];
    settings_.setValue(kLastRecordTypeSetting, typeName);
    fileReader_.recordTypeChanged(typeName);
  }
}

void PlayerUI::speedControlChanged(int index) {
  double speed = 1;
  if (XR_VERIFY(index >= 0 && index < kSpeeds.size())) {
    speed = kSpeeds[index].second;
  }
  fileReader_.setPlaybackSpeed(speed);
}

void PlayerUI::durationChanged(double start, double end, int range) {
  positionSlider_->setRange(0, range);
  string tip = fmt::format(
      "Start: {:.3f}\nEnd: {:.3f}\nDuration: {}",
      start,
      end,
      vrs::helpers::humanReadableDuration(end - start).c_str());
  time_->setToolTip(QString::fromUtf8(tip.c_str()));
}

void PlayerUI::setStatusText(const string& statusText) {
  if (!statusText.empty() && isalnum(*statusText.rbegin())) {
    statusLabel_->setText((statusText + '.').c_str());
  } else {
    statusLabel_->setText(statusText.c_str());
  }
}

bool PlayerUI::eventFilter(QObject* obj, QEvent* event) {
  return fileReader_.eventFilter(obj, event);
}

} // namespace vrsp
