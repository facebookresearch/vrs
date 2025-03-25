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
#include <logging/Log.h>
#include <logging/Verify.h>

#include "VideoTime.h"

#include <vrs/helpers/Strings.h>

namespace vrsp {

using namespace std;

bool VrsPlayerApplication::event(QEvent* event) {
  if (event->type() == QEvent::FileOpen) {
    auto* openEvent = static_cast<QFileOpenEvent*>(event);
    QString file = openEvent->file().isEmpty() ? openEvent->url().toLocalFile() : openEvent->file();
    if (!file.isEmpty()) {
      if (firstFileOpened_) {
        XR_LOGD("Open event for {} (now)", file.toStdString());
        playerUI_->openPath(file);
      } else {
        XR_LOGD("Open event for {} (later)", file.toStdString());
        firstFile_ = std::move(file);
      }
    }
    return true;
  }
  return QApplication::event(event);
}

void VrsPlayerApplication::openFirstFile() {
  if (!firstFileOpened_) {
    firstFileOpened_ = true;
    if (!firstFile_.isEmpty()) {
      XR_LOGD("Open first file {}", firstFile_.toStdString());
      playerUI_->openPath(firstFile_);
    } else {
      XR_LOGD("Open first file: no file, so opening last file");
      playerUI_->openLastFile();
    }
    playerUI_->resizeToDefault();
    playerUI_->window()->show();
  }
}

int VrsPlayerApplication::run(PlayerUI& playerUI, QCommandLineParser& parser) {
  playerUI_ = &playerUI;
  installEventFilter(playerUI_);
  if (!parser.positionalArguments().isEmpty()) {
    const QString& arg = parser.positionalArguments().constFirst();
    vrs::FileSpec fspec;
    if (!arg.isEmpty() && fspec.fromPathJsonUri(arg.toStdString()) == 0) {
      firstFile_ = arg;
      XR_LOGD("VrsPlayerApplication::processCommandLine: {}", firstFile_.toStdString());
    }
  }
  // We can't tell yet if the user opened a file from the UI and a FileOpen event is coming in
  // shortly, so we don't want to prematurely open the last file or the openFile dialog.
  // If a FileOpen event is coming, it's coming fast, so we don't need to wait much.
  int delay = firstFile_.isEmpty() ? 1 : 0;
  QTimer::singleShot(delay, this, &VrsPlayerApplication::openFirstFile);
  return exec();
}

} // namespace vrsp
