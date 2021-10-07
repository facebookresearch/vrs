// Copyright (c) Facebook, Inc. and its affiliates. Confidential and proprietary.

#define DEFAULT_LOG_CHANNEL "Log"
#include "Log.h"

#include <fmt/color.h>

namespace vrs::logging {

void log(Level level, const char* channel, const std::string& message) {
  fmt::color c;
  std::string logLevel = "";
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
      c = fmt::color::green;
      logLevel = "INFO";
      break;
    case Level::Debug:
      c = fmt::color::yellow;
      logLevel = "DEBUG";
      break;
    default:
      c = fmt::color::white;
  }
  fmt::print(stderr, fg(c), "[{}][{}]: {}", channel, logLevel, message);
}

} // namespace vrs::logging
