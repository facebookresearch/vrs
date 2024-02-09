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

#include "AudioBlock.h"

#include <cstring>

#define DEFAULT_LOG_CHANNEL "AudioBlock"
#include <logging/Checks.h>
#include <logging/Log.h>
#include <logging/Verify.h>

#include <vrs/helpers/FileMacros.h>
#include <vrs/helpers/Throttler.h>

using namespace std;
using namespace vrs;

namespace {

utils::Throttler& getThrottler() {
  static utils::Throttler sThrottler;
  return sThrottler;
}

} // namespace

namespace vrs::utils {

AudioBlock::AudioBlock(const AudioContentBlockSpec& spec, vector<uint8_t>&& frameBytes)
    : audioSpec_{spec}, audioBytes_{std::move(frameBytes)} {
  size_t size = audioSpec_.getBlockSize();
  THROTTLED_VERIFY(nullptr, size == ContentBlock::kSizeUnknown || size == audioBytes_.size());
}

void AudioBlock::init(const AudioContentBlockSpec& spec) {
  audioSpec_ = spec;
  size_t size = audioSpec_.getBlockSize();
  if (size != ContentBlock::kSizeUnknown) {
    audioBytes_.resize(size);
  }
}

void AudioBlock::init(const AudioContentBlockSpec& spec, vector<uint8_t>&& frameBytes) {
  audioSpec_ = spec;
  audioBytes_ = std::move(frameBytes);
  size_t size = audioSpec_.getBlockSize();
  THROTTLED_VERIFY(nullptr, size == ContentBlock::kSizeUnknown || size == audioBytes_.size());
}

void AudioBlock::swap(AudioBlock& other) noexcept {
  AudioContentBlockSpec tempSpec = other.audioSpec_;
  other.audioSpec_ = audioSpec_;
  audioSpec_ = tempSpec;
  audioBytes_.swap(other.audioBytes_);
}

void AudioBlock::clearBuffer() {
  if (!audioBytes_.empty()) {
    memset(wdata(), 0, audioBytes_.size());
  }
}

bool AudioBlock::readBlock(RecordReader* reader, const ContentBlock& cb) {
  if (!THROTTLED_VERIFY(reader->getRef(), cb.getContentType() == ContentType::AUDIO)) {
    return false;
  }
  const auto& spec = cb.audio();
  size_t blockSize = cb.getBlockSize();
  audioSpec_ = spec;
  audioBytes_.resize(blockSize);
  return THROTTLED_VERIFY(reader->getRef(), reader->read(audioBytes_.data(), blockSize) == 0);
}

} // namespace vrs::utils
