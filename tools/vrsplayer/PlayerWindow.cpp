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

#include "PlayerWindow.h"

#include <qaction.h>
#include <qmenubar.h>
#include <qmessagebox.h>

#include <vrs/FileHandler.h>
#include <vrs/helpers/EnumStringConverter.h>
#include <vrs/os/Platform.h>

using namespace vrsp;
using namespace std;

namespace {
const char* sAudioModeNames[] = {"mono", "stereo-auto", "stereo-manual"};

struct AudioModeConverter : public EnumStringConverter<
                                AudioMode,
                                sAudioModeNames,
                                COUNT_OF(sAudioModeNames),
                                AudioMode::autoStereo,
                                AudioMode::autoStereo,
                                true> {};
} // namespace

inline QKeySequence shortcut(int keyA, int keyB, int keyC = 0) {
  return {keyA + keyB + keyC};
}

PlayerWindow::PlayerWindow(QApplication& app) : QMainWindow(nullptr), player_{this} {
  setCentralWidget(&player_);
  app.installEventFilter(&player_);
  createMenus();
  connect(
      &player_.getFileReader(),
      &FileReader::updateLayoutMenu,
      this,
      &PlayerWindow::updateLayoutAndPresetMenu);
  connect(&player_, &PlayerUI ::overlaySettingChanged, this, &PlayerWindow::updateTextOverlayMenu);
  setWindowFlags(windowFlags() | Qt::CustomizeWindowHint | Qt::WindowMinMaxButtonsHint);
}

int PlayerWindow::processCommandLine(QCommandLineParser& parser) {
  if (!parser.positionalArguments().isEmpty()) {
    const QString& arg = parser.positionalArguments().constFirst();
    vrs::FileSpec fspec;
    if (!arg.isEmpty() && fspec.fromPathJsonUri(arg.toStdString()) == 0) {
      player_.openPath(arg);
    }
  } else {
    player_.openLastFile();
  }
  player_.resizeToDefault();
  show();
  QApplication::setActiveWindow(this);
  return QApplication::exec();
}

void PlayerWindow::createMenus() {
  QAction* aboutAction = new QAction("About " + QApplication::applicationName() + "...", this);
  aboutAction->setStatusTip("About this application");
  connect(aboutAction, &QAction::triggered, [this] {
    QMessageBox::about(
        this,
        "About " + QApplication::applicationName(),
        QApplication::applicationDisplayName() + " " + QApplication::applicationVersion() +
            ",  by " + QApplication::organizationName() + ".");
  });
#if IS_MAC_PLATFORM()
  // Will merge the menu item in the app's main menu. Unique MacOS behavior.
  QMenu* appMenu = menuBar()->addMenu(QApplication::applicationName());
  appMenu->addAction(aboutAction);
#endif

  fileMenu_ = menuBar()->addMenu("&File");

  QAction* openAction = new QAction("&Open Local File...", this);
  openAction->setShortcuts(QKeySequence::Open);
  openAction->setStatusTip("Open a local file");
  connect(openAction, &QAction::triggered, &player_, &vrsp::PlayerUI::openFileChooser);
  fileMenu_->addAction(openAction);

  QAction* openPathOrUriAction = new QAction("Open Path or URI...", this);
  openPathOrUriAction->setShortcut(shortcut(Qt::CTRL, Qt::SHIFT, Qt::Key_O));
  openPathOrUriAction->setStatusTip("Open a recording using a path or URI...");
  connect(openPathOrUriAction, &QAction::triggered, &player_, &vrsp::PlayerUI::openPathChooser);
  fileMenu_->addAction(openPathOrUriAction);

  fileMenu_->addSeparator();
  QAction* saveFrames = new QAction("Save All Frames to...", this);
  saveFrames->setShortcut(shortcut(Qt::CTRL, Qt::Key_S));
  saveFrames->setStatusTip("Save Visible Frames to Folder...");
  connect(saveFrames, &QAction::triggered, &player_, &vrsp::PlayerUI::saveFrames);
  fileMenu_->addAction(saveFrames);

#if !IS_MAC_PLATFORM()
  fileMenu_->addSeparator();
  fileMenu_->addAction(aboutAction);
#endif

  textOverlayMenu_ = menuBar()->addMenu("Text Overlay");
  updateTextOverlayMenu();

  layoutMenu_ = menuBar()->addMenu("Layout");
  audioMenu_ = menuBar()->addMenu("Audio");
  presetMenu_ = menuBar()->addMenu("Presets");

  updateAudioMenu();
}

