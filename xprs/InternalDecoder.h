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

#pragma once

#include <cstdint>
#include <cstdio>

namespace xprs {

class Picture;

class InternalDecoder {
 public:
  virtual ~InternalDecoder() = default;
  virtual void open() = 0;
  virtual void decode(uint8_t* buffer, size_t size, Picture& pix) = 0;
  virtual bool isHwAccelerated() {
    return false;
  }
  /// Flush the decoder's internal state (e.g., decoded picture buffer).
  /// Call this when seeking backward to avoid duplicate POC errors.
  virtual void flush() {}
};

} // namespace xprs
