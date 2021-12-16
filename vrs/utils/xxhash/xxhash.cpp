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

#include <vrs/utils/xxhash/xxhash.h>
#include <iomanip>
#include <sstream>

#define XXH_INLINE_ALL
#define DEFAULT_LOG_CHANNEL "xxhash"
#include <logging/Checks.h>

using namespace std;

namespace vrs {
XXH64Digester::XXH64Digester() {
  xxh_ = XXH64_createState();
  XR_CHECK_NOTNULL(xxh_);
  XXH64_reset(xxh_, 0);
}

XXH64Digester::~XXH64Digester() {
  clear();
}

void XXH64Digester::clear() {
  if (xxh_ != nullptr) {
    XXH64_freeState(xxh_);
    xxh_ = nullptr;
  }
}

XXH64Digester& XXH64Digester::update(const void* data, size_t len) {
  XR_CHECK_EQ(XXH64_update(xxh_, static_cast<const uint8_t*>(data), len), 0);
  return *this;
}

uint64_t XXH64Digester::digest() {
  uint64_t xxHash64 = XXH64_digest(xxh_);
  clear();
  return xxHash64;
}

string XXH64Digester::digestToString() {
  std::stringstream stream;
  uint64_t xxHash64 = digest();
  stream << std::setfill('0') << std::setw(sizeof(uint64_t) * 2) << std::hex << xxHash64;
  return stream.str();
}

} // namespace vrs
