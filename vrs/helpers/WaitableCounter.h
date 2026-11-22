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

#include <atomic>
#include <chrono>
#include <cstdint>

#include <thread_safety_analysis/ConditionVariable.h>
#include <thread_safety_analysis/Mutex.h>

namespace vrs {

/// Thread-safe counter that threads can wait on, until its value drops to a threshold.
/// Reads and additions do not take the wait mutex. Decreasing or resetting the value wakes waiters.
class WaitableCounter {
 public:
  /// @return The current value.
  uint64_t get() const {
    return value_.load(std::memory_order_relaxed);
  }
  /// @param amount: Amount to add to the value.
  void add(uint64_t amount) {
    value_.fetch_add(amount, std::memory_order_relaxed);
  }
  /// @param amount: Amount to subtract from the value.
  void sub(uint64_t amount) XR_TSA_REQUIRES(!mutex_) {
    value_.fetch_sub(amount, std::memory_order_relaxed);
    notifyWaiters();
  }
  /// Set the value to 0.
  void reset() XR_TSA_REQUIRES(!mutex_) {
    value_.store(0, std::memory_order_relaxed);
    notifyWaiters();
  }
  /// Block until the value is at most maxValue.
  /// @param maxValue: Value at or below which to return.
  void waitForAtMost(uint64_t maxValue) XR_TSA_REQUIRES(!mutex_) {
    tsa::unique_lock<tsa::mutex> lock{mutex_};
    decreased_.wait(lock, [&] { return get() <= maxValue; });
  }
  /// Block until the value is at most maxValue, or until the timeout expires.
  /// @param maxValue: Value at or below which to return.
  /// @param timeoutSec: Maximum number of seconds to wait.
  /// @return True if the value is at most maxValue, false on timeout.
  bool waitForAtMost(uint64_t maxValue, double timeoutSec) XR_TSA_REQUIRES(!mutex_) {
    tsa::unique_lock<tsa::mutex> lock{mutex_};
    return decreased_.wait_for(
        lock, std::chrono::duration<double>(timeoutSec), [&] { return get() <= maxValue; });
  }

 private:
  void notifyWaiters() XR_TSA_REQUIRES(!mutex_) {
    // Waiters check the value while holding the mutex: taking it after the update guarantees that
    // each waiter either sees the new value, or is already waiting when we notify.
    {
      tsa::lock_guard<tsa::mutex> lock{mutex_};
    }
    decreased_.notify_all();
  }

  std::atomic<uint64_t> value_{};
  tsa::mutex mutex_;
  tsa::condition_variable decreased_;
};

} // namespace vrs
