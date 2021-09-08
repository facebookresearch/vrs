// Facebook Technologies, LLC Proprietary and Confidential.

// BOOST_USE_WINDOWS_H must be defined first before we include boost interprocess
// or <windows.h> anywhere, which is why this section must be done before we
// include "PlaybackThread.h"
#include <portability/Platform.h>

#if IS_WINDOWS_PLATFORM()
#define BOOST_USE_WINDOWS_H
#include <windows.h>
#ifdef WIN32_LEAN_AND_MEAN
#include <mmsystem.h>
#endif
#pragma comment(lib, "Winmm.lib")
#endif

#include "PlaybackThread.h"

#include <mutex>

namespace vrs {

static const double kMinDelayTime = 0.001; // under that time, play the record right now

PlaybackThread::PlaybackThread(function<double()> clock, uint32_t maxQueueSize)
    : clock_{clock},
      maxQueueSize_{maxQueueSize},
      writeSemaphore_{maxQueueSize_}, // all the queue slots are available
      readSemaphore_{0}, // nothing to read yet
      waitEvent_{"PlaybackThreadWaitEvent", os::EventChannel::NotificationMode::UNICAST},
      endThread_{false},
      emptyQueueAndEndThread_{false},
      thread_{&PlaybackThread::playbackThreadActivity, this} {}

PlaybackThread::~PlaybackThread() {
  abortPlayback();
}

void PlaybackThread::post(PlaybackRecord* playbackRecord) {
  writeSemaphore_.wait(); // wait that there is room in the queue
  {
    std::unique_lock<std::recursive_mutex> guard{mutex_};
    playbackRecordQueue_.emplace_back(playbackRecord);
  }
  readSemaphore_.post();
}

void PlaybackThread::finishPlayback() {
  emptyQueueAndEndThread_ = true;
  bool abort;
  {
    std::unique_lock<std::recursive_mutex> guard{mutex_};
    abort = playbackRecordQueue_.empty(); // if the queue is empty, just quit the thread ASAP
  }
  if (abort) {
    abortPlayback();
  } else {
    cleanupThread();
  }
}

void PlaybackThread::waitForPlaybackToFinish() {
  while (!playbackRecordQueue_.empty()) {
    waitEvent_.dispatchEvent(); // wake if waiting
    readSemaphore_.post();
  }
}

void PlaybackThread::abortPlayback() {
  endThread_ = true;
  waitEvent_.dispatchEvent(); // wake if waiting
  readSemaphore_.post(); // wake the thread if waiting on a record to read
  cleanupThread();
}

void PlaybackThread::playbackThreadActivity() {
#if IS_WINDOWS_PLATFORM()
  ::timeBeginPeriod(1);
#endif
  while (!endThread_) {
    readSemaphore_.timedWait(1000000); // not a big deal if we timeout: we'll just loop
    std::unique_ptr<PlaybackRecord> record;
    if (!endThread_) {
      std::unique_lock<std::recursive_mutex> guard{mutex_};
      if (!playbackRecordQueue_.empty()) {
        record = std::move(playbackRecordQueue_.front());
        playbackRecordQueue_.pop_front();
        writeSemaphore_.post();
      }
      if (emptyQueueAndEndThread_ && playbackRecordQueue_.empty()) {
        endThread_ = true;
      }
    }
    if (record) {
      double delay = record->getPlaybackTime() - clock_();
      if (delay >= kMinDelayTime) {
        os::EventChannel::Event event;
        waitEvent_.waitForEvent(event, delay);
      }
      if (!endThread_) { // may have changed while being in waitForEvent() above
        record->playback();
      }
    }
  }
#if IS_WINDOWS_PLATFORM()
  ::timeEndPeriod(1);
#endif
}

void PlaybackThread::cleanupThread() {
  // always check the joinable state before actually calling join()
  if (thread_.joinable()) {
    thread_.join();
  }
  playbackRecordQueue_.clear();
}

PlaybackThread::PlaybackRecord::~PlaybackRecord() = default;

} // namespace vrs