void PlayerWindow::moveEvent(QMoveEvent* event) {
  player_.resizeIfNecessary(true);
  QMainWindow::moveEvent(event);
}

string PlayerWindow::getAudioMode() const {
  return AudioModeConverter::toString(audioMode_);
}

void PlayerWindow::restoreAudioSelection(
    const string& audioMode,
    uint32_t leftAudioChannel,
    uint32_t rightAudioChannel) {
  leftAudioChannel_ = min<uint32_t>(leftAudioChannel, audioChannelCount_);
  rightAudioChannel_ = min<uint32_t>(rightAudioChannel, audioChannelCount_);
  setAudioMode(AudioModeConverter::toEnum(audioMode));
}

void PlayerWindow::updateLayoutAndPresetMenu(
    int frameCount,
    int visibleCount,
    int maxPerRowCount,
    const QVariantMap& presets,
    const QVariant& currentPreset) {
  layoutMenu_->clear();
  layoutActionsAndPreset_.clear();
  if (visibleCount < frameCount) {
    unique_ptr<QAction> layoutAction = make_unique<QAction>(QString("Show All Streams"), this);
    connect(layoutAction.get(), &QAction::triggered, [this]() { player_.showAllStreams(); });
    layoutMenu_->addAction(layoutAction.get());
    layoutActionsAndPreset_.emplace_back(std::move(layoutAction));
    unique_ptr<QAction> toggleAction =
        make_unique<QAction>(QString("Toggle Visible Streams"), this);
    connect(toggleAction.get(), &QAction::triggered, [this]() { player_.toggleVisibleStreams(); });
    layoutMenu_->addAction(toggleAction.get());
    layoutActionsAndPreset_.emplace_back(std::move(toggleAction));
    layoutMenu_->addSeparator();
  }
  for (int layout = 1; layout <= visibleCount; layout++) {
    unique_ptr<QAction> layoutAction = make_unique<QAction>(
        QString("Layout Frames ") + QString::number(layout) + 'x' +
            QString::number((visibleCount + layout - 1) / layout),
        this);
    connect(
        layoutAction.get(), &QAction::triggered, [this, layout]() { player_.relayout(layout); });
    if (layout == maxPerRowCount) {
      layoutAction->setCheckable(true);
      layoutAction->setChecked(true);
    }
    layoutMenu_->addAction(layoutAction.get());
    layoutActionsAndPreset_.emplace_back(std::move(layoutAction));
  }
  layoutMenu_->addSeparator();
  unique_ptr<QAction> resetAction = make_unique<QAction>("Reset All Orientation Settings", this);
  resetAction->setStatusTip("Reset all rotation and mirror settings.");
  connect(resetAction.get(), &QAction::triggered, &player_, &vrsp::PlayerUI::resetOrientation);
  layoutMenu_->addAction(resetAction.get());
  layoutActionsAndPreset_.emplace_back(std::move(resetAction));

  // Preset menu
  presetMenu_->clear();
  set<QString> deleteKeys;
  int number = 0;
  for (const auto& key : presets.keys()) {
    unique_ptr<QAction> recallAction;
    if (presets[key] == currentPreset) {
      recallAction = make_unique<QAction>(QString("Current Preset '" + key + "'"), this);
      recallAction->setCheckable(true);
      recallAction->setChecked(true);
      deleteKeys.insert(key);
    } else {
      recallAction = make_unique<QAction>(QString("Recall Preset '" + key + "'"), this);
    }
    if (++number < 10) {
      recallAction->setShortcut(shortcut(Qt::CTRL, static_cast<int>(Qt::Key_0) + number));
    }
    connect(recallAction.get(), &QAction::triggered, [this, key]() { player_.recallPreset(key); });
    presetMenu_->addAction(recallAction.get());
    layoutActionsAndPreset_.emplace_back(std::move(recallAction));
  }
  if (!presets.empty()) {
    presetMenu_->addSeparator();
  }
  if (!deleteKeys.empty()) {
    for (const auto& key : deleteKeys) {
      unique_ptr<QAction> deleteAction;
      deleteAction = make_unique<QAction>(QString("Delete Preset '" + key + "'"), this);
      connect(
          deleteAction.get(), &QAction::triggered, [this, key]() { player_.deletePreset(key); });
      presetMenu_->addAction(deleteAction.get());
      layoutActionsAndPreset_.emplace_back(std::move(deleteAction));
    }
  } else {
    unique_ptr<QAction> saveAction = make_unique<QAction>(QString("Save Preset"), this);
    connect(saveAction.get(), &QAction::triggered, [this]() { player_.savePreset(); });
    presetMenu_->addAction(saveAction.get());
    layoutActionsAndPreset_.emplace_back(std::move(saveAction));
  }
}

