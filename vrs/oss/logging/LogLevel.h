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

// Empty implementation to provide the same APIs as FB's internal logging backend.
namespace arvr {
namespace logging {

enum class Level {
  Disabled = 0, ///< Completely suppresses log output. Not available in the logging macros.
  Error = 1, ///<
  Warning = 2,
  Info = 3,
  Debug = 4,
  Trace = 5,
  UseGlobalSettings = 0xF, ///< Use the global log level instead of a channel override. Not
                           ///< available in the logging macros.
};

// Set the global log level that applies for all channels, unless the channel has separate settings.
// Empty implementation for now in OSS.
inline void setGlobalLogLevel(Level level) {}

} // namespace logging
} // namespace arvr
