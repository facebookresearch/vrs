// Copyright (c) Facebook, Inc. and its affiliates. Confidential and proprietary.

#include <vrs/os/Event.h>

#include <condition_variable>

#include <vrs/os/Time.h>

namespace vrs {
namespace os {

EventChannel::EventChannel(const std::string& name, NotificationMode notificationMode)
    : name_{name},
      notificationMode_{notificationMode},
      numEventsSinceLastWait_{0},
      numEntering_{0},
      numListeners_{0},
      inDestruction_{false},
      mostRecentEvents_{nullptr, 0, 0, 0.0},
      pendingWakeupsCount_{0} {}

EventChannel::~EventChannel() {
  std::unique_lock<std::mutex> lock(mutex_);
  inDestruction_ = true;
  while (numEntering_ + numListeners_ > 0) {
    // It's not safe to call waitForEvent on an EventChannel that can be concurrently
    // destroyed; we might be really unlucky and it doesn't enter waitForEvent
    // until after the destruction is complete.  On the other hand, if there are
    // pending waitForEvent-s it is better to wake them up than to crash.
    wakeupCondition_.notify_all();
    enterCondition_.notify_all();
    std::condition_variable dummyCond;
    dummyCond.wait_for(lock, std::chrono::duration<double>(0.001));
  }
}

void EventChannel::dispatchEvent(void* pointer, int64_t value) {
  std::unique_lock<std::mutex> lock(mutex_);

  mostRecentEvents_.pointer = pointer;
  mostRecentEvents_.value = value;
  mostRecentEvents_.timestampSec = vrs::os::getTimestampSec();
  if (numListeners_ == 0) {
    numEventsSinceLastWait_++;
  } else if (NotificationMode::BROADCAST == notificationMode_) {
    pendingWakeupsCount_ = numListeners_;
    wakeupCondition_.notify_all();
  } else {
    pendingWakeupsCount_ = 1;
    wakeupCondition_.notify_one();
  }
}

void EventChannel::dispatchEvent(int64_t value) {
  dispatchEvent(nullptr, value);
}

EventChannel::Status
EventChannel::waitForEvent(Event& event, double timeoutSec, double lookBackSec) {
  EventChannel::Status status = Status::SUCCESS;
  double startTime = vrs::os::getTimestampSec();

  std::unique_lock<std::mutex> lock(mutex_);

  // Pending wake-up is when broadcast event is dispatched but not all listeners have waked up yet.
  // New listener should not enter critical section before all pre-existing listeners have waked up.
  if (pendingWakeupsCount_ > 0) {
    numEntering_++;
    enterCondition_.wait(lock, [=] { return pendingWakeupsCount_ == 0; });
    numEntering_--;
  }

  // At here, we still have the lock.
  double currentTime = vrs::os::getTimestampSec();
  double timeDiff = currentTime - mostRecentEvents_.timestampSec;
  // If the most recent event is within the look-back range of multiple listeners, then exchange
  // guarantees only one listener can get that past event.
  uint32_t numEventsSinceLastWait = numEventsSinceLastWait_.exchange(0);
  if ((timeDiff < lookBackSec) && (numEventsSinceLastWait > 0)) {
    // Fulfill the wait request with a past event.
    event = mostRecentEvents_;
    event.numMissedEvents =
        numEventsSinceLastWait - 1; // -1 since we did not miss the most recent one
  } else {
    double actualWaitTime = timeoutSec - (currentTime - startTime);
    if (actualWaitTime < 0) {
      status = Status::TIMEOUT;
    } else {
      // At this point, numEventsSinceLastWait is already set to 0.
      numListeners_++;
      bool waitSuccess =
          wakeupCondition_.wait_for(lock, std::chrono::duration<double>(actualWaitTime), [=] {
            return inDestruction_ || pendingWakeupsCount_ > 0;
          });
      numListeners_--;
      if (inDestruction_) {
        // This EventChannel is already being destructed, thus this wake-up is not a real event.
        return Status::FAILURE;
      }
      if (!waitSuccess) {
        status = Status::TIMEOUT;
      } else {
        if (--pendingWakeupsCount_ == 0 && numEntering_ > 0) {
          enterCondition_.notify_all();
        }
      }
    }

    // Caller won't care about a_event if sleep resulted in a timeout.
    event = mostRecentEvents_;
    event.numMissedEvents = numEventsSinceLastWait;
  }

  return status;
}

uint32_t EventChannel::getNumEventsSinceLastWait() const {
  return numEventsSinceLastWait_.load();
}

std::string EventChannel::getName() const {
  return name_;
}

EventChannel::NotificationMode EventChannel::getNotificationMode() const {
  return notificationMode_;
}

} // namespace os
} // namespace vrs
