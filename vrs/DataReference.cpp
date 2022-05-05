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

#include "DataReference.h"

#include <cstring>

#include "FileHandler.h"

namespace vrs {

void DataReference::useRawData(void* data1, uint32_t size1, void* data2, uint32_t size2) {
  data1_ = data1;
  size1_ = size1;
  data2_ = data2;
  size2_ = size2;
}

void DataReference::copyTo(void* destination) const {
  if (size1_ > 0) {
    memcpy(destination, data1_, size1_);
  }
  if (size2_ > 0) {
    memcpy(reinterpret_cast<char*>(destination) + size1_, data2_, size2_);
  }
}

int DataReference::readFrom(FileHandler& file, uint32_t& outReadSize) {
  int error = 0;
  outReadSize = 0;
  if (size1_ > 0) {
    error = file.read(data1_, size1_);
    outReadSize = static_cast<uint32_t>(file.getLastRWSize());
    if (error != 0) {
      return error;
    }
  }
  if (size2_ > 0) {
    error = file.read(data2_, size2_);
    outReadSize += static_cast<uint32_t>(file.getLastRWSize());
  }
  return error;
}

} // namespace vrs
