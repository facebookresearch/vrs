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

#include <cstdio>

#include <vector>

#include <gtest/gtest.h>

#include <TestDataDir/TestDataDir.h>
#include <test_helpers/GTestMacros.h>

#define DEFAULT_LOG_CHANNEL "AudioContentBlockReaderTest"
#include <logging/Log.h>
#include <logging/Verify.h>

#include <vrs/DataLayoutConventions.h>
#include <vrs/DiskFile.h>
#include <vrs/RecordFileReader.h>
#include <vrs/RecordFileWriter.h>
#include <vrs/RecordFormatStreamPlayer.h>
#include <vrs/helpers/FileMacros.h>
#include <vrs/os/Utils.h>
#include <vrs/utils/AudioBlock.h>

using namespace std;
using namespace vrs;
using namespace vrs::datalayout_conventions;

namespace {
const string kVrsFilesDir = coretech::getTestDataDir() + "/VRS_Files/";
const string kWavFile = kVrsFilesDir + "audio_int16_48k.wav";
const uint32_t kWavHeaderSize = 44;

const uint32_t kSampleRate = 48000;
const uint8_t kChannels = 2;
uint32_t kSampleCount = 0;

const vector<uint16_t>& getAudioSamples() {
  static vector<uint16_t> samples;
  if (!samples.empty()) {
    return samples;
  }
  DiskFile file;
  size_t fileSize = os::getFileSize(kWavFile);
  if (XR_VERIFY(fileSize > kWavHeaderSize) && XR_VERIFY(file.open(kWavFile) == 0)) {
    samples.resize(fileSize);
    if (XR_VERIFY(file.read(samples.data(), kWavHeaderSize) == 0) &&
        XR_VERIFY(file.read(samples.data(), fileSize - kWavHeaderSize) == 0)) {
      samples.resize(fileSize - kWavHeaderSize);
    } else {
      samples.clear();
    }
  }
  kSampleCount = samples.size() / (sizeof(int16_t) * kChannels);
  return samples;
}
} // namespace

struct AudioContentBlockReaderTest : testing::Test {};

namespace {

enum class LayoutStyle {
  Classic = 1, // Spec in config record, sample count in data record
  NoSize, // Spec in config record, no sample count in data record
  FullSpecData, // Nothing in config record, full spec in data record
};

class AudioStream : public Recordable {
 public:
  AudioStream(LayoutStyle style, uint32_t fullRecordSize)
      : Recordable(RecordableTypeId::AudioStream), style_{style}, fullRecordSize_{fullRecordSize} {
    switch (style_) {
      case LayoutStyle::Classic:
        addRecordFormat(Record::Type::CONFIGURATION, 1, config_.getContentBlock(), {&config_});
        addRecordFormat(
            Record::Type::DATA, 1, data_.getContentBlock() + ContentType::AUDIO, {&data_});
        break;
      case LayoutStyle::NoSize:
        addRecordFormat(Record::Type::CONFIGURATION, 1, config_.getContentBlock(), {&config_});
        addRecordFormat(Record::Type::DATA, 1, ContentType::AUDIO, {});
        break;
      case LayoutStyle::FullSpecData:
        addRecordFormat(
            Record::Type::DATA, 1, config_.getContentBlock() + ContentType::AUDIO, {&config_});
        break;
    }
  }
  const Record* createConfigurationRecord() override {
    config_.audioFormat.set(AudioFormat::PCM);
    config_.sampleType.set(AudioSampleFormat::S16_LE);
    config_.channelCount.set(kChannels);
    config_.sampleRate.set(kSampleRate);
    switch (style_) {
      case LayoutStyle::Classic:
      case LayoutStyle::NoSize:
        return createRecord(getTimestampSec(), Record::Type::CONFIGURATION, 1, DataSource(config_));
        break;
      case LayoutStyle::FullSpecData:
        return nullptr;
        break;
    }
    return nullptr;
  }
  const Record* createStateRecord() override {
    return createRecord(getTimestampSec(), Record::Type::STATE, 1);
  }
  void createDataRecords(uint32_t sampleCount) {
    const vector<uint16_t>& samples = getAudioSamples();
    const uint16_t* first = samples.data() + sampleCount_ * kChannels;
    size_t size = sampleCount * kChannels * sizeof(int16_t);
    switch (style_) {
      case LayoutStyle::Classic:
        data_.sampleCount.set(sampleCount);
        createRecord(
            getTimestampSec(),
            Record::Type::DATA,
            1,
            DataSource(data_, DataSourceChunk(first, size)));
        break;
      case LayoutStyle::NoSize:
        createRecord(
            getTimestampSec(), Record::Type::DATA, 1, DataSource(DataSourceChunk(first, size)));
        break;
      case LayoutStyle::FullSpecData:
        config_.sampleCount.set(sampleCount);
        createRecord(
            getTimestampSec(),
            Record::Type::DATA,
            1,
            DataSource(config_, DataSourceChunk(first, size)));
        break;
    }
    sampleCount_ += sampleCount;
  }
  void createAllRecords() {
    createConfigurationRecord();
    createStateRecord();
    uint32_t count = 0;
    while (sampleCount_ < kSampleCount) {
      createDataRecords(min<uint32_t>(fullRecordSize_ + count++, kSampleCount - sampleCount_));
    }
  }
  double getTimestampSec() const {
    return sampleCount_ / double(kSampleRate);
  }

