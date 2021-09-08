// Facebook Technologies, LLC Proprietary and Confidential.

#pragma once

#ifndef __XROS__
#include <boost/interprocess/sync/interprocess_semaphore.hpp>
#else
#include <xr/semaphore.h>
#endif

namespace vrs::os {

/// vrs::os::Semaphore does not need to be interprocess.
#ifndef __XROS__
class Semaphore : private boost::interprocess::interprocess_semaphore {
 public:
  explicit Semaphore(unsigned int a_initial_count) : interprocess_semaphore(a_initial_count) {}

  using boost::interprocess::interprocess_semaphore::post;
  using boost::interprocess::interprocess_semaphore::wait;

  bool timedWait(const double a_time_sec);
  bool timed_wait(const boost::posix_time::ptime& abs_time) = delete;
};
#else
class Semaphore {
 public:
  explicit Semaphore(unsigned int a_initial_count);

  bool timedWait(const double a_time_sec);
  void post();
  void wait();

 private:
  xr_semaphore_t sem_;
};
#endif

} // namespace vrs::os
