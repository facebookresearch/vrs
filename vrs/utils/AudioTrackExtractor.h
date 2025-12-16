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
#include <vrs/utils/FilteredFileReader.h>

namespace vrs::utils {

class AudioTrackExtractor : public RecordFormatStreamPlayer {
 public:
  AudioTrackExtractor(string wavFilePath, bool& outStop);
  ~AudioTrackExtractor() override;
  AudioTrackExtractor(const AudioTrackExtractor&) = delete;
  AudioTrackExtractor& operator=(const AudioTrackExtractor&) = delete;
  AudioTrackExtractor(AudioTrackExtractor&&) = delete;
  AudioTrackExtractor& operator=(AudioTrackExtractor&&) = delete;

  bool onAudioRead(const CurrentRecord& record, size_t, const ContentBlock& audioBlock) override;
  bool onUnsupportedBlock(const CurrentRecord& record, size_t, const ContentBlock& cb) override;

  string getSummary(
      const string& vrsFilePath,
      StreamId streamId,
      const string& streamFlavor,
      double firstImageTime,
      double lastImageTime);

 protected:
  bool stop(const string& reason);
  static uint32_t frameDifferenceWeight(int32_t frameDifference);

  // path to the output wav file
  const string wavFilePath_;
  // flag set to true when an error occured, and file decoding should probably stop
  bool& stop_;
  // used to track compatibility of successive audio blocks within a stream;
  // if format changes, we close the wav file and start a new one
  AudioContentBlockSpec fileAudioSpec_;
  // output stream of wav file currently being written
  DiskFile wavFile_;
  // temp audio buffer to hold segment of audio to be written to file
  vector<uint8_t> audio_;

  // Error status describing what happened, or the empty string if nothing lethal is reported
  string status_;
  // For validation: start timestamp of the audio segment
  double audioStartTimestamp_ = 0;
  // For validation: count of audio samples previously processed since the start of the segment
  uint64_t audioSampleCount_ = 0;

  double firstAudioRecordTimestamp_ = -1;
  double lastAudioRecordTimestamp_ = -1;
  double firstAudioRecordDuration_ = -1;
  double lastAudioRecordDuration_ = -1;
  double minMidAudioRecordDuration_ = -1;
  double maxMidAudioRecordDuration_ = -1;
  double minAudioRecordGap_ = -1;
  double maxAudioRecordGap_ = -1;

  int32_t lastRecordSampleCount_ = 0;

  // To guess if an audio record's timestamp is close to the timestamp of the first audio sample,
  // we accumulate differences between expectations and reality, so "less is better".
  // Sum of weights: audio block duration against gap to next audio record's timestamp
  uint64_t firstSampleTimestampTotal_ = 0;
  // Sum of weights: audio block duration against gap to previous audio record's timestamp
  uint64_t pastLastSampleTimestampTotal_ = 0;

  uint32_t audioRecordMissCount_ = 0;
  string firstAudioBlockSpec_;
};

string extractAudioTrack(FilteredFileReader& filteredReader, const string& wavFilePath);

} // namespace vrs::utils
