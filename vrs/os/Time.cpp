// Copyright (c) Facebook, Inc. and its affiliates. Confidential and proprietary.

#include <vrs/os/Time.h>

#include <chrono>

#include <vrs/os/Platform.h>

namespace vrs::os {

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

} // namespace vrs::os
