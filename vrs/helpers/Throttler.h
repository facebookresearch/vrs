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

#include <cstdint>

#include <map>
#include <mutex>
#include <utility>

namespace vrs::utils {

/// Helper class to throttle some action (probably error or warning logging)
/// At the top of your cpp file, add this to get a local instance tracker:
/// namespace {
/// vrs::utils::Throttler& getThrottler() {
///   static vrs::utils::Throttler sThrottler;
///   return sThrottler;
/// }
/// } // namespace
///
/// Then to throttle error logging for a particular object/file/state, use:
///   THROTTLED_LOGE(my_thing, "this bad thing happened")
/// Example of "my_thing": a pointer to a file being read. If the error happens many times for a
/// particular file, you want to stop reporting it everytime, but if it also happens for another,
/// you want to report it independently throttled for that other file.
/// If you don't need that context awareness, pass a nullptr.
///
/// If you're still confused, run the unit tests and check the output.

class Throttler {
  using Ref = std::pair<int, const void*>;

  struct Stats {
    int64_t lastReportedTime{}; // when we last logged
    int64_t lastReportedCounter{}; // attempt counter when we last logged
    int64_t requestCounter{}; // how many attempts to log have we gotten, total
    int64_t skipSinceLastReport{}; // how many log attempts since the last time we logged?
  };

 public:
  explicit Throttler(int64_t everyInstanceLimit = 20, int64_t maxDelaySec = 10)
      : kEveryInstanceLimit{everyInstanceLimit}, kMaxDelaySec{maxDelaySec} {}

  bool report(int line, const void* throttledObjectPtr = nullptr);

  static int64_t reportFrequency(int64_t counter);

 private:
  const int64_t kEveryInstanceLimit;
  const int64_t kMaxDelaySec;
  std::mutex mutex_;
  std::map<Ref, Stats> stats_;
};

#define THROTTLED_LOGE(THROTTLED_OBJECT_PTR, ...)              \
  if (getThrottler().report(__LINE__, THROTTLED_OBJECT_PTR)) { \
    XR_LOGE(__VA_ARGS__);                                      \
  }

#define THROTTLED_LOGW(THROTTLED_OBJECT_PTR, ...)              \
  if (getThrottler().report(__LINE__, THROTTLED_OBJECT_PTR)) { \
    XR_LOGW(__VA_ARGS__);                                      \
  }

#define THROTTLED_VERIFY(THROTTLED_OBJECT_PTR, VERIFY_CONDITION)                          \
  [&](bool _throttled_condition) {                                                        \
    if (!_throttled_condition && getThrottler().report(__LINE__, THROTTLED_OBJECT_PTR)) { \
      XR_LOGW("Verify '{}' failed: ", #VERIFY_CONDITION);                                 \
    }                                                                                     \
    return _throttled_condition;                                                          \
  }(VERIFY_CONDITION)

} // namespace vrs::utils
