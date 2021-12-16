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

#pragma once

#include <cstring>

#include <xxhash.h>
#include <array>
#include <string>
#include <vector>

namespace vrs {

class XXH64Digester {
 public:
  XXH64Digester();
  ~XXH64Digester();
  void clear();
  XXH64Digester& update(const void* data, size_t len);
  template <class T>
  XXH64Digester& update(const std::vector<T>& data) {
    if (!data.empty()) {
      update(data.data(), data.size() * sizeof(T));
    }
    return *this;
  }
  XXH64Digester& update(const std::string& str) {
    return update(str.c_str(), str.size() + 1);
  }
  uint64_t digest();
  std::string digestToString();

 private:
  XXH64_state_t* xxh_{nullptr};
};

} // namespace vrs