void PlayerWindow::updateTextOverlayMenu() {
  textOverlayMenu_->clear();
  QColor color = player_.getOverlayColor();
  addColorAction(color, Qt::white, "Use White");
  addColorAction(color, Qt::black, "Use Black");
  addColorAction(color, Qt::green, "Use Green");
  addColorAction(color, Qt::red, "Use Red");
  addColorAction(color, Qt::blue, "Use Blue");
  addColorAction(color, Qt::yellow, "Use Yellow");
  addColorAction(color, Qt::cyan, "Use Cyan");
  addColorAction(color, Qt::magenta, "Use Magenta");
  textOverlayMenu_->addSeparator();
  QAction* smallerFont = new QAction("Smaller Font", this);
  smallerFont->setShortcut(shortcut(Qt::CTRL, Qt::Key_Minus));
  connect(smallerFont, &QAction::triggered, [this]() { player_.adjustOverlayFontSize(-1); });
  textOverlayMenu_->addAction(smallerFont);
  QAction* largestFont = new QAction("Larger Font", this);
  largestFont->setShortcut(shortcut(Qt::CTRL, Qt::Key_Plus));
  connect(largestFont, &QAction::triggered, [this]() { player_.adjustOverlayFontSize(+1); });
  textOverlayMenu_->addAction(largestFont);
  textOverlayMenu_->addSeparator();
  bool isSolid = player_.isSolidBackground();
  QAction* solidBackground = new QAction("Solid Background", this);
  connect(solidBackground, &QAction::triggered, [this, isSolid]() {
    player_.setSolidBackground(!isSolid);
  });
  solidBackground->setCheckable(true);
  solidBackground->setChecked(isSolid);
  solidBackground->setShortcut(shortcut(Qt::CTRL, Qt::Key_B));
  textOverlayMenu_->addAction(solidBackground);
}

