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

#include <vector>

#include <vrs/RecordFormat.h>
#include <vrs/RecordReaders.h>

using OpusEncoder = struct OpusEncoder;
using OpusDecoder = struct OpusDecoder;

namespace vrs::utils {

using std::vector;

struct AudioCompressionHandler {
  OpusEncoder* encoder{};
  AudioContentBlockSpec encoderSpec;

  bool create(const AudioContentBlockSpec& spec);
  int compress(const void* samples, uint32_t sampleCount, void* outOpusBytes, size_t maxBytes);

  ~AudioCompressionHandler();
};

struct AudioDecompressionHandler {
  OpusDecoder* decoder{};
  AudioContentBlockSpec decoderSpec;

  ~AudioDecompressionHandler();
};

/// Helper class to read & convert audio blocks.
class AudioBlock {
 public:
  AudioBlock() = default;
  AudioBlock(AudioBlock&& other) noexcept = default;
  AudioBlock(const AudioContentBlockSpec& spec, vector<uint8_t>&& frameBytes);
  explicit AudioBlock(const AudioContentBlockSpec& spec);
  AudioBlock(
      AudioFormat audioFormat,
      AudioSampleFormat sampleFormat,
      uint8_t channelCount = 0,
      uint8_t sampleFrameStride = 0,
      uint32_t sampleRate = 0,
      uint32_t sampleCount = 0)
      : AudioBlock(AudioContentBlockSpec(
            audioFormat,
            sampleFormat,
            channelCount,
            sampleFrameStride,
            sampleRate,
            sampleCount)) {}

  void init(const AudioContentBlockSpec& spec);
  inline void init(
      AudioFormat audioFormat,
      AudioSampleFormat sampleFormat,
      uint8_t channelCount = 0,
      uint8_t sampleFrameStride = 0,
      uint32_t sampleRate = 0,
      uint32_t sampleCount = 0) {
    init(AudioContentBlockSpec(
        audioFormat, sampleFormat, channelCount, sampleFrameStride, sampleRate, sampleCount));
  }
  void init(const AudioContentBlockSpec& spec, vector<uint8_t>&& frameBytes);

  void swap(AudioBlock& other) noexcept;
  AudioBlock& operator=(AudioBlock&& other) = default;

  const AudioContentBlockSpec& getSpec() const {
    return audioSpec_;
  }
  AudioFormat getAudioFormat() const {
    return audioSpec_.getAudioFormat();
  }
  AudioSampleFormat getSampleFormat() const {
    return audioSpec_.getSampleFormat();
  }
  uint8_t getChannelCount() const {
    return audioSpec_.getChannelCount();
  }
  uint32_t getSampleRate() const {
    return audioSpec_.getSampleRate();
  }
  uint8_t getSampleFrameStride() const {
    return audioSpec_.getSampleFrameStride();
  }
  uint32_t getSampleCount() const {
    return audioSpec_.getSampleCount();
  }
  void setSampleCount(uint32_t sampleCount) {
    audioSpec_.setSampleCount(sampleCount);
    allocateBytes();
  }

  vector<uint8_t>& getBuffer() {
    return audioBytes_;
  }
  const uint8_t* rdata() const {
    return audioBytes_.data();
  }
  uint8_t* wdata() {
    return audioBytes_.data();
  }
  template <class T>
  const T* data(size_t byte_offset = 0) const {
    return reinterpret_cast<const T*>(audioBytes_.data() + byte_offset);
  }
  template <class T>
  T* data(size_t byte_offset = 0) {
    return reinterpret_cast<T*>(audioBytes_.data() + byte_offset);
  }
  size_t size() const {
    return audioBytes_.size();
  }
  uint8_t* getSample(uint32_t sampleIndex) {
    return audioBytes_.data() + audioSpec_.getSampleFrameStride() * sampleIndex;
  }

  /// Clear the audio sample buffer
  void clearBuffer();

  /// Read the audio content block (no decoding).
  /// @return True if the audio block type is supported & the audio block was read.
  bool readBlock(RecordReader* reader, const ContentBlock& cb);

  /// From any supported AudioFormat, decompress the audio block to AudioFormat::PCM if necessary.
  bool decompressAudio(AudioDecompressionHandler& handler);

  /// Decode an Opus encoded audio block into the internal buffer.
  /// @param handler: Compression/decompression handler to be reused for that audio stream.
  /// @param outAudioBlock: On success, on exit, set to the audio extracted.
  /// @return True only if the audio block was decompressed and outAudioBlock is valid (success).
  bool opusDecompress(AudioDecompressionHandler& handler, AudioBlock& outAudioBlock);

 private:
  void allocateBytes();

  AudioContentBlockSpec audioSpec_;
  vector<uint8_t> audioBytes_;
};

} // namespace vrs::utils
