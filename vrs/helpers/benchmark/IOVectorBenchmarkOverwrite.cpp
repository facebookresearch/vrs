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

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>

#if defined(_MSC_VER)
#include <intrin.h>
#define VRS_NOINLINE __declspec(noinline)
#else
#define VRS_NOINLINE __attribute__((noinline))
#endif

extern "C" VRS_NOINLINE uint64_t sampleChecksum(const void* storage, size_t byteCount) noexcept;

extern "C" VRS_NOINLINE uint64_t
overwriteAndChecksum(void* storage, size_t byteCount, uint8_t value) noexcept {
  if (storage == nullptr) {
    std::abort();
  }
  std::memset(storage, value, byteCount);
#if defined(_MSC_VER)
  _ReadWriteBarrier();
#else
  asm volatile("" : : "g"(storage) : "memory");
#endif
  return sampleChecksum(storage, byteCount);
}

extern "C" VRS_NOINLINE uint64_t sampleChecksum(const void* storage, size_t byteCount) noexcept {
  if (storage == nullptr) {
    std::abort();
  }
  const auto* bytes = static_cast<const uint8_t*>(storage);
  uint64_t checksum = 0;
  for (size_t offset = 0; offset < byteCount; offset += 4096) {
    checksum += bytes[offset];
  }
  if (byteCount > 0) {
    checksum += bytes[byteCount - 1];
  }
  return checksum;
}

#undef VRS_NOINLINE
