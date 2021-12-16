// Copyright (c) Meta Platforms, Inc. and affiliates.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <vrs/os/Time.h>

#include <chrono>

#include <vrs/os/Platform.h>

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

} // namespace os
} // namespace vrs
