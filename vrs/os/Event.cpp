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

#include <vrs/os/Event.h>

#include <condition_variable>
#include <thread>
#include <utility>

#include <vrs/os/Time.h>

#define DEFAULT_LOG_CHANNEL "EventChannel"
#include <logging/Verify.h>

using namespace std;

namespace vrs {
namespace os {

EventChannel::EventChannel(string name, NotificationMode notificationMode)
    : name_{std::move(name)},
      notificationMode_{notificationMode},
      numEventsSinceLastWait_{0},
      numEntering_{0},
      numListeners_{0},
      inDestruction_{false},
      pendingWakeupsCount_{0},
      mostRecentEvents_{0, 0, 0.0} {}

EventChannel::~EventChannel() {
  unique_lock<mutex> lock(mutex_);
  inDestruction_ = true;
  // NOTE: waiting 3 times should be long enough
  int maxLoopCount = 3;
  while ((numEntering_ + numListeners_ > 0) && XR_VERIFY(maxLoopCount-- > 0)) {
    // It's not safe to call waitForEvent on an EventChannel that can be concurrently
    // destroyed; we might be really unlucky and it doesn't enter waitForEvent
    // until after the destruction is complete.  On the other hand, if there are
    // pending waitForEvent-s it is better to wake them up than to crash.
    wakeupCondition_.notify_all();
    enterCondition_.notify_all();
    this_thread::sleep_for(chrono::milliseconds(1));
  }
}

void EventChannel::dispatchEvent(int64_t value) {
  unique_lock<mutex> lock(mutex_);

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

EventChannel::Status
EventChannel::waitForEvent(Event& event, double timeoutSec, double lookBackSec) {
  EventChannel::Status status = Status::SUCCESS;
  double startTime = vrs::os::getTimestampSec();

  unique_lock<mutex> lock(mutex_);

  // Pending wake-up is when broadcast event is dispatched but not all listeners have waked up yet.
  // New listener should not enter critical section before all pre-existing listeners have waked up.
  if (pendingWakeupsCount_ > 0) {
    numEntering_++;
    enterCondition_.wait(lock, [this] { return pendingWakeupsCount_ == 0; });
    numEntering_--;
    if (inDestruction_) {
      // This EventChannel is already being destructed, exit ASAP.
      return Status::FAILURE;
    }
  }

  // At here, we still have the lock.
  double currentTime = vrs::os::getTimestampSec();
  double timeDiff = currentTime - mostRecentEvents_.timestampSec;
  // If the most recent event is within the look-back range of multiple listeners, then exchange
  // guarantees only one listener can get that past event.
  int64_t numEventsSinceLastWait = numEventsSinceLastWait_.exchange(0);
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
          wakeupCondition_.wait_for(lock, chrono::duration<double>(actualWaitTime), [this] {
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

int64_t EventChannel::getNumEventsSinceLastWait() const {
  return numEventsSinceLastWait_.load();
}

string EventChannel::getName() const {
  return name_;
}

EventChannel::NotificationMode EventChannel::getNotificationMode() const {
  return notificationMode_;
}

} // namespace os
} // namespace vrs
