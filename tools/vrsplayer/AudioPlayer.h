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

#include <thread>

#include <QtCore/qglobal.h>
#include <qobject.h>

#include <vrs/RecordFormatStreamPlayer.h>
#include <vrs/helpers/JobQueue.h>

enum class FileReaderState;

typedef void PaStream;

namespace vrsp {

using ::vrs::ContentBlock;
using ::vrs::CurrentRecord;
using ::vrs::DataLayout;
using ::vrs::RecordFormatStreamPlayer;
using ::vrs::StreamId;
using ::vrs::StreamPlayer;

class AudioPlayer : public QObject, public RecordFormatStreamPlayer {
  Q_OBJECT

  struct AudioJob {
    AudioJob() = default;
    AudioJob(std::vector<uint8_t>&& samples, uint32_t frameCount, uint32_t frameSize)
        : samples{std::move(samples)}, frameCount{frameCount}, frameSize{frameSize} {}
    AudioJob(AudioJob&& job)
        : samples{std::move(job.samples)}, frameCount{job.frameCount}, frameSize{job.frameSize} {}

    AudioJob& operator=(AudioJob&& job) {
      samples = std::move(job.samples);
      frameCount = job.frameCount;
      frameSize = job.frameSize;
      return *this;
    }
    std::vector<uint8_t> samples;
    uint32_t frameCount;
    uint32_t frameSize;
  };

 public:
  explicit AudioPlayer(QObject* parent = nullptr);
  ~AudioPlayer() override;

  bool onDataLayoutRead(const CurrentRecord&, size_t blockIndex, DataLayout&) override;
  bool onAudioRead(const CurrentRecord&, size_t blockIndex, const ContentBlock&) override;

  void setupAudioOutput(const ContentBlock& contentBlock);

 signals:

 public slots:
  void mediaStateChanged(FileReaderState state);

 private:
  void playbackThread();

  PaStream* paStream_ = nullptr;
  bool failedInit_ = false;
  int channelCount_ = 0;
  vrs::JobQueueWithThread<AudioJob> playbackQueue_;
};

} // namespace vrsp
