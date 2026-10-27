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

#include <algorithm>
#include <cstdint>
#include <fstream>
#include <iterator>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include <vrs/DataLayoutConventions.h>
#include <vrs/ErrorCode.h>
#include <vrs/RecordFileReader.h>
#include <vrs/RecordFileWriter.h>
#include <vrs/Recordable.h>
#include <vrs/helpers/Strings.h>
#include <vrs/os/Utils.h>
#include <vrs/utils/BufferRecordReader.hpp>
#include <vrs/utils/DataExtractor.h>
#include <vrs/utils/DecoderFactory.h>
#include <vrs/utils/ImageExtractor.h>
#include <vrs/utils/PixelFrame.h>

using namespace std;
using namespace vrs;
using namespace vrs::utils;

namespace {

constexpr const char* kIdentityCodecName = "test_identity_codec";
constexpr const char* kUnregisteredCodecName = "unregistered_codec";
constexpr uint32_t kWidth = 8;
constexpr uint32_t kHeight = 4;

int sUnregisteredCodecLookups = 0;

class IdentityDecoder : public DecoderI {
 public:
  int decode(
      const vector<uint8_t>& encodedFrame,
      void* outDecodedFrame,
      const ImageContentBlockSpec& outputImageSpec) override {
    if (encodedFrame.size() != outputImageSpec.getRawImageSize()) {
      return domainError(DecodeStatus::DecoderError);
    }
    copy(encodedFrame.begin(), encodedFrame.end(), static_cast<uint8_t*>(outDecodedFrame));
    return SUCCESS;
  }
};

unique_ptr<DecoderI> identityDecoderMaker(
    const vector<uint8_t>& /*encodedFrame*/,
    void* /*outDecodedFrame*/,
    const ImageContentBlockSpec& outputImageSpec) {
  if (outputImageSpec.getCodecName() == kUnregisteredCodecName) {
    sUnregisteredCodecLookups++;
  }
  if (outputImageSpec.getCodecName() != kIdentityCodecName) {
    return nullptr;
  }
  return make_unique<IdentityDecoder>();
}

class CustomCodecStream : public Recordable {
 public:
  explicit CustomCodecStream(string codecName)
      : Recordable(RecordableTypeId::UnitTest1), codecName_{std::move(codecName)} {
    addRecordFormat(
        Record::Type::DATA,
        1,
        spec_.getContentBlock() +
            ContentBlock(ImageContentBlockSpec(
                ImageFormat::CUSTOM_CODEC,
                codecName_,
                ImageContentBlockSpec::kQualityUndefined,
                PixelFormat::GREY8,
                kWidth,
                kHeight)),
        {&spec_});
  }
  const Record* createConfigurationRecord() override {
    return createRecord(0, Record::Type::CONFIGURATION, 1);
  }
  const Record* createStateRecord() override {
    return createRecord(0, Record::Type::STATE, 1);
  }
  void writeFrame(double timestamp, const vector<uint8_t>& pixels) {
    spec_.width.set(kWidth);
    spec_.height.set(kHeight);
    spec_.pixelFormat.set(PixelFormat::GREY8);
    spec_.codecName.stage(codecName_);
    createRecord(
        timestamp,
        Record::Type::DATA,
        1,
        DataSource(spec_, DataSourceChunk(pixels.data(), pixels.size())));
  }

 private:
  string codecName_;
  datalayout_conventions::ImageSpec spec_;
};

vector<string> listFiles(const string& folder, const string& prefix) {
  vector<string> names;
  for (const string& path : os::listDir(folder)) {
    string name = os::getFilename(path);
    if (helpers::startsWith(name, prefix)) {
      names.push_back(name);
    }
  }
  sort(names.begin(), names.end());
  return names;
}

vector<uint8_t> readPngPixels(const string& path) {
  ifstream file(path, ios::binary);
  vector<uint8_t> png{istreambuf_iterator<char>(file), istreambuf_iterator<char>()};
  PixelFrame frame;
  if (png.empty() || !frame.readPngFrame(png)) {
    return {};
  }
  return frame.getBuffer();
}

class CustomCodecExtractTest : public testing::Test {
 protected:
  static void SetUpTestSuite() {
    [[maybe_unused]] static const bool sIsIdentityDecoderRegistered = [] {
      DecoderFactory::get().registerDecoderMaker(&identityDecoderMaker);
      return true;
    }();
  }

  void SetUp() override {
    for (size_t frame = 0; frame < pixels_.size(); frame++) {
      pixels_[frame].resize(kWidth * kHeight);
      for (size_t i = 0; i < pixels_[frame].size(); i++) {
        pixels_[frame][i] = static_cast<uint8_t>(i * 7 + frame * 31);
      }
    }
    const vector<uint8_t> truncated(pixels_[0].begin(), pixels_[0].end() - 1);
    RecordFileWriter writer;
    CustomCodecStream decodable(kIdentityCodecName);
    CustomCodecStream undecodable(kUnregisteredCodecName);
    writer.addRecordable(&decodable);
    writer.addRecordable(&undecodable);
    decodable.writeFrame(1, truncated);
    decodable.writeFrame(2, pixels_[1]);
    decodable.writeFrame(3, pixels_[2]);
    for (size_t frame = 0; frame < pixels_.size(); frame++) {
      undecodable.writeFrame(static_cast<double>(frame + 1), pixels_[frame]);
    }
    ASSERT_EQ(writer.writeToFile(path_), 0);
    decodable_ = decodable.getStreamId().getNumericName();
    undecodable_ = undecodable.getStreamId().getNumericName();
    sUnregisteredCodecLookups = 0;
  }

