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
#include <vector>

#include <QAction>
#include <QApplication>
#include <QMainWindow>
#include <QtCore/QCommandLineParser>

#include "PlayerUI.h"

namespace vrsp {

enum class AudioMode {
  mono = 0, // same audio channel sent to all output channels
  autoStereo = 1, // successive audio channels sent as left-right stereo pair
  manualStereo = 2, // arbitrary channels selected to be sent as stereo pair
};

class PlayerWindow : public QMainWindow {
  Q_OBJECT public : explicit PlayerWindow(QApplication& app);

  int processCommandLine(QCommandLineParser& parser);

  void createMenus();

  FileReader& getFileReader() {
    return player_.getFileReader();
  }
  PlayerUI& getPlayerUI() {
    return player_;
  }
  void moveEvent(QMoveEvent* event) override;

 public slots:
  void updateLayoutMenu(
      int frameCount,
      int visibleCount,
      int maxPerRowCount,
      const QVariantMap& presets,
      const QVariant& currentPreset);
  void updateTextOverlayMenu();
  void updateAudioMenu();
  void setAudioConfiguration(uint32_t audioChannelCount, uint32_t playbackChannelCount);
  void setAudioMode(AudioMode audioMode);

 private:
  void addColorAction(const QColor& overlay, const QColor& color, const char* cmdName);

 private:
  PlayerUI player_;
  QMenu* fileMenu_{};
  QMenu* textOverlayMenu_{};
  QMenu* layoutMenu_{};
  std::vector<std::unique_ptr<QAction>> layoutActions_;
  QMenu* orientationMenu_{};
  QMenu* audioMenu_{};
  std::vector<std::unique_ptr<QAction>> audioActions_;
  uint32_t audioChannelCount_{};
  uint32_t playbackChannelCount_{};
  uint32_t leftAudioChannel_{};
  uint32_t rightAudioChannel_{};
  AudioMode audioMode_{AudioMode::autoStereo};
};

} // namespace vrsp
