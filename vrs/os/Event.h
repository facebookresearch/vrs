// (c) Facebook, Inc. and its affiliates. Confidential and proprietary.

#pragma once

#include <atomic>
#include <condition_variable>
#include <limits>
#include <mutex>
#include <string>

namespace vrs::os {
/*
 * EventChannel represents a type of event, which can dispatch instances of event.
 */
class EventChannel {
 public:
  enum class NotificationMode {
    UNICAST, // Only one (unspecified) listener gets event instance
    BROADCAST // All listeners can get the event instance
  };
  enum class Status { SUCCESS = 0, FAILURE = -1, TIMEOUT = -2, INVALID = -3 };

  static const uint32_t kInfiniteTimeout = std::numeric_limits<uint32_t>::max();

  /*
   * Event represents an instance of an event.
   */
  typedef struct event_ {
    void* pointer;
    int64_t value;
    int32_t num_missed_events;
    EventChannel* event_type;
    double timestamp_sec;
  } Event;

  /*
   * Construct EventChannel.
   * @param a_notification_mode - whether event can be unicasted or broadcasted
   * @param a_name - name of event. name should better be unique but nothing enforces that.
   */
  EventChannel(std::string a_name, NotificationMode a_notification_mode);
  ~EventChannel();

  /*
   * Fires an event instance to listener(s).
   * @param a_data - pointer to the data to pass along with the event
   * @param a_value - an int64_t value to pass along with the event
   */
  void dispatchEvent(int64_t a_value);
  void dispatchEvent(void* a_pointer = nullptr, int64_t a_value = 0);

  /*
   * Wait to get an instance of event, in future or in past.
   * The number of past events is in returned Param. The counter in event is then set to 0
   * when someone successfully gets an instance of event.
   * In broadcast mode, multiple listeners can get a future event, but one listener can get
   * a past event.
   * Of note, do not use kInfiniteTimeout as timeout. If nobody dispatches event, your thread will
   * get stuck here for an intolerable amount of time.
   * @param a_event - used to get info about the event instance when returns
   * @param a_timeout_sec - maximum length of time in second to wait
   * @param a_look_back_sec - the max length of time to look back for most recent past event
   * @return - indicator of success/timeout/etc.
   */
  Status waitForEvent(Event& a_event, double a_timeout_sec, double a_look_back_sec = 0.0);

  /*
   * Return the number of past event since last waiting of event without actually waiting
   * for that event.
   * @return - number of past events
   */
  uint32_t getNumEventsSinceLastWait() const;

  /*
   * Return the name of this event type.
   * @return - name of this even type
   */
  std::string getName() const;

  /*
   * Return if single or multiple listeners will be notified by one dispatch.
   * @return - mode of notification
   */
  NotificationMode getNotificationMode() const;

 private:
  std::string m_name;
  NotificationMode m_notification_mode;

  std::mutex m_lock; // guards all of the following variables
  std::atomic_uint_fast32_t m_num_events_since_last_wait; // m_lock only needed for write

  uint32_t m_num_entering; // count waiting for previous dispatch to complete
  uint32_t m_num_listeners; // count waiting for dispatch
  bool m_in_destruction; // true if destructor is running

  Event m_most_recent_event;
  uint32_t m_pending_wakeup_count;

  // Notify when entering waitForEvent can continue, which is when
  // m_pending_wakeup_count becomes zero and m_num_entering is non-zero.
  std::condition_variable m_enter_cond;

  std::condition_variable m_wakeup_cond; // notify when m_pending_wakeup_count becomes non-zero
};

} // namespace vrs::os
