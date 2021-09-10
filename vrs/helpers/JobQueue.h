// Facebook Technologies, LLC Properietary and Confidential.
#pragma once

#include <algorithm>
#include <atomic>
#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <queue>
#include <thread>

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
  bool waitForJob(T& outValue, double waitTime) {
    double limit = os::getTimestampSec() + waitTime;
    std::unique_lock<std::mutex> locker(mutex_);
    double actualWaitTime;
    while (!hasEnded_ && queue_.empty() && (actualWaitTime = limit - os::getTimestampSec()) >= 0) {
      condition_.wait_for(locker, std::chrono::duration<double>(actualWaitTime));
    }
    if (hasEnded_ || queue_.empty()) {
      return false;
    }
    outValue = std::move(queue_.front());
    queue_.pop_front();
    return true;
  }
  bool getJob(T& outValue) {
    std::unique_lock<std::mutex> locker(mutex_);
    if (queue_.empty()) {
      return false;
    }
    outValue = std::move(queue_.front());
    queue_.pop_front();
    return true;
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

 private:
  std::deque<T> queue_;
  std::mutex mutex_;
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
