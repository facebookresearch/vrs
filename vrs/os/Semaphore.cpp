// Copyright (c) Facebook, Inc. and its affiliates. Confidential and proprietary.

#include <vrs/os/Semaphore.h>

#if IS_VRS_OSS_TARGET_PLATFORM()

#include <boost/date_time/posix_time/posix_time.hpp>

namespace vrs::os {

bool Semaphore::timedWait(const double timeSec) {
  using namespace boost::posix_time;
  long integralTime = static_cast<long>(std::floor(timeSec));
  long microSeconds = static_cast<long>((timeSec - integralTime) * 1000000);
  return interprocess_semaphore::timed_wait(
      microsec_clock::universal_time() + seconds(integralTime) + microseconds(microSeconds));
}

} // namespace vrs::os

#endif
