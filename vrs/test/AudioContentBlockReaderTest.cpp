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

#include <string>
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

struct AudioData {
 public:
  AudioData(uint32_t sampleRate, uint8_t channels, uint8_t stereoPairCount, const string& wavFile)
      : wavFile(wavFile),
        sampleRate(sampleRate),
        channels(channels),
        stereoPairCount(stereoPairCount) {
    // Read the WAV file, and store the samples in a vector.
    const string vrsFilesDir = coretech::getTestDataDir() + "/VRS_Files/";
    const string absFilePath = vrsFilesDir + wavFile;
    const uint32_t wavHeaderSize = 44;
    DiskFile file;
    size_t fileSize = os::getFileSize(absFilePath);
    if (XR_VERIFY(fileSize > wavHeaderSize) && XR_VERIFY(file.open(absFilePath) == 0)) {
      samples.resize(fileSize);
      if (XR_VERIFY(file.read(samples.data(), wavHeaderSize) == 0) &&
          XR_VERIFY(file.read(samples.data(), fileSize - wavHeaderSize) == 0)) {
        samples.resize(fileSize - wavHeaderSize);
      } else {
        samples.clear();
      }
    }
    sampleCount = samples.size() / (sizeof(int16_t) * channels);
  }
  const string wavFile;
  vector<uint16_t> samples;
  const uint32_t sampleRate;
  const uint8_t channels;
  const uint8_t stereoPairCount;
  uint32_t sampleCount;
};
} // namespace

struct AudioContentBlockReaderTest : testing::Test {
 protected:
  AudioContentBlockReaderTest()
      : stereoAudio(48000, 2, 1, "audio_int16_48k.wav"),
        multiAudio(48000, 5, 2, "audio_int16_48k_5ch.wav") {}
  const AudioData stereoAudio;
  const AudioData multiAudio;
};

namespace {
enum class LayoutStyle {
  Classic = 1, // Spec in config record, sample count in data record
  NoSize, // Spec in config record, no sample count in data record
  FullSpecData, // Nothing in config record, full spec in data record
  OpusStereo, // Opus compression, with sample count specification
  OpusStereoNoSampleCount, // Opus compression without sample count specification
};

class NextContentBlockAudioSampleCountSpec : public AutoDataLayout {
 public:
  DataPieceValue<uint32_t> sampleCount{kAudioSampleCount};

