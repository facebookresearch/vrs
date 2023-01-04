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

#include "MemBuffer.h"

#define DEFAULT_LOG_CHANNEL "MemBuffer"
#include <logging/Log.h>
#include <logging/Verify.h>

using namespace std;

namespace vrs {
namespace helpers {

MemBuffer::MemBuffer(size_t allocSize) : allocSize_{allocSize} {}

void MemBuffer::addData(const void* data, size_t size) {
  reserve(size);
  auto& back = buffers_.back();
  size_t previousSize = back.size();
  back.resize(previousSize + size);
  memcpy(back.data() + previousSize, data, size);
}

size_t MemBuffer::allocateSpace(uint8_t*& outData, size_t minSize) {
  reserve(minSize);
  auto& back = buffers_.back();
  outData = &back.data()->byte + back.size();
  return back.capacity() - back.size();
}

void MemBuffer::addAllocatedSpace(size_t size) {
  if (XR_VERIFY(!buffers_.empty())) {
    buffers_.back().resize(buffers_.back().size() + size);
  }
}

size_t MemBuffer::getSize() const {
  size_t size = 0;
  for (const auto& buffer : buffers_) {
    size += buffer.size();
  }
  return size;
}

void MemBuffer::getData(vector<uint8_t>& outData) {
  if (buffers_.size() == 1) {
    outData.swap(reinterpret_cast<vector<uint8_t>&>(buffers_.front()));
  } else {
    size_t totalSize = getSize();
    outData.resize(totalSize);
    uint8_t* outPtr = outData.data();
    for (const auto& buffer : buffers_) {
      memcpy(outPtr, buffer.data(), buffer.size());
      outPtr += buffer.size();
    }
    buffers_.clear();
  }
}

void MemBuffer::reserve(size_t size) {
  if (buffers_.empty() || buffers_.back().capacity() - buffers_.back().size() < size) {
    buffers_.emplace_back();
    buffers_.back().reserve(max<size_t>(size, allocSize_));
  }
}

} // namespace helpers
} // namespace vrs
