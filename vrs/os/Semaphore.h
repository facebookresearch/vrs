// Facebook Technologies, LLC Proprietary and Confidential.

#pragma once

#include <vrs/os/Platform.h>

#if IS_VRS_OSS_TARGET_PLATFORM()

#include <boost/interprocess/sync/interprocess_semaphore.hpp>

namespace vrs::os {

class Semaphore : private boost::interprocess::interprocess_semaphore {
 public:
  explicit Semaphore(unsigned int a_initial_count) : interprocess_semaphore(a_initial_count) {}

  using boost::interprocess::interprocess_semaphore::post;
  using boost::interprocess::interprocess_semaphore::wait;

  bool timedWait(const double a_time_sec);
  bool timed_wait(const boost::posix_time::ptime& abs_time) = delete;
};

} // namespace vrs::os

#else
#include "Semaphore_fb.h"
#endif
