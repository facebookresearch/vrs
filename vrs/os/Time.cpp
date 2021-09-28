// Copyright (c) Facebook, Inc. and its affiliates. Confidential and proprietary.

#include <chrono>

#include <visiontypes/clock/ProcessingClock.h>
#include <vrs/os/Time.h>

namespace vrs::os {

double getTimestampSec() {
  return visiontypes::ProcessingClock::now().asSeconds();
}

int64_t getCurrentTimeSecSinceEpoch() {
  using namespace std::chrono;
  return duration_cast<seconds>(system_clock::now().time_since_epoch()).count();
}

} // namespace vrs::os
