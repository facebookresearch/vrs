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

#include <deque>
#include <vector>

namespace vrs {
namespace helpers {

/// Helper class to collect data "writen" to it in various sizes.
/// If the allocation size provided is large enough to hold the whole data,
/// memory copy is minized, and the data vector returned is swapped out (no copy),
/// otherwise, one extra copy is required to put all the pieces together in a single
/// contiguous buffer.
/// However, the class guarantees no extra copies, and no initialization overhead.

class MemBuffer {
  struct uninitialized_byte final {
    uninitialized_byte() {} // do not use '= default' as it will initialize byte!
    uint8_t byte;
  };

 public:
  /// Create a MemBuffer with a minimum block allocation size.
  /// If that size is equal or greater than the total data, memory copies are minimized.
  MemBuffer(size_t allocSize = 256 * 1024);

  /// Add block of bytes
  /// @param data: pointer to the data
  /// @param size: size of the data to add, in bytes
  void addData(const void* data, size_t size);
  /// Allocate contiguous block of bytes to write to,
  /// but without counting the data in the buffer yet.
  /// @param outData: on exit, set to point to the buffer to write data to
  /// @param minSize: minimum number of bytes to reserve
  /// @return the number of bytes actually available to write
  size_t allocateSpace(uint8_t*& outData, size_t minSize);
  /// Add a number of allocated bytes to the buffer
  /// @param size: number of preallocated bytes to add to the buffer
  /// This call assumes that the bytes were set externally.
  void addAllocatedSpace(size_t size);

  /// Get the total number of bytes used in the buffer
  size_t getSize() const;
  /// Get the data in the buffer
  /// @param outData: buffer to set with the whole data
  void getData(std::vector<uint8_t>& outData);

 private:
  void reserve(size_t size);

  size_t allocSize_;
  std::deque<std::vector<uninitialized_byte>> buffers_;
};

} // namespace helpers
} // namespace vrs
