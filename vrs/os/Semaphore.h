// (c) Facebook, Inc. and its affiliates. Confidential and proprietary.

#pragma once

#include <vrs/os/Platform.h>

#if IS_VRS_OSS_TARGET_PLATFORM()

#include <boost/interprocess/sync/interprocess_semaphore.hpp>

namespace vrs::os {

class Semaphore : private boost::interprocess::interprocess_semaphore {
 public:
  explicit Semaphore(unsigned int initialCount) : interprocess_semaphore(initialCount) {}

  using boost::interprocess::interprocess_semaphore::post;
  using boost::interprocess::interprocess_semaphore::wait;

  bool timedWait(const double timeSec);
  bool timed_wait(const boost::posix_time::ptime& absTime) = delete;
};

} // namespace vrs::os

#else
#include "Semaphore_fb.h"
#endif
