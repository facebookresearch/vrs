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

#include "AudioTrackExtractor.h"

#include <iostream>
#include <limits>

#define DEFAULT_LOG_CHANNEL "AudioTrackExtractor"
#include <logging/Log.h>
#include <logging/Verify.h>

#include <vrs/ErrorCode.h>
#include <vrs/RecordReaders.h>
#include <vrs/helpers/FileMacros.h>
#include <vrs/helpers/Rapidjson.hpp>
#include <vrs/helpers/Strings.h>
#include <vrs/os/Utils.h>
#include <vrs/utils/AudioExtractor.h>

using namespace std;
using namespace vrs;
using namespace vrs::helpers;

namespace vrs::utils {

AudioTrackExtractor::AudioTrackExtractor(const string& wavFilePath, bool& outStop)
    : wavFilePath_{wavFilePath}, stop_{outStop}, fileAudioSpec_{AudioFormat::UNDEFINED} {}

AudioTrackExtractor::~AudioTrackExtractor() {
  AudioExtractor::closeWavFile(wavFile_);
}

bool AudioTrackExtractor::stop(const string& reason) {
  status_ = reason;
  stop_ = true;
  return false;
}

uint32_t AudioTrackExtractor::frameDifferenceWeight(int32_t frameDifference) {
  // Amplify large frame differences by using ^4
  return frameDifference * frameDifference * frameDifference * frameDifference;
}

bool AudioTrackExtractor::onAudioRead(
    const CurrentRecord& record,
    size_t,
    const ContentBlock& audioBlock) {
  const AudioContentBlockSpec& audioBlockSpec = audioBlock.audio();
  if (audioBlockSpec.getAudioFormat() != AudioFormat::PCM) {
    return stop("Found non-PCM audio block");
  }

  double audioRecordDuration = 0;
  if (audioBlockSpec.getSampleCount() > 0) {
    audioRecordDuration = audioBlockSpec.getSampleCount() * 1. / audioBlockSpec.getSampleRate();
  }

  if (!wavFile_.isOpened() || !fileAudioSpec_.isCompatibleWith(audioBlockSpec)) {
    AudioContentBlockSpec rawSpec(
        audioBlockSpec.getSampleFormat(),
        audioBlockSpec.getChannelCount(),
        audioBlockSpec.getSampleRate());
    if (wavFile_.isOpened()) {
      return stop(fmt::format(
          "Audio format changed from {} to {} at time {}",
          fileAudioSpec_.asString(),
          rawSpec.asString(),
          humanReadableTimestamp(record.timestamp)));
    }
    int status = AudioExtractor::createWavFile(wavFilePath_, audioBlockSpec, wavFile_);
    if (status != 0) {
      return stop(fmt::format("Can't create wav file: {}", errorCodeToMessage(status)));
    }
    fileAudioSpec_ = rawSpec;
    audioStartTimestamp_ = record.timestamp;
    audioSampleCount_ = 0;
    firstAudioRecordTimestamp_ = record.timestamp;
    firstAudioBlockSpec_ = audioBlockSpec.asString();
    firstAudioRecordDuration_ = audioRecordDuration;
    minMidAudioRecordDuration_ = numeric_limits<double>::max();
    maxMidAudioRecordDuration_ = 0;
    minAudioRecordGap_ = numeric_limits<double>::max();
    firstSampleTimestampTotal_ = 0;
    pastLastSampleTimestampTotal_ = 0;
  } else {
    minMidAudioRecordDuration_ = min<double>(minMidAudioRecordDuration_, lastAudioRecordDuration_);
    maxMidAudioRecordDuration_ = max<double>(maxMidAudioRecordDuration_, lastAudioRecordDuration_);
    double timeGap = record.timestamp - lastAudioRecordTimestamp_;
    minAudioRecordGap_ = min<double>(minAudioRecordGap_, timeGap);
    maxAudioRecordGap_ = max<double>(maxAudioRecordGap_, timeGap);
    int32_t gapInSamples = static_cast<int32_t>(timeGap * fileAudioSpec_.getSampleRate());
    firstSampleTimestampTotal_ +=
        frameDifferenceWeight(gapInSamples - static_cast<int32_t>(audioBlockSpec.getSampleCount()));
    pastLastSampleTimestampTotal_ += frameDifferenceWeight(gapInSamples - lastRecordSampleCount_);

    // crude method to estimate how audio records are probably missing
    double gapRatio = timeGap / max<double>(lastAudioRecordDuration_, audioRecordDuration);
    if (gapRatio > 1.8) {
      audioRecordMissCount_ += static_cast<uint32_t>(gapRatio - 0.5);
    }
  }
  lastAudioRecordTimestamp_ = record.timestamp;
  lastAudioRecordDuration_ = audioRecordDuration;
  lastRecordSampleCount_ = static_cast<int32_t>(audioBlockSpec.getSampleCount());
  audioSampleCount_ += audioBlock.audio().getSampleCount();

  audio_.resize(audioBlock.getBlockSize());
  int status = record.reader->read(audio_);
  if (status != 0) {
    return stop(fmt::format(
        "Can't read record at {}: {}",
        humanReadableTimestamp(record.timestamp),
        errorCodeToMessage(status)));
  }

  status = AudioExtractor::writeWavAudioData(wavFile_, audioBlockSpec, audio_);
  if (status != 0) {
    return stop(fmt::format(
        "Can't write to wav file at {}: {}",
        humanReadableTimestamp(record.timestamp),
        errorCodeToMessage(status)));
  }

  return !stop_;
}

bool AudioTrackExtractor::onUnsupportedBlock(
    const CurrentRecord& record,
    size_t,
    const ContentBlock& cb) {
  // the audio was not decoded ... not sure why?
  if (cb.getContentType() == ContentType::AUDIO) {
    stop(fmt::format("Unable to handle audio block {}", cb.audio().asString()));
  }
  return false;
}

string AudioTrackExtractor::getSummary(
    const string& vrsFilePath,
    StreamId streamId,
    const string& streamFlavor,
    double firstImageTime,
    double lastImageTime) {
  JDocument doc;
  JsonWrapper json{doc};
  AudioExtractor::closeWavFile(wavFile_);
  json.addMember("input", vrsFilePath);
  json.addMember("output", wavFilePath_);
  json.addMember("stream_id", streamId.getNumericName());
  if (!streamFlavor.empty()) {
    json.addMember("stream_flavor", streamFlavor);
  }
  if (firstImageTime >= 0) {
    json.addMember("first_image_timestamp", firstImageTime);
  }
  if (lastImageTime >= 0) {
    json.addMember("last_image_timestamp", lastImageTime);
  }
  json.addMember("status", status_.empty() ? "success" : status_);
  if (status_.empty()) {
    if (firstAudioRecordTimestamp_ <= lastAudioRecordTimestamp_) {
      json.addMember("first_audio_record_timestamp", firstAudioRecordTimestamp_);
      json.addMember("last_audio_record_timestamp", lastAudioRecordTimestamp_);
    }
    if (firstAudioRecordDuration_ <= lastAudioRecordDuration_) {
      json.addMember("first_audio_record_duration", firstAudioRecordDuration_);
      json.addMember("last_audio_record_duration", lastAudioRecordDuration_);
    }
    if (minMidAudioRecordDuration_ <= maxMidAudioRecordDuration_) {
      json.addMember("min_mid_audio_record_duration", minMidAudioRecordDuration_);
      json.addMember("max_mid_audio_record_duration", maxMidAudioRecordDuration_);
    }
    if (minAudioRecordGap_ <= maxAudioRecordGap_) {
      json.addMember("min_audio_record_gap", minAudioRecordGap_);
      json.addMember("max_audio_record_gap", maxAudioRecordGap_);
    }
    double totalDuration = 0;
    if (audioSampleCount_ > 0 && fileAudioSpec_.getSampleRate() > 0) {
      totalDuration = audioSampleCount_ * 1. / fileAudioSpec_.getSampleRate();
    }
    json.addMember("total_audio_duration", totalDuration);
    json.addMember("audio_record_miss_count", audioRecordMissCount_);
    double firstSampleRatio =
        static_cast<double>(firstSampleTimestampTotal_) / pastLastSampleTimestampTotal_;
    json.addMember("first_sample_timestamp_ratio", firstSampleRatio);
  }
  if (!firstAudioBlockSpec_.empty()) {
    json.addMember("audio_channel_count", fileAudioSpec_.getChannelCount());
    json.addMember("audio_sample_rate", fileAudioSpec_.getSampleRate());
    json.addMember("audio_sample_format", fileAudioSpec_.getSampleFormatAsString());
    json.addMember("first_audio_block_spec", firstAudioBlockSpec_);
  }
  return jDocumentToJsonStringPretty(doc);
}

namespace {
string writeJson(const string& jsonFilePath, const string& diagnostic, bool success) {
  DiskFile jsonFile;
  int status;
  if (((status = jsonFile.create(jsonFilePath)) != 0) ||
      ((status = jsonFile.write(diagnostic.c_str(), diagnostic.size())) != 0) ||
      ((status = jsonFile.close()) != 0)) {
    XR_LOGE("Can't write json diagnostic at '{}': {}", jsonFilePath, errorCodeToMessage(status));
  }
  return diagnostic;
}

string failure(JDocument& doc, const string& jsonFilePath) {
  return writeJson(jsonFilePath, jDocumentToJsonStringPretty(doc), false);
}
} // namespace

string extractAudioTrack(FilteredFileReader& filteredReader, const std::string& filePath) {
  const string wavFilePath = filePath + (helpers::endsWith(filePath, ".wav") ? "" : ".wav");
  const string jsonFilePath = wavFilePath + ".json";
  JDocument doc;
  JsonWrapper json{doc};
  string folderPath = os::getParentFolder(wavFilePath);
  if (folderPath.length() > 0) {
    if (!os::pathExists(folderPath)) {
      int status = os::makeDirectories(folderPath);
      if (status != 0) {
        json.addMember("status", "No audio track found.");
        return failure(doc, jsonFilePath);
      }
    }
    if (!os::isDir(folderPath)) {
      json.addMember(
          "status",
          fmt::format("Can't write output files at {}, because something is there...", folderPath));
      return failure(doc, jsonFilePath);
    }
  }
  bool stop = false;
  unique_ptr<AudioTrackExtractor> audioExtractor;
  StreamId streamId;
  for (auto id : filteredReader.filter.streams) {
    if (filteredReader.reader.mightContainAudio(id)) {
      if (audioExtractor) {
        json.addMember("status", "Multiple audio track found.");
        return failure(doc, jsonFilePath);
      }
      streamId = id;
      audioExtractor = make_unique<AudioTrackExtractor>(wavFilePath, stop);
      filteredReader.reader.setStreamPlayer(id, audioExtractor.get());
    }
  }
  if (!audioExtractor) {
    json.addMember("status", "No audio track found.");
    return failure(doc, jsonFilePath);
  }
  filteredReader.iterateSafe();
  double firstImageTime = -1;
  double lastImageTime = -1;
  for (auto id : filteredReader.filter.streams) {
    if (filteredReader.reader.mightContainImages(id)) {
      auto record = filteredReader.reader.getRecord(id, Record::Type::DATA, 0);
      if (record != nullptr && (firstImageTime < 0 || record->timestamp < firstImageTime)) {
        firstImageTime = record->timestamp;
      }
      record = filteredReader.reader.getLastRecord(id, Record::Type::DATA);
      if (record != nullptr && (lastImageTime < 0 || record->timestamp > lastImageTime)) {
        lastImageTime = record->timestamp;
      }
    }
  }
  return writeJson(
      jsonFilePath,
      audioExtractor->getSummary(
          filteredReader.getPathOrUri(),
          streamId,
          filteredReader.reader.getFlavor(streamId),
          firstImageTime,
          lastImageTime),
      true);
}

} // namespace vrs::utils
