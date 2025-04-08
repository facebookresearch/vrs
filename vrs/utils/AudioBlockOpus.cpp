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

#ifdef OPUS_IS_AVAILABLE
extern "C" {
#include <opus.h>
#include <opus_multistream.h>
}
#endif

#define DEFAULT_LOG_CHANNEL "AudioBlockOpus"
#include <logging/Log.h>
#include <logging/Verify.h>

#include <vrs/RecordFormat.h>
#include <vrs/helpers/FileMacros.h>

using namespace std;
using namespace vrs;

namespace {

bool supportedSampleRate(uint32_t rate) {
  return rate == 8000 || rate == 12000 || rate == 16000 || rate == 24000 || rate == 48000;
}

} // namespace

namespace vrs::utils {

#ifdef OPUS_IS_AVAILABLE

bool AudioBlock::opusDecompress(AudioDecompressionHandler& handler, AudioBlock& outAudioBlock) {
  if (getAudioFormat() != AudioFormat::OPUS || getSampleFormat() != AudioSampleFormat::S16_LE ||
      !supportedSampleRate(getSampleRate())) {
    return false;
  }
  if (handler.decoder != nullptr && !handler.decoderSpec.isCompatibleWith(audioSpec_)) {
    opus_multistream_decoder_destroy(handler.decoder);
    handler.decoder = nullptr;
  }

  if (handler.decoder == nullptr) {
    int error = 0;

    uint32_t totalAudioChannel = getChannelCount();
    if (totalAudioChannel > 255 || totalAudioChannel == 0) {
      XR_LOGW("Invalid channel count of {}", totalAudioChannel);
      return false;
    }

    uint32_t totalCoupledAudioChannel = 2 * getStereoPairCount();
    if (totalAudioChannel < totalCoupledAudioChannel) {
      XR_LOGW(
          "Invalid channel count of {} and stereo channel count of {}",
          totalAudioChannel,
          totalCoupledAudioChannel);
      return false;
    }

    uint32_t totalMonoChannel = totalAudioChannel - totalCoupledAudioChannel;
    uint32_t totalAudioStreamCount = totalMonoChannel + getStereoPairCount();

    vector<uint8_t> mapping(getChannelCount());
    for (uint32_t i = 0; i < totalCoupledAudioChannel + totalMonoChannel; ++i) {
      mapping[i] = i;
    }

    handler.decoder = opus_multistream_decoder_create(
        getSampleRate(),
        getChannelCount(),
        totalAudioStreamCount,
        getStereoPairCount(),
        mapping.data(),
        &error);

    if (error != OPUS_OK || handler.decoder == nullptr) {
      XR_LOGW("Couldn't create Opus decoder. Error {}: {}", error, opus_strerror(error));
      return false;
    }
    handler.decoderSpec = audioSpec_;
  }

  uint32_t sampleCount = getSampleCount();
  // if we don't know the sample count, use the maximum possible according to the Opus spec: 120 ms
  if (sampleCount == 0) {
    sampleCount = getSampleRate() * 120 / 1000;
  }

  outAudioBlock.init(
      AudioFormat::PCM,
      AudioSampleFormat::S16_LE,
      getChannelCount(),
      0,
      getSampleRate(),
      sampleCount);
  opus_int32 result = opus_multistream_decode(
      handler.decoder,
      data<unsigned char>(),
      audioBytes_.size(),
      outAudioBlock.data<opus_int16>(),
      sampleCount,
      0);
  if (result > 0) {
    outAudioBlock.setSampleCount(result);
  } else {
    XR_LOGW("Couldn't decode Opus data. Error {}: {}", result, opus_strerror(result));
    outAudioBlock.setSampleCount(0);
  }
  return result > 0;
}

AudioDecompressionHandler::~AudioDecompressionHandler() {
  if (decoder != nullptr) {
    opus_multistream_decoder_destroy(decoder);
  }
}

bool AudioCompressionHandler::create(const AudioContentBlockSpec& spec) {
  if (encoder != nullptr) {
    opus_multistream_encoder_destroy(encoder);
    encoder = nullptr;
  }
  if (!XR_VERIFY(supportedSampleRate(spec.getSampleRate()))) {
    return false;
  }
  int error = 0;

  uint32_t totalAudioChannel = spec.getChannelCount();
  if (totalAudioChannel > 255 || totalAudioChannel == 0) {
    XR_LOGW("Invalid channel count of {}", totalAudioChannel);
    return false;
  }

  uint32_t totalCoupledAudioChannel = 2 * spec.getStereoPairCount();
  if (totalAudioChannel < totalCoupledAudioChannel) {
    XR_LOGW(
        "Invalid channel count of {} and stereo channel count of {}",
        totalAudioChannel,
        totalCoupledAudioChannel);
    return false;
  }

  uint32_t totalMonoChannel = totalAudioChannel - totalCoupledAudioChannel;
  uint32_t totalAudioStreamCount = totalMonoChannel + spec.getStereoPairCount();

  vector<uint8_t> mapping(spec.getChannelCount());

  for (uint32_t i = 0; i < totalCoupledAudioChannel + totalMonoChannel; ++i) {
    mapping[i] = i;
  }

  encoder = opus_multistream_encoder_create(
      spec.getSampleRate(),
      spec.getChannelCount(),
      totalAudioStreamCount,
      spec.getStereoPairCount(),
      mapping.data(),
      OPUS_APPLICATION_AUDIO,
      &error);

  if (error != OPUS_OK || encoder == nullptr) {
    XR_LOGW("Couldn't create Opus encoder. Error {}: {}", error, opus_strerror(error));
    return false;
  }
  encoderSpec = spec;
  XR_VERIFY(opus_multistream_encoder_ctl(encoder, OPUS_SET_BITRATE(96000)) == OPUS_OK);
  XR_VERIFY(opus_multistream_encoder_ctl(encoder, OPUS_SET_SIGNAL(OPUS_SIGNAL_MUSIC)) == OPUS_OK);
  XR_VERIFY(opus_multistream_encoder_ctl(encoder, OPUS_SET_VBR(1)) == OPUS_OK);
  return true;
}

int AudioCompressionHandler::compress(
    const void* samples,
    uint32_t sampleCount,
    void* outOpusBytes,
    size_t maxBytes) {
  return opus_multistream_encode(
      encoder, (opus_int16*)samples, sampleCount, (unsigned char*)outOpusBytes, maxBytes);
}

AudioCompressionHandler::~AudioCompressionHandler() {
  if (encoder != nullptr) {
    opus_multistream_encoder_destroy(encoder);
  }
}

#else

bool AudioBlock::opusDecompress(AudioDecompressionHandler& handler, AudioBlock& outAudioBlock) {
  return false;
}

AudioDecompressionHandler::~AudioDecompressionHandler() {}

bool AudioCompressionHandler::create(const AudioContentBlockSpec& spec) {
  return false;
}

int AudioCompressionHandler::compress(
    const void* samples,
    uint32_t sampleCount,
    void* outOpusBytes,
    size_t maxBytes) {
  return -1;
}

AudioCompressionHandler::~AudioCompressionHandler() {}

#endif

} // namespace vrs::utils
