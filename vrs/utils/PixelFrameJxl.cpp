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

#include "PixelFrame.h"

#define DEFAULT_LOG_CHANNEL "PixelFrameJxl"
#include <logging/Log.h>
#include <logging/Verify.h>

namespace {

vrs::utils::PixelFrameDecoder& jxlDecoder() {
  static vrs::utils::PixelFrameDecoder sJxlDecoder = nullptr;
  return sJxlDecoder;
}

} // namespace

namespace vrs::utils {

using namespace std;

void PixelFrame::setJxlDecoder(const PixelFrameDecoder& decoder) {
  jxlDecoder() = decoder;
}

bool PixelFrame::readJxlFrame(RecordReader* reader, const uint32_t sizeBytes) {
  if (sizeBytes == 0) {
    return false; // empty image
  }
  // read JPEG-XL data
  vector<uint8_t> jxlBuf;
  jxlBuf.resize(sizeBytes);
  if (!XR_VERIFY(reader->read(jxlBuf.data(), sizeBytes) == 0)) {
    return false;
  }
  return readJxlFrame(jxlBuf);
}

bool PixelFrame::readJxlFrame(const vector<uint8_t>& jxlBuf, bool decodePixels) {
  const vrs::utils::PixelFrameDecoder& decoder = jxlDecoder();
  if (decoder) {
    return decoder(*this, jxlBuf, decodePixels);
  }
  XR_LOGW_EVERY_N_SEC(10, "Can't decode jxl frame, because no jxl decoder was provided.");
  return false;
}

} // namespace vrs::utils
