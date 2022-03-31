// Copyright (c) Meta Platforms, Inc. and affiliates.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#define DEFAULT_LOG_CHANNEL "Log"
#include "Log.h"

#include <fmt/color.h>

using namespace std;

namespace vrs {
namespace logging {

void log(Level level, const char* channel, const std::string& message) {
  const fmt::color kNoColor = static_cast<fmt::color>(0xFFFFFFFF);
  fmt::color c = kNoColor;
  const char* logLevel = "";
  switch (level) {
    case Level::Error:
      c = fmt::color::red;
      logLevel = "ERROR";
      break;
    case Level::Warning:
      c = fmt::color::orange;
      logLevel = "WARNING";
      break;
    case Level::Info:
      c = fmt::color::blue;
      logLevel = "INFO";
      break;
    case Level::Debug:
      c = fmt::color::green;
      logLevel = "DEBUG";
      break;
    default:
      c = kNoColor;
  }
  if (c != kNoColor) {
    fmt::print(stderr, fg(c), "[{}][{}]: {}\x1b[0m\n", channel, logLevel, message);
  } else {
    fmt::print(stderr, "[{}][{}]: {}\n", channel, logLevel, message);
  }
}

void log_every_n_seconds(
    const char* file,
    int line,
    Level level,
    int nSeconds,
    const char* channel,
    const std::string& message) {
  log(level, channel, message);
}

} // namespace logging
} // namespace vrs