  void expectFiles(const string& decodableFolder, const string& undecodableFolder) {
    const string& d = decodable_;
    const string& u = undecodable_;
    EXPECT_EQ(
        listFiles(decodableFolder, d + "-"),
        (vector<string>{
            d + "-00001-1.000.custom_codec", d + "-00002-2.000.png", d + "-00003-3.000.png"}));
    EXPECT_EQ(
        os::getFileSize(os::pathJoin(decodableFolder, d + "-00001-1.000.custom_codec")),
        static_cast<int64_t>(kWidth * kHeight - 1));
    EXPECT_EQ(readPngPixels(os::pathJoin(decodableFolder, d + "-00002-2.000.png")), pixels_[1]);
    EXPECT_EQ(readPngPixels(os::pathJoin(decodableFolder, d + "-00003-3.000.png")), pixels_[2]);
    const vector<string> blobs = {
        u + "-00001-1.000.custom_codec",
        u + "-00002-2.000.custom_codec",
        u + "-00003-3.000.custom_codec"};
    EXPECT_EQ(listFiles(undecodableFolder, u + "-"), blobs);
    for (const string& blob : blobs) {
      EXPECT_EQ(
          os::getFileSize(os::pathJoin(undecodableFolder, blob)),
          static_cast<int64_t>(kWidth * kHeight));
    }
    EXPECT_EQ(sUnregisteredCodecLookups, 1);
  }

  const string path_ = os::pathJoin(os::getTempFolder(), "CustomCodecExtract.vrs");
  vector<vector<uint8_t>> pixels_ = vector<vector<uint8_t>>(3);
  string decodable_;
  string undecodable_;
};

} // namespace

TEST_F(CustomCodecExtractTest, ImageExtractorWritesPngOrEncodedBlob) {
  const string folder =
      os::getUniquePath(os::pathJoin(os::getTempFolder(), "CustomCodecExtractImages"));
  ASSERT_EQ(os::makeDirectories(folder), 0);
  RecordFileReader reader;
  ASSERT_EQ(reader.openFile(path_), 0);
  {
    uint32_t counter = 0;
    vector<unique_ptr<ImageExtractor>> extractors;
    for (StreamId id : reader.getStreams()) {
      extractors.emplace_back(make_unique<ImageExtractor>(folder, counter, false));
      reader.setStreamPlayer(id, extractors.back().get());
    }
    ASSERT_EQ(reader.readAllRecords(), 0);
  }
  expectFiles(folder, folder);
}

TEST_F(CustomCodecExtractTest, ReadCustomCodecFrameReportsMissingDecoder) {
  const auto block = [](const char* codecName, size_t size) {
    return ContentBlock(
        ImageContentBlockSpec(
            ImageFormat::CUSTOM_CODEC,
            codecName,
            ImageContentBlockSpec::kQualityUndefined,
            PixelFormat::GREY8,
            kWidth,
            kHeight),
        size);
  };
  const vector<uint8_t> truncated(pixels_[0].begin(), pixels_[0].end() - 1);
  const vector<uint8_t> empty;
  PixelFrame frame;
  BufferReader reader;
  bool hasDecoder = true;
  EXPECT_FALSE(frame.readCustomCodecFrame(
      reader.init(pixels_[1]), block(kUnregisteredCodecName, pixels_[1].size()), hasDecoder));
  EXPECT_FALSE(hasDecoder);
  EXPECT_FALSE(
      frame.readCustomCodecFrame(reader.init(empty), block(kUnregisteredCodecName, 0), hasDecoder));
  EXPECT_TRUE(hasDecoder);
  hasDecoder = false;
  EXPECT_FALSE(frame.readCustomCodecFrame(
      reader.init(truncated), block(kIdentityCodecName, truncated.size()), hasDecoder));
  EXPECT_TRUE(hasDecoder);
  hasDecoder = false;
  EXPECT_TRUE(frame.readCustomCodecFrame(
      reader.init(pixels_[1]), block(kIdentityCodecName, pixels_[1].size()), hasDecoder));
  EXPECT_TRUE(hasDecoder);
  EXPECT_EQ(frame.getBuffer(), pixels_[1]);
  EXPECT_FALSE(frame.readCustomCodecFrame(
      reader.init(truncated), block(kIdentityCodecName, truncated.size()), hasDecoder));
  EXPECT_EQ(frame.getBuffer(), pixels_[1]);
}

TEST_F(CustomCodecExtractTest, DataExtractorWritesPngOrEncodedBlob) {
  const string folder =
      os::getUniquePath(os::pathJoin(os::getTempFolder(), "CustomCodecExtractAll"));
  ASSERT_EQ(DataExtractor::extractAll(path_, folder), 0);
  expectFiles(os::pathJoin(folder, decodable_), os::pathJoin(folder, undecodable_));
}
