// Facebook Technologies, LLC Proprietary and Confidential.

#include <vrs/os/Event.h>

#include <condition_variable>

#include <vrs/os/Time.h>

namespace vrs::os {

EventChannel::EventChannel(std::string a_name, NotificationMode a_notification_mode)
    : m_name{a_name},
      m_notification_mode{a_notification_mode},
      m_num_events_since_last_wait{0},
      m_num_entering{0},
      m_num_listeners{0},
      m_in_destruction{false},
      m_most_recent_event{nullptr, 0, 0, this, 0.0},
      m_pending_wakeup_count{0} {}

EventChannel::~EventChannel() {
  std::unique_lock<std::mutex> lock(m_lock);
  m_in_destruction = true;
  while (m_num_entering + m_num_listeners > 0) {
    // It's not safe to call waitForEvent on an EventChannel that can be concurrently
    // destroyed; we might be really unlucky and it doesn't enter waitForEvent
    // until after the destruction is complete.  On the other hand, if there are
    // pending waitForEvent-s it is better to wake them up than to crash.
    m_wakeup_cond.notify_all();
    m_enter_cond.notify_all();
    std::condition_variable dummy_cond;
    dummy_cond.wait_for(lock, std::chrono::duration<double>(0.001));
  }
}

void EventChannel::dispatchEvent(void* a_pointer, int64_t a_value) {
  std::unique_lock<std::mutex> lock(m_lock);

  m_most_recent_event.pointer = a_pointer;
  m_most_recent_event.value = a_value;
  m_most_recent_event.timestamp_sec = vrs::os::getTimestampSec();
  if (m_num_listeners == 0) {
    m_num_events_since_last_wait++;
  } else if (NotificationMode::BROADCAST == m_notification_mode) {
    m_pending_wakeup_count = m_num_listeners;
    m_wakeup_cond.notify_all();
  } else {
    m_pending_wakeup_count = 1;
    m_wakeup_cond.notify_one();
  }
}

void EventChannel::dispatchEvent(int64_t a_value) {
  dispatchEvent(nullptr, a_value);
}

EventChannel::Status
EventChannel::waitForEvent(Event& a_event, double a_timeout_sec, double a_look_back_sec) {
  EventChannel::Status status = Status::SUCCESS;
  double startTime = vrs::os::getTimestampSec();

  std::unique_lock<std::mutex> lock(m_lock);

  // Pending wake-up is when broadcast event is dispatched but not all listeners have waked up yet.
  // New listener should not enter critical section before all pre-existing listeners have waked up.
  if (m_pending_wakeup_count > 0) {
    m_num_entering++;
    m_enter_cond.wait(lock, [=] { return m_pending_wakeup_count == 0; });
    m_num_entering--;
  }

  // At here, we still have the lock.
  double currentTime = vrs::os::getTimestampSec();
  double time_diff = currentTime - m_most_recent_event.timestamp_sec;
  // If the most recent event is within the look-back range of multiple listeners, then exchange
  // guarantees only one listener can get that past event.
  uint32_t num_events_since_last_wait = m_num_events_since_last_wait.exchange(0);
  if ((time_diff < a_look_back_sec) && (num_events_since_last_wait > 0)) {
    // Fulfill the wait request with a past event.
    a_event = m_most_recent_event;
    a_event.num_missed_events =
        num_events_since_last_wait - 1; // -1 since we did not miss the most recent one
  } else {
    double actualWaitTime = a_timeout_sec - (currentTime - startTime);
    if (actualWaitTime < 0) {
      status = Status::TIMEOUT;
    } else {
      // At this point, m_num_events_since_last_wait is already set to 0.
      m_num_listeners++;
      bool ret = m_wakeup_cond.wait_for(lock, std::chrono::duration<double>(actualWaitTime), [=] {
        return m_in_destruction || m_pending_wakeup_count > 0;
      });
      m_num_listeners--;
      if (m_in_destruction) {
        // This EventChannel is already being destructed, thus this wake-up is not a real event.
        return Status::FAILURE;
      } else if (ret == false) {
        status = Status::TIMEOUT;
      } else {
        if (--m_pending_wakeup_count == 0 && m_num_entering > 0) {
          m_enter_cond.notify_all();
        }
      }
    }

    // Caller won't care about a_event if sleep resulted in a timeout.
    a_event = m_most_recent_event;
    a_event.num_missed_events = num_events_since_last_wait;
  }

  return status;
}

uint32_t EventChannel::getNumEventsSinceLastWait() const {
  return m_num_events_since_last_wait.load();
}

std::string EventChannel::getName() const {
  return m_name;
}

EventChannel::NotificationMode EventChannel::getNotificationMode() const {
  return m_notification_mode;
}

} // namespace vrs::os
