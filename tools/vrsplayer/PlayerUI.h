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

#include <functional>
#include <vector>

#include <QtCore/qglobal.h>
#include <qcolor.h>
#include <qcombobox.h>
#include <qgraphicsscene.h>
#include <qnamespace.h>
#include <qpixmap.h>
#include <qsettings.h>
#include <qwidget.h>

#include "FileReader.h"

QT_BEGIN_NAMESPACE
class QHBoxLayout;
class QAbstractButton;
class QSlider;
class QLabel;
class QUrl;
QT_END_NAMESPACE

namespace vrsp {

using PathPreparer = std::function<QString(const QString&)>;

class PlayerWindow;

class PlayerUI : public QWidget {
  Q_OBJECT
 public:
  explicit PlayerUI(PlayerWindow* playerWindow);

  void setPathPreparer(const PathPreparer& pathPreparer) {
    pathPreparer_ = pathPreparer;
  }

  void openPath(const QString& url);
  void resizeToDefault();
  void resizeIfNecessary(bool maxSizeOnly = false);
  FileReader& getFileReader() {
    return fileReader_;
  }
  PlayerWindow* getPlayerWindow() const {
    return playerWindow_;
  }

  QColor getOverlayColor() const {
    return overlayColor_;
  }
  bool isSolidBackground() const {
    return solidBackground_;
  }

 signals:
  void overlaySettingChanged();
  void selectedAudioChannelsChanged(uint32_t leftAudioChannel, uint32_t rightAudioChannel);

 public slots:
  void openFileChooser();
  void openPathChooser();
  void saveFrames();
  void openLastFile();
  void backwardPressed();
  void stopPressed();
  void playPausePressed();
  void forwardPressed();
  void checkForUpdates();
  void relayout(int framesPerRow);
  void resetOrientation();
  void showAllStreams();
  void toggleVisibleStreams();
  void savePreset();
  void recallPreset(const QString& preset);
  void deletePreset(const QString& preset);
  void reportError(const QString& errorTitle, const QString& errorMessage);
  void setOverlayColor(QColor color);
  void adjustOverlayFontSize(int sizeChange);
  void setSolidBackground(bool solid);
  void adjustSpeed(int change);
  static void printSegmentationColors();

 private slots:
  void mediaStateChanged(FileReaderState state);
  void durationChanged(double start, double end, int duration);
  void timeChanged(double time, int position);
  void recordTypeChanged(int index);
  void speedControlChanged(int index);
  void setStatusText(const std::string& statusText);
  bool eventFilter(QObject* obj, QEvent* event) override;

 private:
  PlayerWindow* playerWindow_;
  QSettings settings_;
  QColor overlayColor_{Qt::yellow};
  int fontSize_{14};
  bool solidBackground_{false};
  FileReader fileReader_;
  QVBoxLayout* videoFrames_;
  std::vector<FrameWidget*> frames_;
  QAbstractButton* backwardButton_;
  QAbstractButton* stopButton_;
  QAbstractButton* playPauseButton_;
  QAbstractButton* forwardButton_;
  QComboBox* speedControl_;
  QLabel* time_;
  QSlider* positionSlider_;
  QLabel* statusLabel_;
  QTimer checkForUpdatesTimer_;
  PathPreparer pathPreparer_;
};

} // namespace vrsp
