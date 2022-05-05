/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <vrs/os/Semaphore.h>

#if IS_VRS_OSS_TARGET_PLATFORM()

#include <boost/date_time/posix_time/posix_time.hpp>

namespace vrs {
namespace os {

bool Semaphore::timedWait(const double timeSec) {
  using namespace boost::posix_time;
  long integralTime = static_cast<long>(std::floor(timeSec));
  long microSeconds = static_cast<long>((timeSec - integralTime) * 1000000);
  return interprocess_semaphore::timed_wait(
      microsec_clock::universal_time() + seconds(integralTime) + microseconds(microSeconds));
}

} // namespace os
} // namespace vrs

#endif
