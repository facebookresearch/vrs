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

#include <vrs/os/Platform.h>

#include <vrs/FileHandler.h>

using namespace vrsp;
using namespace std;

PlayerWindow::PlayerWindow(QApplication& app) : QMainWindow(nullptr) {
  setCentralWidget(&player_);
  app.installEventFilter(&player_);
  createMenus();
  connect(
      &player_.getFileReader(),
      &FileReader::updateLayoutMenu,
      this,
      &PlayerWindow::updateLayoutMenu);
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
  openPathOrUriAction->setShortcut(Qt::CTRL + Qt::SHIFT + Qt::Key_O);
  openPathOrUriAction->setStatusTip("Open a recording using a path or URI...");
  connect(openPathOrUriAction, &QAction::triggered, &player_, &vrsp::PlayerUI::openPathChooser);
  fileMenu_->addAction(openPathOrUriAction);

#if !IS_MAC_PLATFORM()
  fileMenu_->addSeparator();
  fileMenu_->addAction(aboutAction);
#endif

  colorMenu_ = menuBar()->addMenu("Text Color");
  QAction* white = new QAction("Use White", this);
  connect(white, &QAction::triggered, [this]() { player_.setOverlayColor(Qt::white); });
  colorMenu_->addAction(white);
  QAction* black = new QAction("Use Black", this);
  connect(black, &QAction::triggered, [this]() { player_.setOverlayColor(Qt::black); });
  colorMenu_->addAction(black);
  QAction* green = new QAction("Use Green", this);
  connect(green, &QAction::triggered, [this]() { player_.setOverlayColor(Qt::green); });
  colorMenu_->addAction(green);
  QAction* red = new QAction("Use Red", this);
  connect(red, &QAction::triggered, [this]() { player_.setOverlayColor(Qt::red); });
  colorMenu_->addAction(red);
  QAction* blue = new QAction("Use Blue", this);
  connect(blue, &QAction::triggered, [this]() { player_.setOverlayColor(Qt::blue); });
  colorMenu_->addAction(blue);
  QAction* yellow = new QAction("Use Yellow", this);
  connect(yellow, &QAction::triggered, [this]() { player_.setOverlayColor(Qt::yellow); });
  colorMenu_->addAction(yellow);
  QAction* cyan = new QAction("Use Cyan", this);
  connect(cyan, &QAction::triggered, [this]() { player_.setOverlayColor(Qt::cyan); });
  colorMenu_->addAction(cyan);
  QAction* magenta = new QAction("Use Magenta", this);
  connect(magenta, &QAction::triggered, [this]() { player_.setOverlayColor(Qt::magenta); });
  colorMenu_->addAction(magenta);

  layoutMenu_ = menuBar()->addMenu("Layout");

  orientationMenu_ = menuBar()->addMenu("Orientation");
  QAction* resetAction = new QAction("Reset All Orientation Settings", this);
  resetAction->setStatusTip("Reset all rotation and mirror settings.");
  connect(resetAction, &QAction::triggered, &player_, &vrsp::PlayerUI::resetOrientation);
  orientationMenu_->addAction(resetAction);
}

void PlayerWindow::updateLayoutMenu(
    int frameCount,
    int visibleCount,
    int maxPerRowCount,
    const QVariantMap& presets,
    const QVariant& currentPreset) {
  layoutMenu_->clear();
  layoutActions_.clear();
  if (visibleCount < frameCount) {
    unique_ptr<QAction> layoutAction = make_unique<QAction>(QString("Show All Streams"), this);
    connect(layoutAction.get(), &QAction::triggered, [this]() { player_.showAllStreams(); });
    layoutMenu_->addAction(layoutAction.get());
    layoutActions_.emplace_back(std::move(layoutAction));
    unique_ptr<QAction> toggleAction =
        make_unique<QAction>(QString("Toggle Visible Streams"), this);
    connect(toggleAction.get(), &QAction::triggered, [this]() { player_.toggleVisibleStreams(); });
    layoutMenu_->addAction(toggleAction.get());
    layoutActions_.emplace_back(std::move(toggleAction));
    layoutMenu_->addSeparator();
  }
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
      recallAction->setShortcut(Qt::CTRL + Qt::Key_0 + number);
    }
    connect(recallAction.get(), &QAction::triggered, [this, key]() { player_.recallPreset(key); });
    layoutMenu_->addAction(recallAction.get());
    layoutActions_.emplace_back(std::move(recallAction));
  }
  if (!presets.empty()) {
    layoutMenu_->addSeparator();
  }
  if (!deleteKeys.empty()) {
    for (const auto& key : deleteKeys) {
      unique_ptr<QAction> deleteAction;
      deleteAction = make_unique<QAction>(QString("Delete Preset '" + key + "'"), this);
      connect(
          deleteAction.get(), &QAction::triggered, [this, key]() { player_.deletePreset(key); });
      layoutMenu_->addAction(deleteAction.get());
      layoutActions_.emplace_back(std::move(deleteAction));
    }
  } else {
    unique_ptr<QAction> saveAction = make_unique<QAction>(QString("Save Preset"), this);
    connect(saveAction.get(), &QAction::triggered, [this]() { player_.savePreset(); });
    layoutMenu_->addAction(saveAction.get());
    layoutActions_.emplace_back(std::move(saveAction));
  }
  layoutMenu_->addSeparator();
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
    layoutActions_.emplace_back(std::move(layoutAction));
  }
}