 private:
  const LayoutStyle style_;
  const uint32_t fullRecordSize_;
  AudioSpec config_;
  NextAudioContentBlockSampleCountSpec data_;
  uint64_t sampleCount_ = 0;
};

struct Analytics {
  uint32_t configDatalayoutCount = 0;
  uint32_t dataDatalayoutCount = 0;

  uint32_t audioBlockCount = 0;
  uint32_t audioSampleCount = 0;

  uint32_t unsupportedCount = 0;
};

class AnalyticsPlayer : public RecordFormatStreamPlayer {
 public:
  bool onDataLayoutRead(const CurrentRecord& record, size_t, DataLayout&) override {
    if (record.recordType == Record::Type::CONFIGURATION) {
      analytics_.configDatalayoutCount++;
    } else if (record.recordType == Record::Type::DATA) {
      analytics_.dataDatalayoutCount++;
    }
    return true;
  }
  bool onAudioRead(const CurrentRecord& record, size_t, const ContentBlock& cb) override {
    analytics_.audioBlockCount++;
    utils::AudioBlock audioBlock;
    if (audioBlock.readBlock(record.reader, cb)) {
      analytics_.audioSampleCount += audioBlock.getSampleCount();
      EXPECT_EQ(audioBlock.getSampleRate(), kSampleRate);
      EXPECT_EQ(audioBlock.getChannelCount(), kChannels);
    }
    return true;
  }
  bool onUnsupportedBlock(const CurrentRecord&, size_t, const ContentBlock&) override {
    analytics_.unsupportedCount++;
    return false;
  }

  const Analytics& getAnalytics() const {
    return analytics_;
  }

 private:
  Analytics analytics_;
};

Analytics readAudioVRSFile(const string& path) {
  RecordFileReader reader;
  AnalyticsPlayer player;
  reader.openFile(path);
  for (auto id : reader.getStreams()) {
    reader.setStreamPlayer(id, &player);
  }
  reader.readAllRecords();
  return player.getAnalytics();
}

Analytics runTest(const char* name, LayoutStyle style, uint32_t sampleCount) {
  const string testPath = os::getTempFolder() + name + ".vrs";

  RecordFileWriter fileWriter;
  AudioStream audioStream(style, sampleCount);
  fileWriter.addRecordable(&audioStream);
  audioStream.createAllRecords();
  EXPECT_EQ(fileWriter.writeToFile(testPath), 0);

  return readAudioVRSFile(testPath);
}

} // namespace

#define TEST_NAME (::testing::UnitTest::GetInstance()->current_test_info()->name())

const uint32_t kTotalSampleCount = 60743;

TEST_F(AudioContentBlockReaderTest, testClassicAudio) {
  ASSERT_GT(getAudioSamples().size(), 100000);

  Analytics analytics = runTest(TEST_NAME, LayoutStyle::Classic, 480);

  EXPECT_EQ(analytics.configDatalayoutCount, 1);
  EXPECT_EQ(analytics.dataDatalayoutCount, analytics.audioBlockCount);
  EXPECT_EQ(analytics.audioBlockCount, 114);
  EXPECT_EQ(analytics.audioSampleCount, kTotalSampleCount);
  EXPECT_EQ(analytics.unsupportedCount, 0);
}

TEST_F(AudioContentBlockReaderTest, testNoSize) {
  ASSERT_GT(getAudioSamples().size(), 100000);

  Analytics analytics = runTest(TEST_NAME, LayoutStyle::NoSize, 256);

  EXPECT_EQ(analytics.configDatalayoutCount, 1);
  EXPECT_EQ(analytics.dataDatalayoutCount, 0);
  EXPECT_EQ(analytics.audioBlockCount, 177);
  EXPECT_EQ(analytics.audioSampleCount, kTotalSampleCount);
  EXPECT_EQ(analytics.unsupportedCount, 0);
}

TEST_F(AudioContentBlockReaderTest, testFullSpecData) {
  ASSERT_GT(getAudioSamples().size(), 100000);

  Analytics analytics = runTest(TEST_NAME, LayoutStyle::FullSpecData, 256);

  EXPECT_EQ(analytics.configDatalayoutCount, 0);
  EXPECT_EQ(analytics.dataDatalayoutCount, 177);
  EXPECT_EQ(analytics.audioBlockCount, 177);
  EXPECT_EQ(analytics.audioSampleCount, kTotalSampleCount);
  EXPECT_EQ(analytics.unsupportedCount, 0);
}
