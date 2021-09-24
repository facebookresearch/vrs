// (c) Facebook, Inc. and its affiliates. Confidential and proprietary.

#include <vrs/os/Semaphore.h>

#if IS_VRS_OSS_TARGET_PLATFORM()

#include <boost/date_time/posix_time/posix_time.hpp>

namespace vrs::os {

bool Semaphore::timedWait(const double a_time_sec) {
  using namespace boost::posix_time;
  long integral_time = static_cast<long>(std::floor(a_time_sec));
  long micro_seconds = static_cast<long>((a_time_sec - integral_time) * 1000000);
  return interprocess_semaphore::timed_wait(
      microsec_clock::universal_time() + seconds(integral_time) + microseconds(micro_seconds));
}

} // namespace vrs::os

#endif