void PlayerWindow::updateAudioMenu() {
  audioMenu_->clear();
  audioActions_.clear();
  if (audioChannelCount_ == 0 || playbackChannelCount_ == 0) {
    auto noAudioAction = make_unique<QAction>(
        audioChannelCount_ == 0 ? "No Playable Audio" : "No Audio Playback Device", this);
    noAudioAction->setStatusTip("No playable audio stream found in this file.");
    noAudioAction->setDisabled(true);
    audioMenu_->addAction(noAudioAction.get());
    audioActions_.push_back(std::move(noAudioAction));
  } else {
    auto addAudioMode = [this](AudioMode mode, const char* name) {
      unique_ptr<QAction> action = make_unique<QAction>(QString(name), this);
      connect(action.get(), &QAction::triggered, [this, mode]() { setAudioMode(mode); });
      if (audioMode_ == mode) {
        action->setCheckable(true);
        action->setChecked(true);
      } else if (mode != AudioMode::mono && (audioChannelCount_ < 2 || playbackChannelCount_ < 2)) {
        action->setEnabled(false);
      }
      audioMenu_->addAction(action.get());
      audioActions_.emplace_back(std::move(action));
    };
    addAudioMode(AudioMode::mono, "Mono");
    addAudioMode(AudioMode::autoStereo, "Stereo - Auto Channel Pairing");
    addAudioMode(AudioMode::manualStereo, "Stereo - Manual Channel Pairing");
    audioMenu_->addSeparator();
    bool stereo =
        audioMode_ != AudioMode::mono && audioChannelCount_ > 1 && playbackChannelCount_ > 1;
    QMenu* firstChannelMenu = audioMenu_->addMenu(
        stereo ? audioMode_ == AudioMode::autoStereo ? "Stereo Pair" : "Left Channel" : "Channel");
    for (int channel = 0; channel < audioChannelCount_; channel++) {
      bool stereoPair = audioMode_ == AudioMode::autoStereo && channel == leftAudioChannel_ &&
          channel + 1 == rightAudioChannel_;
      unique_ptr<QAction> audioAction = stereoPair
          ? make_unique<QAction>(
                QString("Channels ") + QString::number(channel + 1) + '-' +
                    QString::number(channel + 2),
                this)
          : make_unique<QAction>(QString("Channel ") + QString::number(channel + 1), this);
      connect(audioAction.get(), &QAction::triggered, [this, channel]() {
        leftAudioChannel_ = channel;
        setAudioMode(audioMode_);
      });
      if (channel == leftAudioChannel_) {
        audioAction->setCheckable(true);
        audioAction->setChecked(true);
      }
      firstChannelMenu->addAction(audioAction.get());
      audioActions_.emplace_back(std::move(audioAction));
      if (stereoPair) {
        channel++;
      }
    }
    if (audioMode_ == AudioMode::manualStereo) {
      QMenu* secondChannelMenu = audioMenu_->addMenu("Right Channel");
      for (int channel = 0; channel < audioChannelCount_; channel++) {
        unique_ptr<QAction> audioAction =
            make_unique<QAction>(QString("Channel ") + QString::number(channel + 1), this);
        connect(audioAction.get(), &QAction::triggered, [this, channel]() {
          rightAudioChannel_ = channel;
          setAudioMode(audioMode_);
        });
        if (channel == rightAudioChannel_) {
          audioAction->setCheckable(true);
          audioAction->setChecked(true);
        }
        secondChannelMenu->addAction(audioAction.get());
        audioActions_.emplace_back(std::move(audioAction));
      }
    }
  }
}

void PlayerWindow::setAudioConfiguration(
    uint32_t audioChannelCount,
    uint32_t playbackChannelCount) {
  audioChannelCount_ = audioChannelCount;
  playbackChannelCount_ = playbackChannelCount;
  setAudioMode(AudioMode::autoStereo);
}

void PlayerWindow::setAudioMode(AudioMode audioMode) {
  if (audioChannelCount_ < 2 || playbackChannelCount_ < 2) {
    audioMode = AudioMode::mono;
  }
  audioMode_ = audioMode;
  if (leftAudioChannel_ >= audioChannelCount_) {
    leftAudioChannel_ = 0;
  }
  switch (audioMode_) {
    case AudioMode::mono:
      rightAudioChannel_ = leftAudioChannel_;
      break;

    case AudioMode::autoStereo:
      if (leftAudioChannel_ + 1 >= audioChannelCount_) {
        leftAudioChannel_--;
      }
      rightAudioChannel_ = leftAudioChannel_ + 1;
      break;

    case AudioMode::manualStereo:
      // Nothing to do
      break;
  }

  emit player_.selectedAudioChannelsChanged(leftAudioChannel_, rightAudioChannel_);
  updateAudioMenu();
}

void PlayerWindow::addColorAction(const QColor& overlay, const QColor& color, const char* cmdName) {
  QAction* action = new QAction(cmdName, this);
  connect(action, &QAction::triggered, [this, color]() { player_.setOverlayColor(color); });
  action->setCheckable(true);
  action->setChecked(overlay == color);
  textOverlayMenu_->addAction(action);
}
