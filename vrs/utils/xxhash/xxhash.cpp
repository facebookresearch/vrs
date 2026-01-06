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

#include <vrs/utils/xxhash/xxhash.h>

#include <cstring>

#include <fmt/format.h>

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

XXH64Digester& XXH64Digester::ingest(const void* data, size_t len) {
  XR_CHECK_EQ(XXH64_update(xxh_, static_cast<const uint8_t*>(data), len), 0);
  return *this;
}

XXH64Digester& XXH64Digester::ingest(const std::map<string, string>& data) {
  const char* kSignature = "map<string, string>";
  ingest(kSignature, strlen(kSignature));
  for (const auto& iter : data) {
    ingest(iter.first);
    ingest(iter.second);
  }
  return *this;
}

uint64_t XXH64Digester::digest() {
  uint64_t xxHash64 = XXH64_digest(xxh_);
  clear();
  return xxHash64;
}

string XXH64Digester::digestToString() {
  return fmt::format("{:016x}", digest());
}

} // namespace vrs
