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
#include <limits>
#include <mutex>
#include <string>

namespace vrs {
namespace os {

/// EventChannel represents a type of event, which can dispatch instances of event.
class EventChannel {
 public:
  enum class NotificationMode {
    UNICAST, // Only one (unspecified) listener gets event instance
    BROADCAST // All listeners can get the event instance
  };
  enum class Status { SUCCESS = 0, FAILURE = -1, TIMEOUT = -2, INVALID = -3 };

  static const uint32_t kInfiniteTimeout = std::numeric_limits<uint32_t>::max();

  /// Event represents an instance of an event.
  struct Event {
    void* pointer;
    int64_t value;
    int32_t numMissedEvents;
    double timestampSec;
  };

  /// Construct EventChannel.
  /// @param notificationMode: whether event can be unicasted or broadcasted
  /// @param name: name of event. name should better be unique but nothing enforces that.
  EventChannel(std::string name, NotificationMode notificationMode);
  ~EventChannel();

  /// Fires an event instance to listener(s).
  /// @param pointer: pointer to the data to pass along with the event
  /// @param value: an int64_t value to pass along with the event
  void dispatchEvent(void* pointer = nullptr, int64_t value = 0);
  void dispatchEvent(int64_t value);

  /// Wait to get an instance of event, in future or in past.
  /// The number of past events is in returned Param. The counter in event is then set to 0
  /// when someone successfully gets an instance of event.
  /// In broadcast mode, multiple listeners can get a future event, but one listener can get
  /// a past event.
  /// Of note, do not use kInfiniteTimeout as timeout. If nobody dispatches event, your thread will
  /// get stuck here for an intolerable amount of time.
  /// @param event: used to get info about the event instance when returns
  /// @param timeoutSec: maximum length of time in second to wait
  /// @param lookBackSec: the max length of time to look back for most recent past event
  /// @return A status indicator of success/timeout/etc.
  Status waitForEvent(Event& event, double timeoutSec, double lookBackSec = 0.0);

  /// Return the number of past event since last waiting of event without actually waiting
  /// for that event.
  /// @return Number of past events
  uint32_t getNumEventsSinceLastWait() const;

  /// Return the name of this event type.
  /// @return Name of this even type
  std::string getName() const;

  /// Return if single or multiple listeners will be notified by one dispatch.
  /// @return: mode of notification
  NotificationMode getNotificationMode() const;

 private:
  std::string name_;
  NotificationMode notificationMode_;

  std::mutex mutex_; // guards all of the following variables
  std::atomic_uint_fast32_t numEventsSinceLastWait_; // mutex_ only needed for write

  uint32_t numEntering_; // count waiting for previous dispatch to complete
  uint32_t numListeners_; // count waiting for dispatch
  bool inDestruction_; // true if destructor is running

  Event mostRecentEvents_;
  uint32_t pendingWakeupsCount_;

  // Notify when entering waitForEvent can continue, which is when
  // pendingWakeupsCount_ becomes zero and numEntering_ is non-zero.
  std::condition_variable enterCondition_;

  std::condition_variable wakeupCondition_; // notify when pendingWakeupsCount_ becomes non-zero
};

} // namespace os
} // namespace vrs