  AutoDataLayoutEnd end;
};

class AudioStream : public Recordable {
 public:
  AudioStream(LayoutStyle style, uint32_t fullRecordSize, const AudioData& ipAudioData)
      : Recordable(RecordableTypeId::AudioStream),
        style_{style},
        fullRecordSize_{fullRecordSize},
        ipAudioData_{ipAudioData} {
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
      case LayoutStyle::OpusStereo:
        addRecordFormat(Record::Type::CONFIGURATION, 1, config_.getContentBlock(), {&config_});
        addRecordFormat(
            Record::Type::DATA, 1, data_.getContentBlock() + ContentType::AUDIO, {&data_});
        break;
      case LayoutStyle::OpusStereoNoSampleCount:
        addRecordFormat(Record::Type::CONFIGURATION, 1, config_.getContentBlock(), {&config_});
        addRecordFormat(Record::Type::DATA, 1, ContentType::AUDIO, {});
        break;
    }
  }
  const Record* createConfigurationRecord() override {
    config_.audioFormat.set(AudioFormat::PCM);
    config_.sampleType.set(AudioSampleFormat::S16_LE);
    config_.channelCount.set(ipAudioData_.channels);
    config_.sampleRate.set(ipAudioData_.sampleRate);
    switch (style_) {
      case LayoutStyle::Classic:
      case LayoutStyle::NoSize:
        return createRecord(getTimestampSec(), Record::Type::CONFIGURATION, 1, DataSource(config_));
        break;
      case LayoutStyle::FullSpecData:
        return nullptr;
        break;
      case LayoutStyle::OpusStereo:
        config_.audioFormat.set(AudioFormat::OPUS);
        config_.stereoPairCount.set(ipAudioData_.stereoPairCount);
        return createRecord(getTimestampSec(), Record::Type::CONFIGURATION, 1, DataSource(config_));
        break;
      case LayoutStyle::OpusStereoNoSampleCount:
        config_.audioFormat.set(AudioFormat::OPUS);
        config_.stereoPairCount.set(ipAudioData_.stereoPairCount);
        return createRecord(getTimestampSec(), Record::Type::CONFIGURATION, 1, DataSource(config_));
        break;
    }
    return nullptr;
  }
  const Record* createStateRecord() override {
    return createRecord(getTimestampSec(), Record::Type::STATE, 1);
  }
  void createDataRecords(uint32_t sampleCount) {
    const vector<uint16_t>& samples = ipAudioData_.samples;
    const uint16_t* first = samples.data() + sampleCount_ * ipAudioData_.channels;
    size_t size = sampleCount * ipAudioData_.channels * sizeof(int16_t);
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
      case LayoutStyle::OpusStereo:
      case LayoutStyle::OpusStereoNoSampleCount: {
        if (compressionHandler_.encoder == nullptr) {
          compressionHandler_.create(
              {AudioFormat::OPUS,
               AudioSampleFormat::S16_LE,
               ipAudioData_.channels,
               0,
               ipAudioData_.sampleRate,
               0,
               ipAudioData_.stereoPairCount});
          opusData_.resize(4096 * ipAudioData_.channels);
        }
        // Opus isn't very flexible: it can only process specific sizes, so we might need to padd!
        vector<uint16_t> paddedSamples;
        if (sampleCount < fullRecordSize_) {
          paddedSamples.resize(fullRecordSize_ * ipAudioData_.channels, 0);
          memcpy(paddedSamples.data(), first, size);
          first = paddedSamples.data();
          size = paddedSamples.size() * sizeof(int16_t);
        }
        int result = compressionHandler_.compress(
            first, fullRecordSize_, opusData_.data(), opusData_.size());
        if (XR_VERIFY(result > 0)) {
          if (style_ == LayoutStyle::OpusStereo) {
            data_.sampleCount.set(fullRecordSize_);
            createRecord(
                getTimestampSec(),
                Record::Type::DATA,
                1,
                DataSource(data_, DataSourceChunk(opusData_.data(), result)));
          } else {
            createRecord(
                getTimestampSec(),
                Record::Type::DATA,
                1,
                DataSource(DataSourceChunk(opusData_.data(), result)));
          }
        }
      } break;
    }
    sampleCount_ += sampleCount;
  }
  void createAllRecords() {
    createConfigurationRecord();
    createStateRecord();
    if (style_ == LayoutStyle::OpusStereo || style_ == LayoutStyle::OpusStereoNoSampleCount) {
      // We must use blocks of a specific size (Opus limitation)
      while (sampleCount_ < ipAudioData_.sampleCount) {
        createDataRecords(min<uint32_t>(fullRecordSize_, ipAudioData_.sampleCount - sampleCount_));
      }
    } else {
      // Use blocks of different sizes, to exercise the system!
      uint32_t variation = 0;
      while (sampleCount_ < ipAudioData_.sampleCount) {
        createDataRecords(min<uint32_t>(
            fullRecordSize_ + (style_ == LayoutStyle::OpusStereo ? 0 : variation++),
            ipAudioData_.sampleCount - sampleCount_));
      }
    }
  }
  double getTimestampSec() const {
    return sampleCount_ / double(ipAudioData_.sampleRate);
  }

 private:
  const LayoutStyle style_;
  const uint32_t fullRecordSize_;
  AudioSpec config_;
  NextContentBlockAudioSampleCountSpec data_;
  uint64_t sampleCount_ = 0;
  utils::AudioCompressionHandler compressionHandler_;
  vector<uint8_t> opusData_;
  const AudioData& ipAudioData_;
};

struct Analytics {
  uint32_t configDatalayoutCount = 0;
  uint32_t dataDatalayoutCount = 0;
  uint32_t audioBlockCount = 0;
  uint32_t audioSampleCount = 0;
  uint8_t channels = 0;
  uint32_t sampleRate = 0;
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
    if (audioBlock.readBlock(record.reader, cb) &&
        XR_VERIFY(audioBlock.decompressAudio(decompressor_))) {
      analytics_.audioSampleCount += audioBlock.getSampleCount();
      EXPECT_EQ(audioBlock.getSampleRate(), analytics_.sampleRate);
      EXPECT_EQ(audioBlock.getChannelCount(), analytics_.channels);
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

  void setAnalyticsSampleRate(uint32_t sampleRate) {
    analytics_.sampleRate = sampleRate;
  }

  void setAnalyticsChannels(uint8_t channels) {
    analytics_.channels = channels;
  }

 private:
  Analytics analytics_;
  utils::AudioDecompressionHandler decompressor_;
};

Analytics readAudioVRSFile(const string& path, const AudioData& ipAudioData) {
  AnalyticsPlayer player;
  RecordFileReader reader;
  reader.openFile(path);
  player.setAnalyticsChannels(ipAudioData.channels);
  player.setAnalyticsSampleRate(ipAudioData.sampleRate);
  for (auto id : reader.getStreams()) {
    reader.setStreamPlayer(id, &player);
  }
  reader.readAllRecords();
  return player.getAnalytics();
}

Analytics
runTest(const char* name, LayoutStyle style, uint32_t sampleCount, const AudioData& ipAudioData) {
  const string testPath = os::getTempFolder() + name + ".vrs";

  RecordFileWriter fileWriter;
  AudioStream audioStream(style, sampleCount, ipAudioData);
  fileWriter.addRecordable(&audioStream);
  audioStream.createAllRecords();
  EXPECT_EQ(fileWriter.writeToFile(testPath), 0);

  return readAudioVRSFile(testPath, ipAudioData);
}

} // namespace

