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

#include "AudioPlayer.h"

#include <vector>

#include <portaudio.h>

#define DEFAULT_LOG_CHANNEL "AudioPlayer"
#include <logging/Log.h>
#include <logging/Verify.h>

#include <vrs/RecordReaders.h>
#include <vrs/StreamPlayer.h>

#include "FileReader.h"
#include "VideoTime.h"

namespace vrsp {

using namespace std;
using namespace vrs;

AudioPlayer::AudioPlayer(QObject* parent) : QObject(parent) {
  failedInit_ = !XR_VERIFY(Pa_Initialize() == paNoError);
}

AudioPlayer::~AudioPlayer() {
  playbackQueue_.endThread();
  if (paStream_ != nullptr) {
    VideoTime::setTimeAudioStreamSource(nullptr);
    Pa_StopStream(paStream_);
    Pa_CloseStream(paStream_);
  }
  Pa_Terminate();
}

bool vrsp::AudioPlayer::onDataLayoutRead(const CurrentRecord&, size_t, DataLayout&) {
  return true;
}

bool AudioPlayer::onAudioRead(const CurrentRecord& record, size_t blkIdx, const ContentBlock& cb) {
  // XR_LOGI("Audio block: {:.3f} {}", record.timestamp, contentBlock.asString());
  AudioBlock audioBlock;
  if (audioBlock.readBlock(record.reader, cb)) {
    if (cb.audio().getAudioFormat() != AudioFormat::PCM) {
      return true; // we read the audio, but we don't actually support it in vrsplayer (yet!)
    }
    if (!failedInit_) {
      const auto& audio = cb.audio();
      if (paStream_ == nullptr) {
        // the first time around, we just setup the device & the clock, but we don't play anything
        // The first data read happens when we load the file, so we don't want to hear it!
        setupAudioOutput(audio);
        fmt::print(
            "Found '{} - {}': {}, {}\n",
            record.streamId.getNumericName(),
            record.streamId.getTypeName(),
            getCurrentRecordFormatReader()->recordFormat.asString(),
            audio.asString());
      } else if (
          VideoTime::getPlaybackSpeed() <= 1 && sampleFormat_ == audio.getSampleFormat() &&
          audio.getSampleCount() >= channelCount_) {
        playbackQueue_.sendJob(std::move(audioBlock));
      }
    }
  }
  return true;
}

void AudioPlayer::setupAudioOutput(const AudioContentBlockSpec& audioSpec) {
  PaDeviceIndex defaultOutpuDeviceIndex = Pa_GetDefaultOutputDevice();
  const PaDeviceInfo* info = Pa_GetDeviceInfo(defaultOutpuDeviceIndex);
  sampleFormat_ = audioSpec.getSampleFormat();
  channelCount_ = min<int>(audioSpec.getChannelCount(), info->maxOutputChannels);

  PaStreamParameters output;
  output.device = defaultOutpuDeviceIndex;
  output.channelCount = channelCount_;
  output.sampleFormat = paCustomFormat;
  switch (sampleFormat_) {
    case AudioSampleFormat::S8:
      output.sampleFormat = paInt8;
      break;

    case AudioSampleFormat::U8:
      output.sampleFormat = paUInt8;
      break;

    case AudioSampleFormat::S16_LE:
      output.sampleFormat = paInt16;
      break;

    case AudioSampleFormat::S24_LE:
      output.sampleFormat = paInt24;
      break;

    case AudioSampleFormat::S32_LE:
      output.sampleFormat = paInt32;
      break;

    case AudioSampleFormat::F32_LE:
      output.sampleFormat = paFloat32;
      break;

    // Might need to implement some conversions, eventually...
    case AudioSampleFormat::A_LAW:
    case AudioSampleFormat::MU_LAW:
    case AudioSampleFormat::U16_LE:
    case AudioSampleFormat::U24_LE:
    case AudioSampleFormat::U32_LE:
    case AudioSampleFormat::F64_LE:
    case AudioSampleFormat::S16_BE:
    case AudioSampleFormat::S24_BE:
    case AudioSampleFormat::S32_BE:
    case AudioSampleFormat::U16_BE:
    case AudioSampleFormat::U24_BE:
    case AudioSampleFormat::U32_BE:
    case AudioSampleFormat::F32_BE:
    case AudioSampleFormat::F64_BE:
    case AudioSampleFormat::COUNT:
    case AudioSampleFormat::UNDEFINED:
      failedInit_ = true;
      XR_LOGE(
          "Audio sample format {} not supported. Audio sample conversion required.",
          audioSpec.asString());
      return;
  }
  output.hostApiSpecificStreamInfo = nullptr;
  output.suggestedLatency = (info != nullptr) ? info->defaultLowOutputLatency : 0;

  PaError status = Pa_OpenStream(
      &paStream_, nullptr, &output, audioSpec.getSampleRate(), 0, 0, nullptr, nullptr);
  if (status == paNoError && XR_VERIFY(paStream_ != nullptr)) {
    status = Pa_StartStream(paStream_);
    if (XR_VERIFY(status == paNoError)) {
      XR_LOGI("Audio output '{}' configured for {}!", info->name, audioSpec.asString());
      VideoTime::setTimeAudioStreamSource(paStream_);
      playbackQueue_.startThread(&AudioPlayer::playbackThread, this);
    } else {
      failedInit_ = true;
    }
  } else {
    failedInit_ = true;
  }
  if (failedInit_ && paStream_ != nullptr) {
    Pa_StopStream(paStream_);
    Pa_CloseStream(paStream_);
    paStream_ = nullptr;
  }
  if (failedInit_) {
    XR_LOGE("Failed to initialize audio device for {}", audioSpec.asString());
  }
}

void AudioPlayer::mediaStateChanged(FileReaderState state) {
  if (state != FileReaderState::Playing) {
    playbackQueue_.cancelAllQueuedJobs();
  }
}

void AudioPlayer::playbackThread() {
  AudioBlock block;
  while (playbackQueue_.waitForJob(block)) {
    uint32_t frameCount = block.getSampleCount();
    uint8_t frameStride = block.getSpec().getSampleFrameStride();
    uint8_t paFrameStride = channelCount_ * block.getSpec().getBytesPerSample();

    uint32_t framesPlayed = 0;
    while (framesPlayed < frameCount) {
      uint32_t frameBatchSize = min<uint32_t>(frameCount - framesPlayed, 512);
      if (frameCount - framesPlayed - frameBatchSize < 64) {
        frameBatchSize = frameCount - framesPlayed; // avoid tiny batches
      }
      const uint8_t* src = block.rdata() + framesPlayed * frameStride;
      if (paFrameStride == frameStride) {
        Pa_WriteStream(paStream_, src, frameBatchSize);
      } else {
        // either we play fewer channels than provided, or frames are padded, we need to compact
        uint8_t* dst = const_cast<uint8_t*>(reinterpret_cast<const uint8_t*>(block.rdata()));
        uint32_t sample = 0;
        while (dst + paFrameStride > src && sample < frameBatchSize) {
          memmove(dst, src, paFrameStride); // more expensive, but safe in case of overlap
          src += frameStride;
          dst += paFrameStride;
          sample++;
        }
        while (sample < frameBatchSize) {
          memcpy(dst, src, paFrameStride);
          src += frameStride;
          dst += paFrameStride;
          sample++;
        }
        Pa_WriteStream(paStream_, block.rdata(), frameBatchSize);
      }
      framesPlayed += frameBatchSize;
    }
  }
}

} // namespace vrsp
