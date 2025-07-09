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

#include <vrs/DiskFile.h>
#include <vrs/RecordFormatStreamPlayer.h>

#include "AudioBlock.h"

namespace vrs::utils {

/// Audio track extraction code, with few diagnostics, and auto-file name generation.
/// If the audio format changes mid-track, will create a new file.
/// Designed to extract all the audio parts possible of a file, in a fairly crude way.

class AudioExtractor : public RecordFormatStreamPlayer {
 public:
  AudioExtractor(string folderPath, StreamId id, uint32_t& counter);

  ~AudioExtractor() override;

  bool onAudioRead(const CurrentRecord& record, size_t, const ContentBlock& audioBlock) override;
  bool onUnsupportedBlock(const CurrentRecord& record, size_t, const ContentBlock& cb) override;

  static int createWavFile(
      const string& wavFilePath,
      const AudioContentBlockSpec& audioBlock,
      DiskFile& outFile);
  static int writeWavAudioData(
      DiskFile& inFile,
      const AudioContentBlockSpec& audioBlock,
      const vector<uint8_t>& audio);
  static int closeWavFile(DiskFile& inFile);

 protected:
  // folder to save wav files to
  string folderPath_;
  // device id & instance for the stream we are operating on
  StreamId id_;
  // used to sum up the total number of audio files written out across all
  // streams (caller provides a reference so this class can add to it)
  uint32_t& cumulativeOutputAudioFileCount_;
  // count of audio files written out in this specific stream
  uint32_t streamOutputAudioFileCount_ = 0;
  // used to track compatibility of successive audio blocks within a stream;
  // if format changes, we close the wav file and start a new one
  AudioContentBlockSpec currentAudioContentBlockSpec_;
  // output stream of wav file currently being written
  DiskFile currentWavFile_;
  // For validation: start timestamp of the audio segment
  double segmentStartTimestamp_ = 0;
  // For validation: count of audio samples previously processed since the start of the segment
  uint64_t segmentSamplesCount_ = 0;
  utils::AudioDecompressionHandler decompressor_;
};

} // namespace vrs::utils
