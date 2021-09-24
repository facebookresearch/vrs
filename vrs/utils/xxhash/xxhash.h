// (c) Facebook, Inc. and its affiliates. Confidential and proprietary.

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
