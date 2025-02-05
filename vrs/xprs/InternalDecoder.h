// (c) Meta Platforms, Inc. and affiliates. Confidential and proprietary.

#pragma once

#include <stdint.h>
#include <stdio.h>

namespace xprs {

class Picture;

class InternalDecoder {
 public:
  virtual ~InternalDecoder() {}
  virtual void open() = 0;
  virtual void decode(uint8_t* buffer, size_t size, Picture& pix) = 0;
};

} // namespace xprs
