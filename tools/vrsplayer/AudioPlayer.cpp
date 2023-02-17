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
  vector<uint8_t> buffer(cb.getBlockSize());
  if (record.reader->read(buffer) == 0) {
    if (!failedInit_) {
      const auto& audio = cb.audio();
      if (paStream_ == nullptr) {
        // the first time around, we just setup the device & the clock, but we don't play anything
        // The first data read happens when we load the file, so we don't want to hear it!
        setupAudioOutput(cb);
        fmt::print(
            "Found '{} - {}': {}, {}\n",
            record.streamId.getNumericName(),
            record.streamId.getTypeName(),
            getCurrentRecordFormatReader()->recordFormat.asString(),
            audio.asString());
      } else if (VideoTime::getPlaybackSpeed() <= 1) {
        uint32_t sampleCount = audio.getSampleCount();
        uint32_t srcChannelCount = audio.getChannelCount();
        uint8_t bytesPerSample = audio.getBytesPerSample();
        if (channelCount_ == srcChannelCount) {
          playbackQueue_.sendJob(AudioJob(std::move(buffer), sampleCount, bytesPerSample));
        } else {
          const uint8_t* src = reinterpret_cast<const uint8_t*>(buffer.data());
          uint8_t* dst = reinterpret_cast<uint8_t*>(buffer.data());
          for (uint32_t sample = 0; sample < sampleCount; ++sample) {
            memcpy(dst, src, channelCount_ * bytesPerSample);
            src += srcChannelCount * bytesPerSample;
            dst += channelCount_ * bytesPerSample;
          }
          playbackQueue_.sendJob(AudioJob(std::move(buffer), sampleCount, bytesPerSample));
        }
      }
    }
    return true;
  }
  return false;
}

void AudioPlayer::setupAudioOutput(const ContentBlock& contentBlock) {
  const auto& audio = contentBlock.audio();

  PaDeviceIndex defaultOutpuDeviceIndex = Pa_GetDefaultOutputDevice();
  const PaDeviceInfo* info = Pa_GetDeviceInfo(defaultOutpuDeviceIndex);
  channelCount_ = min<int>(audio.getChannelCount(), info->maxOutputChannels);

  PaStreamParameters output;
  output.device = defaultOutpuDeviceIndex;
  output.channelCount = channelCount_;
  output.sampleFormat = paCustomFormat;
  switch (audio.getSampleFormat()) {
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
          contentBlock.asString());
      return;
  }
  output.hostApiSpecificStreamInfo = nullptr;
  output.suggestedLatency = (info != nullptr) ? info->defaultLowOutputLatency : 0;

  PaError status =
      Pa_OpenStream(&paStream_, nullptr, &output, audio.getSampleRate(), 0, 0, nullptr, nullptr);
  if (status == paNoError && XR_VERIFY(paStream_ != nullptr)) {
    status = Pa_StartStream(paStream_);
    if (XR_VERIFY(status == paNoError)) {
      XR_LOGI("Audio output '{}' configured for {}!", info->name, contentBlock.asString());
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
    XR_LOGE("Failed to initialize audio device for {}", contentBlock.asString());
  }
}

void AudioPlayer::mediaStateChanged(FileReaderState state) {
  if (state != FileReaderState::Playing) {
    playbackQueue_.cancelAllQueuedJobs();
  }
}

void AudioPlayer::playbackThread() {
  AudioJob job;
  while (playbackQueue_.waitForJob(job)) {
    uint32_t playedFrames = 0;
    while (playedFrames < job.frameCount) {
      uint32_t frameBatchSize = min<uint32_t>(job.frameCount - playedFrames, 512);
      if (job.frameCount - playedFrames - frameBatchSize < 64) {
        frameBatchSize = job.frameCount - playedFrames;
      }
      Pa_WriteStream(
          paStream_,
          job.samples.data() + playedFrames * job.frameSize * channelCount_,
          frameBatchSize);
      playedFrames += frameBatchSize;
    }
  }
}

} // namespace vrsp