#define TEST_NAME (::testing::UnitTest::GetInstance()->current_test_info()->name())

const uint32_t kTotalSampleCount = 60743;

TEST_F(AudioContentBlockReaderTest, testClassicAudio) {
  ASSERT_GT(stereoAudio.samples.size(), 100000);

  Analytics analytics = runTest(TEST_NAME, LayoutStyle::Classic, 480, stereoAudio);

  EXPECT_EQ(analytics.configDatalayoutCount, 1);
  EXPECT_EQ(analytics.dataDatalayoutCount, analytics.audioBlockCount);
  EXPECT_EQ(analytics.audioBlockCount, 114);
  EXPECT_EQ(analytics.audioSampleCount, kTotalSampleCount);
  EXPECT_EQ(analytics.unsupportedCount, 0);
}

TEST_F(AudioContentBlockReaderTest, testNoSize) {
  ASSERT_GT(stereoAudio.samples.size(), 100000);

  Analytics analytics = runTest(TEST_NAME, LayoutStyle::NoSize, 256, stereoAudio);

  EXPECT_EQ(analytics.configDatalayoutCount, 1);
  EXPECT_EQ(analytics.dataDatalayoutCount, 0);
  EXPECT_EQ(analytics.audioBlockCount, 177);
  EXPECT_EQ(analytics.audioSampleCount, kTotalSampleCount);
  EXPECT_EQ(analytics.unsupportedCount, 0);
}

TEST_F(AudioContentBlockReaderTest, testFullSpecData) {
  ASSERT_GT(stereoAudio.samples.size(), 100000);

  Analytics analytics = runTest(TEST_NAME, LayoutStyle::FullSpecData, 256, stereoAudio);

  EXPECT_EQ(analytics.configDatalayoutCount, 0);
  EXPECT_EQ(analytics.dataDatalayoutCount, analytics.audioBlockCount);
  EXPECT_EQ(analytics.audioBlockCount, 177);
  EXPECT_EQ(analytics.audioSampleCount, kTotalSampleCount);
  EXPECT_EQ(analytics.unsupportedCount, 0);
}

TEST_F(AudioContentBlockReaderTest, testOpusStereo) {
  ASSERT_GT(stereoAudio.samples.size(), 100000);

  const uint32_t kBlockSampleSize = 480; // 10 ms @ 48 kHz
  const uint32_t kBlockCount = (kTotalSampleCount + kBlockSampleSize - 1) / kBlockSampleSize;

  Analytics analytics = runTest(TEST_NAME, LayoutStyle::OpusStereo, kBlockSampleSize, stereoAudio);

  EXPECT_EQ(analytics.configDatalayoutCount, 1);
  EXPECT_EQ(analytics.dataDatalayoutCount, kBlockCount);
  EXPECT_EQ(analytics.audioBlockCount, kBlockCount);
  // we padded the blocks to have the required block size, so we may have more samples
  EXPECT_EQ(analytics.audioSampleCount, kBlockCount * kBlockSampleSize);
  EXPECT_EQ(analytics.unsupportedCount, 0);
}

TEST_F(AudioContentBlockReaderTest, testOpusStereoNoSampleCount) {
  ASSERT_GT(stereoAudio.samples.size(), 100000);

  const uint32_t kBlockSampleSize = 480; // 10 ms @ 48 kHz
  const uint32_t kBlockCount = (kTotalSampleCount + kBlockSampleSize - 1) / kBlockSampleSize;

  Analytics analytics =
      runTest(TEST_NAME, LayoutStyle::OpusStereoNoSampleCount, kBlockSampleSize, stereoAudio);

  EXPECT_EQ(analytics.configDatalayoutCount, 1);
  EXPECT_EQ(analytics.dataDatalayoutCount, 0);
  EXPECT_EQ(analytics.audioBlockCount, kBlockCount);
  // we padded the blocks to have the required block size, so we may have more samples
  EXPECT_EQ(analytics.audioSampleCount, kBlockCount * kBlockSampleSize);
  EXPECT_EQ(analytics.unsupportedCount, 0);
}

TEST_F(AudioContentBlockReaderTest, testOpusMultiChannel) {
  ASSERT_GT(multiAudio.samples.size(), 100000);

  const uint32_t kBlockSampleSize = 480; // 10 ms @ 48 kHz
  const uint32_t kBlockCount = (kTotalSampleCount + kBlockSampleSize - 1) / kBlockSampleSize;

  Analytics analytics = runTest(TEST_NAME, LayoutStyle::OpusStereo, kBlockSampleSize, multiAudio);

  EXPECT_EQ(analytics.configDatalayoutCount, 1);
  EXPECT_EQ(analytics.dataDatalayoutCount, kBlockCount);
  EXPECT_EQ(analytics.audioBlockCount, kBlockCount);
  //  we padded the blocks to have the required block size, so we may have more samples
  EXPECT_EQ(analytics.audioSampleCount, kBlockCount * kBlockSampleSize);
  EXPECT_EQ(analytics.unsupportedCount, 0);
}
