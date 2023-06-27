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

#pragma once

#include <vrs/os/Platform.h>

#include <boost/date_time/posix_time/posix_time_types.hpp>
#include <boost/interprocess/sync/interprocess_semaphore.hpp>

namespace vrs {
namespace os {

class Semaphore : private boost::interprocess::interprocess_semaphore {
 public:
  explicit Semaphore(unsigned int initialCount) : interprocess_semaphore(initialCount) {}

  using boost::interprocess::interprocess_semaphore::post;
  using boost::interprocess::interprocess_semaphore::wait;

  bool timedWait(const double timeSec);
  bool timed_wait(const boost::posix_time::ptime& absTime) = delete;
};

} // namespace os
} // namespace vrs
