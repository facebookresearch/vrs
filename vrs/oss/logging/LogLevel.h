// Copyright (c) Facebook, Inc. and its affiliates. Confidential and proprietary.

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
