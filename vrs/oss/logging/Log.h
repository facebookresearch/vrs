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

#include <string>

#include <fmt/core.h>

namespace vrs {
namespace logging {

enum class Level {
  Error = 0,
  Warning = 1,
  Info = 2,
  Debug = 3,
};

/// Logging backend: redirect where you need, depending on the log level and your preferences.
void log(Level level, const char* channel, const std::string& message);

/// Logging backend: redirect where you need, depending on the log level and your preferences
/// This variant filters out too frequent output, based on the call site and a minimum duration
/// between printouts.
/// Warning: Currently does the same thing as regular logging (no time filtering yet).
void log_every_n_seconds(
    const char* file,
    int line,
    Level level,
    int nSeconds,
    const char* channel,
    const std::string& message);

} // namespace logging
} // namespace vrs

#ifdef DEFAULT_LOG_CHANNEL
#define XR_LOG_DEFAULT(level, ...) log(level, DEFAULT_LOG_CHANNEL, fmt::format(__VA_ARGS__))
#define XR_LOG_EVERY_N_SEC_DEFAULT(level, nseconds, ...) \
  log_every_n_seconds(                                   \
      __FILE__, __LINE__, level, nseconds, DEFAULT_LOG_CHANNEL, fmt::format(__VA_ARGS__))

#define XR_LOGD(...) XR_LOG_DEFAULT(vrs::logging::Level::Debug, __VA_ARGS__)
#define XR_LOGI(...) XR_LOG_DEFAULT(vrs::logging::Level::Info, __VA_ARGS__)
#define XR_LOGW(...) XR_LOG_DEFAULT(vrs::logging::Level::Warning, __VA_ARGS__)
#define XR_LOGE(...) XR_LOG_DEFAULT(vrs::logging::Level::Error, __VA_ARGS__)

#define XR_LOGD_EVERY_N_SEC(nseconds, ...) \
  XR_LOG_EVERY_N_SEC_DEFAULT(vrs::logging::Level::Debug, nseconds, __VA_ARGS__)
#define XR_LOGI_EVERY_N_SEC(nseconds, ...) \
  XR_LOG_EVERY_N_SEC_DEFAULT(vrs::logging::Level::Info, nseconds, __VA_ARGS__)
#define XR_LOGW_EVERY_N_SEC(nseconds, ...) \
  XR_LOG_EVERY_N_SEC_DEFAULT(vrs::logging::Level::Warning, nseconds, __VA_ARGS__)
#define XR_LOGE_EVERY_N_SEC(nseconds, ...) \
  XR_LOG_EVERY_N_SEC_DEFAULT(vrs::logging::Level::Error, nseconds, __VA_ARGS__)
#endif

#define XR_LOG_CHANNEL(level, channel, ...) log(level, channel, fmt::format(__VA_ARGS__))

#define XR_LOGCD(CHANNEL, ...) XR_LOG_CHANNEL(vrs::logging::Level::Debug, CHANNEL, __VA_ARGS__)
#define XR_LOGCI(CHANNEL, ...) XR_LOG_CHANNEL(vrs::logging::Level::Info, CHANNEL, __VA_ARGS__)
#define XR_LOGCW(CHANNEL, ...) XR_LOG_CHANNEL(vrs::logging::Level::Warning, CHANNEL, __VA_ARGS__)
#define XR_LOGCE(CHANNEL, ...) XR_LOG_CHANNEL(vrs::logging::Level::Error, CHANNEL, __VA_ARGS__)
