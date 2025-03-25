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

#include <qapplication.h>
#include <QtCore/QCommandLineParser>

namespace vrsp {

class PlayerUI;

class VrsPlayerApplication : public QApplication {
 public:
  VrsPlayerApplication(int& argc, char** argv) : QApplication(argc, argv) {}

  int run(PlayerUI& playerUI, QCommandLineParser& parser);
  bool event(QEvent* event) override;

  void openFirstFile();

 private:
  PlayerUI* playerUI_{};
  QString firstFile_;
  bool firstFileOpened_{false};
};

} // namespace vrsp
