// Copyright (c) Facebook, Inc. and its affiliates. Confidential and proprietary.

#pragma once

#include <string>

#include <fmt/core.h>

#ifndef DEFAULT_LOG_CHANNEL
#error "DEFAULT_LOG_CHANNEL must be defined before including <logging/Log.h>"
#endif // DEFAULT_LOG_CHANNEL

namespace vrs::logging {
enum class Level {
  Error = 0,
  Warning = 1,
  Info = 2,
  Debug = 3,
};

void log(Level level, const char* channel, const std::string& message);
} // namespace vrs::logging

#define XR_LOG_DEFAULT(level, ...) log(level, DEFAULT_LOG_CHANNEL, fmt::format(__VA_ARGS__))

#define XR_LOGD(...) XR_LOG_DEFAULT(vrs::logging::Level::Debug, __VA_ARGS__)
#define XR_LOGI(...) XR_LOG_DEFAULT(vrs::logging::Level::Info, __VA_ARGS__)
#define XR_LOGW(...) XR_LOG_DEFAULT(vrs::logging::Level::Warning, __VA_ARGS__)
#define XR_LOGE(...) XR_LOG_DEFAULT(vrs::logging::Level::Error, __VA_ARGS__)
