// (c) Facebook, Inc. and its affiliates. Confidential and proprietary.

#pragma once

#include <cstdint>

/// VRS internal API. DO NOT USE THESE API OUTSIDE OF VRS. Make your own time domain decision.

namespace vrs::os {

/// Time in seconds since some unspecified point in time, which might be the last boot time.
/// This time is guaranteed to be monotonous throughout the run of the app, and really measure
/// walltime spent between two invocations, even if the device sleeps.
double getTimestampSec();

/// Epoch time maybe adjusted at any point in time, it is NOT a monotonic clock, and it should
/// only be used for time intervals in rare conditions, such as when the time needs to be persisted,
/// and compared between runs, possibly between reboots. Accuracy should never be relied on, and
/// using a count of seconds will discourage usage for high resolution cases.
/// Therefore, using double is misleading & dangerous.
int64_t getCurrentTimeSecSinceEpoch();

} // namespace vrs::os
