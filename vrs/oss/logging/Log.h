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

#pragma once

#include <string>

#include <fmt/core.h>

#ifndef DEFAULT_LOG_CHANNEL
#error "DEFAULT_LOG_CHANNEL must be defined before including <logging/Log.h>"
#endif // DEFAULT_LOG_CHANNEL

namespace vrs {
namespace logging {

enum class Level {
  Error = 0,
  Warning = 1,
  Info = 2,
  Debug = 3,
};

void log(Level level, const char* channel, const std::string& message);

} // namespace logging
} // namespace vrs

#define XR_LOG_DEFAULT(level, ...) log(level, DEFAULT_LOG_CHANNEL, fmt::format(__VA_ARGS__))

#define XR_LOGD(...) XR_LOG_DEFAULT(vrs::logging::Level::Debug, __VA_ARGS__)
#define XR_LOGI(...) XR_LOG_DEFAULT(vrs::logging::Level::Info, __VA_ARGS__)
#define XR_LOGW(...) XR_LOG_DEFAULT(vrs::logging::Level::Warning, __VA_ARGS__)
#define XR_LOGE(...) XR_LOG_DEFAULT(vrs::logging::Level::Error, __VA_ARGS__)
