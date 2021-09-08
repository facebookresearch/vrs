// Facebook Technologies, LLC Proprietary and Confidential.

#pragma once

#include <atomic>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>

#include <vrs/os/Event.h>
#include <vrs/os/Semaphore.h>

namespace vrs {

using std::deque;
using std::function;

/**
  A PlaybackThread manages a thread of its own, to which PlaybackRecords can be posted.
  It is associated with a clock, which may or may not be realtime.
  (The clock is probably the current time offset by a constant.)
  A PlaybackRecord is an object that has a playback time, which tells when its playback() method
  is expected to be called in the PlaybackThread's thread and clock's time domain.
*/
class PlaybackThread {
 public:
  class PlaybackRecord {
   public:
    PlaybackRecord(double time) : playbackTime_(time) {}
    virtual ~PlaybackRecord();

    virtual void playback() = 0;

    double getPlaybackTime() const {
      return playbackTime_;
    }

   private:
    double playbackTime_;
  };

  PlaybackThread(function<double()> clock, uint32_t maxQueueSize);
  ~PlaybackThread();

  /// Post a PlaybackRecord to be played at its own time in the PlaybackThread's thread.
  /// The object will be deleted after playback.
  /// If more than maxQueueSize_ PlaybackRecords are queued,
  /// this method will block until records have been played.
  /// This will prevent loading records too quickly and consume memory and CPU cycles too early.
  /// maxQueueSize_ should probably allow for a fraction of second (0.5?) of playback for that
  /// particular type of records.
  void post(PlaybackRecord* playbackRecord);

  /// Finish playing back all the PlaybackRecords, quit the playback thread, and return.
  void finishPlayback();

  void waitForPlaybackToFinish();

  /// Stop playing back records, quit the playback thread ASAP, delete unplayed records and return.
  void abortPlayback();

  void playbackThreadActivity();

  void setClock(function<double()> clock) {
    clock_ = clock;
  }

 private:
  void cleanupThread();

  function<double()> clock_;
  uint32_t maxQueueSize_;
  std::recursive_mutex mutex_;
  os::Semaphore writeSemaphore_; ///< to control the max number of writes
  os::Semaphore readSemaphore_; ///< to control reading
  os::EventChannel waitEvent_; ///< Use to wait while being wakable
  std::atomic<bool> endThread_; ///< set to end the thread ASAP
  std::atomic<bool> emptyQueueAndEndThread_; ///< set to end the thread when the queue is empty
  deque<std::unique_ptr<PlaybackRecord>> playbackRecordQueue_;
  std::thread thread_;
};

} // namespace vrs
