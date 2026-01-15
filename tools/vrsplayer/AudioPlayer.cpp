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
    if (!failedInit_) {
      const auto& audio = cb.audio();
      if (paStream_ == nullptr) {
        // the first time around, we just setup the device & the clock, but we don't play anything
        // The first data read happens when we load the file, so we don't want to hear it!
        if (setupAudioOutput(audio)) {
          fmt::print(
              "Found '{} - {}': {}, {}\n",
              record.streamId.getNumericName(),
              record.streamId.getTypeName(),
              getCurrentRecordFormatReader()->recordFormat.asString(),
              audio.asString());
        }
      } else if (
          VideoTime::getPlaybackSpeed() <= 1 && sampleFormat_ == audio.getSampleFormat() &&
          audio.getChannelCount() >= paChannelCount_ && audioBlock.getSampleCount() > 0) {
        playbackQueue_.sendJob(std::move(audioBlock));
      }
    }
  }
  return true;
}

bool AudioPlayer::setupAudioOutput(const AudioContentBlockSpec& audioSpec) {
  PaDeviceIndex defaultOutputDeviceIndex = Pa_GetDefaultOutputDevice();
  const PaDeviceInfo* info = Pa_GetDeviceInfo(defaultOutputDeviceIndex);
  sampleFormat_ = audioSpec.getSampleFormat();
  audioChannelCount_ = audioSpec.getChannelCount();
  paChannelCount_ = min<uint32_t>(audioSpec.getChannelCount(), info->maxOutputChannels);

  PaStreamParameters output;
  output.device = defaultOutputDeviceIndex;
  output.channelCount = paChannelCount_;
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
      return false;
  }
  output.hostApiSpecificStreamInfo = nullptr;
  output.suggestedLatency = (info != nullptr) ? info->defaultLowOutputLatency : 0;

  PaError status = Pa_OpenStream(
      &paStream_, nullptr, &output, audioSpec.getSampleRate(), 0, 0, nullptr, nullptr);
  if (status == paNoError && XR_VERIFY(paStream_ != nullptr)) {
    status = Pa_StartStream(paStream_);
    if (XR_VERIFY(status == paNoError)) {
      string configuration = (paChannelCount_ == 1) ? "Mono"
          : (paChannelCount_ == 2)                  ? "Stereo"
                                                    : to_string(paChannelCount_) + " channels";
      XR_LOGI("{} audio output '{}' initialized.", configuration, info->name);
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
    audioChannelCount_ = 0;
    paChannelCount_ = 0;
    return false;
  }
  emit audioOutputInitialized(audioChannelCount_, paChannelCount_);
  return true;
}

void AudioPlayer::mediaStateChanged(FileReaderState state) {
  if (state != FileReaderState::Playing) {
    playbackQueue_.cancelAllQueuedJobs();
  }
}

void AudioPlayer::selectedAudioChannelsChanged(
    uint32_t leftAudioChannel,
    uint32_t rightAudioChannel) {
  leftAudioChannel_ = leftAudioChannel;
  rightAudioChannel_ = rightAudioChannel;
}

void AudioPlayer::playbackThread() {
  AudioBlock block;
  while (playbackQueue_.waitForJob(block)) {
    if (block.getAudioFormat() == AudioFormat::OPUS) {
      if (!block.decompressAudio(opusHandler_)) {
        continue;
      }
    }
    if (block.getAudioFormat() != AudioFormat::PCM) {
      continue;
    }
    uint32_t frameCount = block.getSampleCount();
    uint8_t frameStride = block.getSpec().getSampleFrameStride();
    uint8_t bytesPerSample = block.getSpec().getBytesPerSample();
    uint8_t paFrameStride = paChannelCount_ * bytesPerSample;

    const uint32_t firstAudioChannel = leftAudioChannel_;
    const uint32_t secondAudioChannel = rightAudioChannel_;

    bool sequential = paChannelCount_ < 2 || secondAudioChannel == firstAudioChannel + 1;
    const uint32_t srcOffset1 = firstAudioChannel * bytesPerSample;
    const uint32_t srcOffset2 = secondAudioChannel * bytesPerSample;
    vector<uint8_t> safeBuffer;

    uint32_t framesPlayed = 0;
    while (framesPlayed < frameCount) {
      uint32_t frameBatchSize = min<uint32_t>(frameCount - framesPlayed, 512);
      if (frameCount - framesPlayed - frameBatchSize < 64) {
        frameBatchSize = frameCount - framesPlayed; // avoid tiny batches
      }
      const uint8_t* src = block.rdata() + framesPlayed * frameStride;
      if (!sequential) {
        safeBuffer.resize(paFrameStride * frameBatchSize);
        uint8_t* dst = safeBuffer.data();
        uint32_t sample = 0;
        while (sample < frameBatchSize) {
          memcpy(dst, src + srcOffset1, bytesPerSample);
          memcpy(dst + bytesPerSample, src + srcOffset2, bytesPerSample);
          src += frameStride;
          dst += paFrameStride;
          sample++;
        }
        Pa_WriteStream(paStream_, safeBuffer.data(), frameBatchSize);
      } else {
        if (paFrameStride == frameStride) {
          Pa_WriteStream(paStream_, src, frameBatchSize);
        } else {
          // either we play fewer channels than provided, or frames are padded, we need to compact
          uint8_t* dst = block.data<uint8_t>();
          uint32_t sample = 0;
          while (dst + paFrameStride >= src + frameStride && sample < frameBatchSize) {
            memmove(dst, src + srcOffset1, paFrameStride); // more expensive, but safe
            src += frameStride;
            dst += paFrameStride;
            sample++;
          }
          while (sample < frameBatchSize) {
            memcpy(dst, src + srcOffset1, paFrameStride);
            src += frameStride;
            dst += paFrameStride;
            sample++;
          }
          Pa_WriteStream(paStream_, block.rdata(), frameBatchSize);
        }
      }
      framesPlayed += frameBatchSize;
    }
  }
}

} // namespace vrsp
