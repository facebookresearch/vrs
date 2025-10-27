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

#include <vrs/os/Time.h>

#include <chrono>

#include <vrs/os/Platform.h>

#if IS_MAC_PLATFORM() || IS_LINUX_PLATFORM()
#include <sys/resource.h>
#elif IS_WINDOWS_PLATFORM()
#include <Windows.h>
#endif

namespace vrs {
namespace os {

#if IS_VRS_OSS_CODE()
double getTimestampSec() {
  using namespace std::chrono;
  return duration_cast<duration<double>>(steady_clock::now().time_since_epoch()).count();
}
#endif

int64_t getCurrentTimeSecSinceEpoch() {
  using namespace std::chrono;
  return duration_cast<seconds>(system_clock::now().time_since_epoch()).count();
}

int64_t getTimestampMs() {
  using namespace std::chrono;
  return duration_cast<std::chrono::milliseconds>(steady_clock::now().time_since_epoch()).count();
}

bool getProcessCpuTimes(double& outUserCpuTime, double& outSystemCpuTime) {
#if IS_MAC_PLATFORM() || IS_LINUX_PLATFORM()
  struct rusage r_usage{};
  getrusage(RUSAGE_SELF, &r_usage);
  const double cMicroseconds = 1e-6;
  outUserCpuTime = r_usage.ru_utime.tv_sec + r_usage.ru_utime.tv_usec * cMicroseconds;
  outSystemCpuTime = r_usage.ru_stime.tv_sec + r_usage.ru_stime.tv_usec * cMicroseconds;
  return true;

#elif IS_WINDOWS_PLATFORM()
  FILETIME start;
  FILETIME exit;
  ULARGE_INTEGER kernelTime;
  ULARGE_INTEGER userTime;

  if (GetProcessTimes(
          GetCurrentProcess(),
          &start,
          &exit,
          reinterpret_cast<PFILETIME>(&kernelTime),
          reinterpret_cast<PFILETIME>(&userTime)) == 0) {
    outUserCpuTime = 0;
    outSystemCpuTime = 0;
    return false;
  }

  const double cSecFactor = 1e-7; // source is a count of 0.1us
  outUserCpuTime = userTime.QuadPart * cSecFactor;
  outSystemCpuTime = kernelTime.QuadPart * cSecFactor;
  return true;

#else
  outUserCpuTime = 0;
  outSystemCpuTime = 0;
  return false;
#endif
}

double getTotalProcessCpuTime() {
  double user = 0;
  double kernel = 0;
  if (!getProcessCpuTimes(user, kernel)) {
    return 0;
  }
  return user + kernel;
}

} // namespace os
} // namespace vrs
