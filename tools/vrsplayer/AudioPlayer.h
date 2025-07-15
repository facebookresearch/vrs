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

#include <QtCore/qglobal.h>
#include <qobject.h>

#include <vrs/RecordFormatStreamPlayer.h>
#include <vrs/helpers/JobQueue.h>
#include <vrs/utils/AudioBlock.h>

enum class FileReaderState;

typedef void PaStream; // NOLINT(modernize-use-using)

namespace vrsp {

using ::vrs::ContentBlock;
using ::vrs::CurrentRecord;
using ::vrs::DataLayout;
using ::vrs::RecordFormatStreamPlayer;
using ::vrs::StreamId;
using ::vrs::StreamPlayer;
using ::vrs::utils::AudioBlock;

class AudioPlayer : public QObject, public RecordFormatStreamPlayer {
  Q_OBJECT

 public:
  explicit AudioPlayer(QObject* parent = nullptr);
  ~AudioPlayer() override;

  bool onDataLayoutRead(const CurrentRecord&, size_t blockIndex, DataLayout&) override;
  bool onAudioRead(const CurrentRecord&, size_t blockIndex, const ContentBlock&) override;

  int getAudioChannelCount() const {
    return audioChannelCount_;
  }

 signals:
  void audioOutputInitialized(uint32_t audioChannelCount, uint32_t playbackChannelCount);

 public slots:
  void mediaStateChanged(FileReaderState state);
  void selectedAudioChannelsChanged(uint32_t leftAudioChannel, uint32_t rightAudioChannel);

 private:
  bool setupAudioOutput(const vrs::AudioContentBlockSpec& audioSpec);
  void playbackThread();

  PaStream* paStream_{};
  uint32_t audioChannelCount_{};
  uint32_t paChannelCount_{};
  uint32_t leftAudioChannel_{};
  uint32_t rightAudioChannel_{};
  vrs::AudioSampleFormat sampleFormat_{vrs::AudioSampleFormat::UNDEFINED};
  bool failedInit_{};
  vrs::JobQueueWithThread<AudioBlock> playbackQueue_;
  vrs::utils::AudioDecompressionHandler opusHandler_;
};

} // namespace vrsp
