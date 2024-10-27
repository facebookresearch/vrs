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
#include <condition_variable>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>
#include <algorithm>

#include <vrs/os/Time.h>

namespace vrs {

/// Helper class to handler a queue of jobs between threads.
/// This class doesn't know about threads, but its APIs are thread-safe,
/// allowing for both concurrent job producers, and concurrent job consumers.

template <class T>
class JobQueue {
 public:
  void sendJob(const T& value) {
    std::unique_lock<std::mutex> locker(mutex_);
    queue_.emplace_back(value);
    condition_.notify_one();
  }
  void sendJob(T&& value) {
    std::unique_lock<std::mutex> locker(mutex_);
    queue_.emplace_back(std::move(value));
    condition_.notify_one();
  }
  /// Wait for a job up to a specified wait time, or until the queue was ended
  bool waitForJob(T& outValue, double waitTime) {
    if (waitTime <= 0) {
      return getJob(outValue);
    }
    double limit = os::getTimestampSec() + waitTime;
    std::unique_lock<std::mutex> locker(mutex_);
    return waitForJobLocked(outValue, locker, limit);
  }
  /// Wait for a job until one is available, or the queue was ended
  bool waitForJob(T& outValue) {
    while (!hasEnded_) {
      if (waitForJob(outValue, 5)) {
        return true;
      }
    }
    return false;
  }
  /// get a pending job, if any, but don't wait
  bool getJob(T& outValue) {
    std::unique_lock<std::mutex> locker(mutex_);
    if (queue_.empty()) {
      return false;
    }
    outValue = std::move(queue_.front());
    queue_.pop_front();
    return true;
  }
  bool waitForJobs(std::deque<T>& jobs, double waitTime) {
    jobs.clear();
    if (waitTime > 0) {
      waitTime += os::getTimestampSec();
    }
    std::unique_lock<std::mutex> locker(mutex_);
    if (!queue_.empty()) {
      jobs.swap(queue_);
      return true;
    }
    if (waitTime > 0) {
      jobs.resize(1);
      if (waitForJobLocked(jobs.front(), locker, waitTime)) {
        return true;
      }
      jobs.clear();
    }
    return false;
  }
  void wakeAll() {
    std::unique_lock<std::mutex> locker(mutex_);
    condition_.notify_all();
  }
  void reset() {
    std::unique_lock<std::mutex> locker(mutex_);
    queue_.clear();
    hasEnded_ = false;
  }
  void endQueue() {
    std::unique_lock<std::mutex> locker(mutex_);
    hasEnded_ = true;
    condition_.notify_all();
  }
  bool hasEnded() const {
    return hasEnded_;
  }
  /// Cancel/remove queued jobs that match a criteria.
  /// Optionaly pass them to a "clean-up" function before they're removed from the queue.
  void cancelQueuedJobs(
      std::function<bool(const T&)> selector,
      std::function<void(const T&)> cleanup = nullptr) {
    std::unique_lock<std::mutex> locker(mutex_);
    auto iter = std::remove_if(queue_.begin(), queue_.end(), selector);
    if (cleanup) {
      for (auto subIter = iter; subIter != queue_.end(); ++subIter) {
        cleanup(*subIter);
      }
    }
    queue_.erase(iter, queue_.end());
  }
  void cancelAllQueuedJobs() {
    std::unique_lock<std::mutex> locker(mutex_);
    queue_.clear();
  }
  size_t getQueueSize() const {
    std::unique_lock<std::mutex> locker(mutex_);
    return queue_.size();
  }

 protected:
  bool waitForJobLocked(T& outValue, std::unique_lock<std::mutex>& locker, double limitTime) {
    double actualWaitTime = 0;
    while (!hasEnded_ && queue_.empty() &&
           (actualWaitTime = limitTime - os::getTimestampSec()) >= 0) {
      condition_.wait_for(locker, std::chrono::duration<double>(actualWaitTime));
    }
    if (hasEnded_ || queue_.empty()) {
      return false;
    }
    outValue = std::move(queue_.front());
    queue_.pop_front();
    return true;
  }

 private:
  std::deque<T> queue_;
  mutable std::mutex mutex_;
  std::condition_variable condition_;
  std::atomic<bool> hasEnded_{false};
};

/// Convenience class when a working with a single background thread.
template <class T>
class JobQueueWithThread : public JobQueue<T> {
 public:
  ~JobQueueWithThread() {
    endThread();
  }

  template <typename... Args>
  void startThread(Args&&... args) {
    thread_ = std::unique_ptr<std::thread>(new std::thread(std::forward<Args>(args)...));
  }
  template <typename... Args>
  void startThreadIfNeeded(Args&&... args) {
    if (!thread_) {
      thread_ = std::unique_ptr<std::thread>(new std::thread(std::forward<Args>(args)...));
    }
  }
  void endThread() {
    if (thread_) {
      this->endQueue();
      if (thread_->joinable()) {
        thread_->join();
      }
    }
  }

 protected:
  std::unique_ptr<std::thread> thread_;
};

} // namespace vrs
