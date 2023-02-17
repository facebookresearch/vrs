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

#include <array>

#include <qapplication.h>
#include <qdir.h>

#include <vrs/os/Platform.h>

#if IS_VRS_FB_INTERNAL()
#include <qplugin.h>

#if defined(Q_OS_LINUX)
Q_IMPORT_PLUGIN(QXcbIntegrationPlugin);
#elif defined(Q_OS_MACOS)
Q_IMPORT_PLUGIN(QCocoaIntegrationPlugin);
#elif defined(Q_OS_WIN)
Q_IMPORT_PLUGIN(QWindowsIntegrationPlugin)
#endif
#endif

#include <logging/LogLevel.h>

#include "PlayerWindow.h"

using namespace std;

namespace {

using namespace vrs;

#if defined(Q_OS_LINUX)
void platformConfig() {
  // The default build config seems to be unable to locate fonts
  // Give QT a hint via the QT_QPA_FONTDIR env var
  auto fontdir = qgetenv("QT_QPA_FONTDIR");
  if (fontdir.isEmpty()) {
    // search some common font directories
    array<QDir, 3> dirs{
        QString{"/usr/share/fonts/truetype/"},
        QString{"/usr/share/fonts/gnu-free/"},
    };
    for (const auto& dir : dirs) {
      if (dir.exists() && !dir.isEmpty()) {
        qputenv("QT_QPA_FONTDIR", dir.path().toUtf8());
        break;
      }
    }
  }
}
#else
void platformConfig() {}
#endif

} // namespace

int main(int argc, char* argv[]) {
  ::arvr::logging::setGlobalLogLevel(arvr::logging::Level::Info);

  platformConfig();
  QApplication app(argc, argv);

  QApplication::setStyle("Fusion");

  qRegisterMetaType<FileReaderState>();

  QCoreApplication::setApplicationName("VRSplayer");
  QCoreApplication::setOrganizationName("Meta Reality Labs");
  QGuiApplication::setApplicationDisplayName(QCoreApplication::applicationName());
  QCoreApplication::setApplicationVersion("v2.1.0");

  QCommandLineParser parser;
  parser.setApplicationDescription("VRSplayer");
  parser.addHelpOption();
  parser.addVersionOption();
  parser.addPositionalArgument("url", "https://about.facebook.com/realitylabs/");
  parser.process(app);

  vrsp::PlayerWindow playerWindow(app);

  return playerWindow.processCommandLine(parser);
}
