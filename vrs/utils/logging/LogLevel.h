// Copyright (c) Facebook, Inc. and its affiliates. Confidential and proprietary.

#pragma once

// Dummy implementation to make it consistent with FB internal logging backend.
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

/**
 * Set the global log level that applies for all channels, unless the channel has separate settings.
 */
void setGlobalLogLevel(Level level) {}

} // namespace logging
} // namespace arvr
