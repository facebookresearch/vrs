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

#include "Throttler.h"

#include <cmath>

#define DEFAULT_LOG_CHANNEL "Throttler"
#include <logging/Log.h>

#include <vrs/os/Time.h>

namespace vrs::utils {

bool Throttler::report(int line, const void* throttledObjectPtr) {
  std::unique_lock<std::mutex> lock(mutex_);
  Stats& stats = stats_[{line, throttledObjectPtr}];
  const int64_t now = vrs::os::getTimestampMs();
  bool doIt = true;
  if (++stats.requestCounter > kEveryInstanceLimit &&
      now - stats.lastReportedTime < kMaxDelaySec * 1000) {
    doIt = ((stats.skipSinceLastReport + 1) % reportFrequency(stats.requestCounter)) == 0;
  }
  if (doIt) {
    if (stats.requestCounter == kEveryInstanceLimit) {
      XR_LOGW(
          "The following condition has happened {} times now, "
          "so we will no longer report each new occurrence.",
          stats.requestCounter);
    } else if (stats.skipSinceLastReport > 0) {
      XR_LOGW(
          "The following condition has happened {} times, and we no longer report each occurrence. "
          "We skipped {} reports since the last one.",
          stats.requestCounter,
          stats.skipSinceLastReport);
    }
    stats.lastReportedTime = now;
    stats.lastReportedCounter = stats.requestCounter;
    stats.skipSinceLastReport = 0;
  } else {
    stats.skipSinceLastReport++;
  }
  return doIt;
}

// 0-10 -> 1, 11-100 -> 10, 101-1000 -> 100, 1001-10000 -> 10000, etc
int64_t Throttler::reportFrequency(int64_t counter) {
  int64_t power = log10(counter - 1);
  int64_t res = 1;
  for (int64_t p = 1; p <= power; p++) {
    res *= 10;
  }
  return res;
}

} // namespace vrs::utils
