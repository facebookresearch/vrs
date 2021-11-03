// Copyright (c) Facebook, Inc. and its affiliates. Confidential and proprietary.

#define DEFAULT_LOG_CHANNEL "Log"
#include "Log.h"

#include <fmt/color.h>

namespace vrs::logging {

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

} // namespace vrs::logging
