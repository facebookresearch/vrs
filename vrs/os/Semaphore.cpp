// Facebook Technologies, LLC Proprietary and Confidential.

#include <vrs/os/Semaphore.h>

#ifndef __XROS__
#include <boost/date_time/posix_time/posix_time.hpp>
#else
#include <xr/time.h>
#endif

namespace vrs::os {

#ifndef __XROS__

bool Semaphore::timedWait(const double a_time_sec) {
  using namespace boost::posix_time;
  long integral_time = static_cast<long>(std::floor(a_time_sec));
  long micro_seconds = static_cast<long>((a_time_sec - integral_time) * 1000000);
  return interprocess_semaphore::timed_wait(
      microsec_clock::universal_time() + seconds(integral_time) + microseconds(micro_seconds));
}

#else

Semaphore::Semaphore(unsigned int a_initial_count) {
  xr_semaphore_init(&sem_, a_initial_count);
}

bool Semaphore::timedWait(const double a_time_sec) {
  if (a_time_sec < 0) {
    return false;
  }

  const xr_time_t deadline = xr_deadline(XR_NSEC(static_cast<uint64_t>(a_time_sec * 1e9)));
  xr_error_t err;
  do {
    err = xr_semaphore_try_acquire_until(&sem_, deadline);
  } while (err == XR_ERR_INTERRUPTED);
  XR_ASSERT(err == XR_OK || err == XR_ERR_TIMED_OUT);
  return err == XR_OK;
}
void Semaphore::post() {
  xr_semaphore_acquire_uninterruptible(&sem_);
}
void Semaphore::wait() {
  xr_semaphore_release(&sem_);
}

#endif

} // namespace vrs::os
