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

#include <cstdint>

/// VRS internal API. DO NOT USE THESE API OUTSIDE OF VRS. Make your own time domain decision.

namespace vrs {
namespace os {

/// Time in seconds since some unspecified point in time, which might be the last boot time.
/// This time is guaranteed to be monotonous throughout the run of the app, and really measure
/// walltime spent between two invocations, even if the device sleeps.
double getTimestampSec();

/// Time in milliseconds since some unspecified point in time, which might be the last boot time.
/// This time is guaranteed to be monotonous throughout the run of the app, and really measure
/// walltime spent between two invocations, even if the device sleeps.
/// Use this clock for interval measurements.
int64_t getTimestampMs();

/// Epoch time maybe adjusted at any point in time, it is NOT a monotonic clock, and it should
/// only be used for time intervals in rare conditions, such as when the time needs to be persisted,
/// and compared between runs, possibly between reboots. Accuracy should never be relied on, and
/// using a count of seconds will discourage usage for high resolution cases.
/// Therefore, using double is misleading & dangerous.
int64_t getCurrentTimeSecSinceEpoch();

} // namespace os
} // namespace vrs
