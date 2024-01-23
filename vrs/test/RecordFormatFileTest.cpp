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

#include <ctime>

#include <gtest/gtest.h>

#include <test_helpers/GTestMacros.h>

#define DEFAULT_LOG_CHANNEL "RecordFormatFileTest"
#include <logging/Log.h>

#include <vrs/DataLayoutConventions.h>
#include <vrs/FileFormat.h>
#include <vrs/RecordFileReader.h>
#include <vrs/RecordFileWriter.h>
#include <vrs/RecordFormatStreamPlayer.h>
#include <vrs/Recordable.h>
#include <vrs/TagConventions.h>
#include <vrs/helpers/Strings.h>
#include <vrs/os/Time.h>
#include <vrs/os/Utils.h>

using namespace std;
using namespace vrs;
using namespace vrs::datalayout_conventions;

namespace {

// Some not so-nice strings to verify they are saved & restored as provided...
const string kBadString1 = "\0x00hello\0x00";
const string kBadString2 = "\t1PASH3T1RS8113\n";

// The SantaCruzCamera definitions were copied from the real SantaCruz definitions,
// which aren't visible from the VRS module.
// They are used to test DataLayout, not the other way around, so this duplication can be done,
// without caring about future changes to the official SantaCruzCamera definitions.
namespace SantaCruzCamera {

using ::vrs::AutoDataLayout;
using ::vrs::AutoDataLayoutEnd;
using ::vrs::DataPieceArray;
using ::vrs::DataPieceValue;
using ::vrs::DataReference;
using ::vrs::Point3Df;
using ::vrs::Point4Df;
using ::vrs::datalayout_conventions::ImageSpecType;
using ::vrs::FileFormat::LittleEndian;

const int32_t kCalibrationDataSize = 22;
const uint32_t kConfigurationVersion = 5;

#pragma pack(push, 1)

struct VRSDataV2 {
  enum : uint32_t { kVersion = 2 };

  LittleEndian<double> captureTimestamp;
  LittleEndian<double> arrivalTimestamp;
  LittleEndian<uint64_t> frameCounter;
  LittleEndian<uint32_t> cameraUniqueId;
};

struct VRSDataV3 : VRSDataV2 {
  enum : uint32_t { kVersion = 3 };

  void upgradeFrom(uint32_t formatVersion) {
    if (formatVersion < kVersion) {
      streamId.set(0);
      gainHAL.set(0);
    }
  }

  LittleEndian<int32_t> streamId;
  LittleEndian<uint32_t> gainHAL;
};

struct VRSDataV4 : VRSDataV3 {
  enum : uint32_t { kVersion = 4 };

  void upgradeFrom(uint32_t formatVersion) {
    if (formatVersion < kVersion) {
      VRSDataV3::upgradeFrom(formatVersion);
      exposureDuration.set(0);
    }
  }

  LittleEndian<double> exposureDuration;
};

constexpr float kGainMultiplierConvertor = 16.0;

struct VRSDataV5 : VRSDataV4 {
  enum : uint32_t { kVersion = 5 };

  void upgradeFrom(uint32_t formatVersion) {
    if (formatVersion < kVersion) {
      VRSDataV4::upgradeFrom(formatVersion);
      gain.set(gainHAL.get() / kGainMultiplierConvertor);
    }
  }

  LittleEndian<float> gain;
};

struct VRSData : VRSDataV5 {
  enum : uint32_t { kVersion = 6 };

  bool canHandle(
      const CurrentRecord& record,
      void* imageData,
      uint32_t imageSize,
      DataReference& outDataReference) {
    uint32_t formatVersion = record.formatVersion;
    uint32_t payloadSize = record.recordSize;
    if ((formatVersion == kVersion && sizeof(VRSData) + imageSize == payloadSize) ||
        (formatVersion == VRSDataV5::kVersion && sizeof(VRSDataV5) + imageSize == payloadSize) ||
        (formatVersion == VRSDataV4::kVersion && sizeof(VRSDataV4) + imageSize == payloadSize) ||
        (formatVersion == VRSDataV3::kVersion && sizeof(VRSDataV3) + imageSize == payloadSize) ||
        (formatVersion == VRSDataV2::kVersion && sizeof(VRSDataV2) + imageSize == payloadSize)) {
      outDataReference.useRawData(this, payloadSize - imageSize, imageData, imageSize);
      return true;
    }
    return false;
  }

  void upgradeFrom(uint32_t formatVersion) {
    if (formatVersion < kVersion) {
      VRSDataV5::upgradeFrom(formatVersion);
      temperature.set(-1.0);
    }
  }

  LittleEndian<float> temperature;
};

struct VRSConfiguration {
  VRSConfiguration() = default;

  LittleEndian<uint32_t> width;
  LittleEndian<uint32_t> height;
  LittleEndian<uint32_t> bytesPerPixels;
  LittleEndian<uint32_t> format;
  LittleEndian<uint32_t> cameraId;
  LittleEndian<uint16_t> cameraSerial;
  LittleEndian<float> calibration[kCalibrationDataSize];
};

#pragma pack(pop)

// The types & names of some of these fields are using the new DataLayout conventions
// for ImageContentBlocks. See VRS/DataLayoutConventions.h
class DataLayoutConfiguration : public AutoDataLayout {
 public:
  enum : uint32_t { kVersion = 5 };

  DataPieceValue<ImageSpecType> width{::vrs::datalayout_conventions::kImageWidth};
  DataPieceValue<ImageSpecType> height{::vrs::datalayout_conventions::kImageHeight};
  DataPieceValue<ImageSpecType> bytesPerPixels{::vrs::datalayout_conventions::kImageBytesPerPixel};
  DataPieceValue<ImageSpecType> format{::vrs::datalayout_conventions::kImagePixelFormat};
  DataPieceValue<uint32_t> cameraId{"camera_id"};
  DataPieceValue<uint16_t> cameraSerial{"camera_serial"};
  DataPieceArray<float> calibration{"camera_calibration", kCalibrationDataSize};

  AutoDataLayoutEnd endLayout;
};

class DataLayoutDataV2 : public AutoDataLayout {
 public:
  enum : uint32_t { kVersion = 2 };

  DataPieceValue<double> captureTimestamp{"capture_timestamp"};
  DataPieceValue<double> arrivalTimestamp{"arrival_timestamp"};
  DataPieceValue<uint64_t> frameCounter{"frame_counter"};
  DataPieceValue<uint32_t> cameraUniqueId{"camera_unique_id"};

  AutoDataLayoutEnd endLayout;
};

class DataLayoutData : public AutoDataLayout {
 public:
  enum : uint32_t { kVersion = 6 };
  // v2
  DataPieceValue<double> captureTimestamp{"capture_timestamp"};
  DataPieceValue<double> arrivalTimestamp{"arrival_timestamp"};
  DataPieceValue<uint64_t> frameCounter{"frame_counter"};
  DataPieceValue<uint32_t> cameraUniqueId{"camera_unique_id"};
  // v3
  DataPieceValue<int32_t> streamId{"stream_id", 0};
  DataPieceValue<uint32_t> gainHAL{"gain_hal", 0};
  // v4
  DataPieceValue<double> exposureDuration{"exposure_duration", 0};
  // v5
  DataPieceValue<float> gain{"gain", 0}; // complex default value: force calling a method
 public:
  float getGain() {
    if (gain.isAvailable()) {
      return gain.get();
    }
    return gainHAL.get() / kGainMultiplierConvertor;
  }
  // v6
  DataPieceValue<float> temperature{"temperature", -1};

  AutoDataLayoutEnd endLayout;
};

} // namespace SantaCruzCamera

class VariableImageSpec : public AutoDataLayout {
 public:
  DataPieceValue<ImageSpecType> width{kImageWidth};
  DataPieceValue<ImageSpecType> height{kImageHeight};
  DataPieceEnum<PixelFormat, ImageSpecType> pixelFormat{kImagePixelFormat};
  DataPieceString name{"some_name"};
  DataPieceVector<string> names{"some_names"};
  DataPieceVector<string> more_names{"more_names"};
  DataPieceStringMap<int32_t> stringMapInt{"string_map_int"};
  DataPieceStringMap<Point2Di> stringMapPointInt{"string_map_point_int"};
  DataPieceStringMap<Matrix3Di> stringMapMatrixInt{"string_map_matrix_int"};

  AutoDataLayoutEnd end;
};

const uint32_t kVariableImageRecordFormatVersion = 100;

class OtherRecordable : public Recordable {
 public:
  explicit OtherRecordable(uint32_t cameraId) : Recordable(RecordableTypeId::UnitTest2) {
    addRecordFormat(
        Record::Type::CONFIGURATION,
        SantaCruzCamera::DataLayoutConfiguration::kVersion,
        config.getContentBlock(),
        {&config});
    addRecordFormat(
        Record::Type::DATA,
        SantaCruzCamera::DataLayoutData::kVersion,
        data.getContentBlock(),
        {&data});
    config.cameraId.set(cameraId);
  }
  const Record* createConfigurationRecord() override {
    return createRecord(
        os::getTimestampSec(),
        Record::Type::CONFIGURATION,
        SantaCruzCamera::DataLayoutConfiguration::kVersion,
        DataSource(config));
  }
  const Record* createStateRecord() override {
    return createRecord(os::getTimestampSec(), Record::Type::STATE, 0);
  }
  void createDataRecord() {
    data.frameCounter.set(++frameCounter);
    createRecord(
        os::getTimestampSec(),
        Record::Type::DATA,
        SantaCruzCamera::DataLayoutData::kVersion,
        DataSource(data));
  }

  SantaCruzCamera::DataLayoutConfiguration config;
  SantaCruzCamera::DataLayoutData data;
  uint64_t frameCounter{};
};

class OtherStreamPlayer : public RecordFormatStreamPlayer {
 public:
  bool onDataLayoutRead(const CurrentRecord& record, size_t blockIndex, DataLayout& layout)
      override {
    if (record.recordType == Record::Type::CONFIGURATION) {
      if (record.formatVersion == SantaCruzCamera::DataLayoutConfiguration::kVersion) {
        SantaCruzCamera::DataLayoutConfiguration& expectedLayout =
            getExpectedLayout<SantaCruzCamera::DataLayoutConfiguration>(layout, blockIndex);
        cameraId = expectedLayout.cameraId.get();
      }
    } else if (record.recordType == Record::Type::DATA) {
      if (record.formatVersion == SantaCruzCamera::DataLayoutData::kVersion) {
        SantaCruzCamera::DataLayoutData& expectedLayout =
            getExpectedLayout<SantaCruzCamera::DataLayoutData>(layout, blockIndex);
        frameCounter = expectedLayout.frameCounter.get();
      }
    }
    return true;
  }
  bool processRecordHeader(const CurrentRecord& record, DataReference& outDataReference) override {
    if (record.recordType == Record::Type::DATA) {
      if (record.reader->looksCompressed()) {
        usesCompression = true;
      }
    }
    return RecordFormatStreamPlayer::processRecordHeader(record, outDataReference);
  }

  uint32_t cameraId{};
  uint64_t frameCounter{};
  bool usesCompression{false};
};

class DataLayoutFileTest : public Recordable, RecordFormatStreamPlayer {
  const uint32_t kStateVersion = 1;
  const double kTime = 100;
  const size_t kFrameCount = 10; // count for each frame type

 public:
  explicit DataLayoutFileTest(string fileName)
      : Recordable(RecordableTypeId::UnitTest1), fileName_{std::move(fileName)} {}

  const Record* createStateRecord() override {
    return createRecord(kTime, Record::Type::STATE, kStateVersion);
  }

  const Record* createConfigurationRecord() override {
    configurationCount_++;
    if (configurationCount_ % 2 == 0) {
      // VRS 1.0 style record creation
      SantaCruzCamera::VRSConfiguration vrsConfig;
      vrsConfig.width.set(640 + configurationCount_);
      vrsConfig.height.set(480 + configurationCount_);
      vrsConfig.bytesPerPixels.set(1);
      vrsConfig.format.set(1);
      vrsConfig.cameraId.set(123456);
      vrsConfig.cameraSerial.set(11);
      vrsConfig.calibration[0].set(1);
      vrsConfig.calibration[1].set(2);
      vrsConfig.calibration[2].set(3);
      vrsConfig.calibration[3].set(4);
      vrsConfig.calibration[4].set(5);
      vrsConfig.calibration[5].set(6);
      return createRecord(
          kTime + fixedImageCount_ - 0.1,
          Record::Type::CONFIGURATION,
          SantaCruzCamera::kConfigurationVersion,
          DataSource(vrsConfig));
    } else {
      // VRS 2.0 style record creation
      SantaCruzCamera::DataLayoutConfiguration vrsConfig;
      vrsConfig.width.set(640 + configurationCount_);
      vrsConfig.height.set(480 + configurationCount_);
      vrsConfig.bytesPerPixels.set(1);
      vrsConfig.format.set(1);
      vrsConfig.cameraId.set(123456);
      vrsConfig.cameraSerial.set(11);
      vrsConfig.calibration.set({1, 2, 3, 4, 5, 6});
      return createRecord(
          kTime + fixedImageCount_ - 0.1,
          Record::Type::CONFIGURATION,
          SantaCruzCamera::kConfigurationVersion,
          DataSource(vrsConfig));
    }
  }

  // Fixed frame have the same size, specified in the last configuration record
  // Every 3 frames, we generate a new configuration with a new dimension to mix things up
  void createFixedFrame() {
    // Change configuration at some point
    if (fixedImageCount_ % 3 == 0) {
      createConfigurationRecord();
    }
    vector<int8_t> buffer;
    buffer.resize((640 + configurationCount_) * (480 + configurationCount_) * 1);
    for (size_t k = 0; k < buffer.size(); ++k) {
      buffer[k] = static_cast<int8_t>(k);
    }
    if (fixedImageCount_ % 2 == 0) {
      // VRS 1.0 style record creation
      SantaCruzCamera::VRSDataV2 data;
      data.captureTimestamp.set(0.5 * fixedImageCount_);
      data.arrivalTimestamp.set(kTime + 0.1 * fixedImageCount_);
      data.frameCounter.set(fixedImageCount_);
      data.cameraUniqueId.set(123456 + fixedImageCount_);
      createRecord(
          kTime + fixedImageCount_++,
          Record::Type::DATA,
          SantaCruzCamera::VRSDataV2::kVersion,
          DataSource(data, buffer));
    } else {
      // VRS 2.0 style record creation
      SantaCruzCamera::DataLayoutData layout;
      layout.captureTimestamp.set(0.5 * fixedImageCount_);
      layout.arrivalTimestamp.set(kTime + 0.1 * fixedImageCount_);
      layout.frameCounter.set(fixedImageCount_);
      layout.cameraUniqueId.set(123456 + fixedImageCount_);
      layout.streamId.set(static_cast<int32_t>(fixedImageCount_ * 2));
      layout.gainHAL.set(fixedImageCount_ * 3);
      layout.exposureDuration.set(0.01 * fixedImageCount_);
      layout.temperature.set(0.2f * fixedImageCount_);
      createRecord(
          kTime + fixedImageCount_++,
          Record::Type::DATA,
          SantaCruzCamera::DataLayoutData::kVersion,
          DataSource(layout, buffer));
    }
  }

  // Variable frames have a resolution specified in the datalayout just before the image block
  void createVariableFrame() {
    uint32_t width = 10 + variableImageCount_;
    imageSpec_.width.set(width);
    uint32_t height = 50 + variableImageCount_;
    imageSpec_.height.set(height);
    PixelFormat pixelFormat = PixelFormat::BGR8;
    imageSpec_.pixelFormat.set(pixelFormat);
    imageSpec_.name.stage(to_string(width));
    imageSpec_.names.stage({"hello", "", "bonjour"});
    imageSpec_.more_names.stage({"hi", "", "cio"});
    imageSpec_.stringMapInt.stage({{"one", 1}, {"two", 2}, {"three", 3}});
    imageSpec_.stringMapPointInt.stage({{"first", {1, 2}}, {"second", {3, 4}}});
    int mat[3][3] = {{1, 2, 3}, {4, 5, 6}, {7, 8, 9}};
    imageSpec_.stringMapMatrixInt.stage({{"single", mat}});
    vector<int8_t> buffer;
    buffer.resize(width * height * ImageContentBlockSpec::getBytesPerPixel(pixelFormat));
    for (size_t k = 0; k < buffer.size(); ++k) {
      buffer[k] = static_cast<int8_t>(k + 1);
    }
    createRecord(
        kTime + variableImageCount_++ + 0.1,
        Record::Type::DATA,
        kVariableImageRecordFormatVersion,
        DataSource(imageSpec_, buffer));
  }

  bool onDataLayoutRead(const CurrentRecord& record, size_t blockIndex, DataLayout& layout)
      override {
    if (record.recordType == Record::Type::DATA) {
      if (record.formatVersion == SantaCruzCamera::VRSDataV2::kVersion) {
        // VRS 1.0 style record for a fixed-size frame
        EXPECT_EQ(blockIndex, 0);
        SantaCruzCamera::DataLayoutData& expectedLayout =
            getExpectedLayout<SantaCruzCamera::DataLayoutData>(layout, blockIndex);
        uint64_t frameNumber = static_cast<uint32_t>(expectedLayout.frameCounter.get());
        EXPECT_EQ(record.timestamp, kTime + frameNumber);
        EXPECT_EQ(expectedLayout.captureTimestamp.get(), 0.5 * frameNumber);
        EXPECT_EQ(expectedLayout.arrivalTimestamp.get(), kTime + 0.1 * frameNumber);
        EXPECT_EQ(expectedLayout.cameraUniqueId.get(), 123456 + frameNumber);
        EXPECT_FALSE(expectedLayout.streamId.isAvailable());
        EXPECT_EQ(expectedLayout.streamId.get(), 0);
        EXPECT_FALSE(expectedLayout.gainHAL.isAvailable());
        EXPECT_EQ(expectedLayout.gainHAL.get(), 0);
        EXPECT_FALSE(expectedLayout.exposureDuration.isAvailable());
        EXPECT_EQ(expectedLayout.exposureDuration.get(), 0);
        EXPECT_FALSE(expectedLayout.temperature.isAvailable());
        EXPECT_EQ(expectedLayout.temperature.get(), -1);
      } else if (record.formatVersion == SantaCruzCamera::DataLayoutData::kVersion) {
        // VRS 2.0 style record for a fixed-size frame
        EXPECT_EQ(blockIndex, 0);
        SantaCruzCamera::DataLayoutData& expectedLayout =
            getExpectedLayout<SantaCruzCamera::DataLayoutData>(layout, blockIndex);
        uint64_t frameNumber = static_cast<uint32_t>(expectedLayout.frameCounter.get());
        EXPECT_EQ(record.timestamp, kTime + frameNumber);
        EXPECT_EQ(expectedLayout.captureTimestamp.get(), 0.5 * frameNumber);
        EXPECT_EQ(expectedLayout.arrivalTimestamp.get(), kTime + 0.1 * frameNumber);
        EXPECT_EQ(expectedLayout.cameraUniqueId.get(), 123456 + frameNumber);
        EXPECT_TRUE(expectedLayout.streamId.isAvailable());
        EXPECT_EQ(expectedLayout.streamId.get(), static_cast<int32_t>(frameNumber * 2));
        EXPECT_TRUE(expectedLayout.gainHAL.isAvailable());
        EXPECT_EQ(expectedLayout.gainHAL.get(), frameNumber * 3);
        EXPECT_TRUE(expectedLayout.exposureDuration.isAvailable());
        EXPECT_EQ(expectedLayout.exposureDuration.get(), frameNumber * 0.01);
        EXPECT_TRUE(expectedLayout.temperature.isAvailable());
        EXPECT_EQ(expectedLayout.temperature.get(), frameNumber * 0.2f);
      } else {
        // VRS 2.0 style record for a variable-size frame
        EXPECT_EQ(record.formatVersion, kVariableImageRecordFormatVersion);
        EXPECT_EQ(blockIndex, 0);
        VariableImageSpec& expectedLayout =
            getExpectedLayout<VariableImageSpec>(layout, blockIndex);
        EXPECT_EQ(expectedLayout.width.get(), 10 + variableImageCount_);
        EXPECT_EQ(expectedLayout.height.get(), 50 + variableImageCount_);
        EXPECT_EQ(expectedLayout.pixelFormat.get(), PixelFormat::BGR8);
        EXPECT_TRUE(expectedLayout.name.isAvailable());
        EXPECT_STREQ(
            expectedLayout.name.get().c_str(), to_string(expectedLayout.width.get()).c_str());
        vector<string> names;
        EXPECT_TRUE(expectedLayout.names.get(names));
        EXPECT_EQ(names.size(), 3);
        EXPECT_STREQ(names[0].c_str(), "hello");
        EXPECT_STREQ(names[1].c_str(), "");
        EXPECT_STREQ(names[2].c_str(), "bonjour");
        EXPECT_TRUE(expectedLayout.more_names.get(names));
        EXPECT_EQ(names.size(), 3);
        EXPECT_STREQ(names[0].c_str(), "hi");
        EXPECT_STREQ(names[1].c_str(), "");
        EXPECT_STREQ(names[2].c_str(), "cio");

        map<string, int32_t> mapInt;
        EXPECT_TRUE(expectedLayout.stringMapInt.get(mapInt));
        EXPECT_EQ(mapInt.size(), 3);
        EXPECT_EQ(mapInt["one"], 1);
        EXPECT_EQ(mapInt["two"], 2);
        EXPECT_EQ(mapInt["three"], 3);
        map<string, Point2Di> mapPoint;
        EXPECT_TRUE(expectedLayout.stringMapPointInt.get(mapPoint));
        EXPECT_EQ(mapPoint.size(), 2);
        EXPECT_EQ(mapPoint["first"], Point2Di({1, 2}));
        EXPECT_EQ(mapPoint["second"], Point2Di({3, 4}));
        map<string, Matrix3Di> mapMatrix;
        EXPECT_TRUE(expectedLayout.stringMapMatrixInt.get(mapMatrix));
        EXPECT_EQ(mapMatrix.size(), 1);
        int mat[3][3] = {{1, 2, 3}, {4, 5, 6}, {7, 8, 9}};
        EXPECT_EQ(mapMatrix["single"], Matrix3Di(mat));
      }
    } else if (record.recordType == Record::Type::CONFIGURATION) {
      configurationCount_++;
      EXPECT_EQ(record.formatVersion, SantaCruzCamera::kConfigurationVersion);
      EXPECT_EQ(blockIndex, 0);
      SantaCruzCamera::DataLayoutConfiguration& expectedLayout =
          getExpectedLayout<SantaCruzCamera::DataLayoutConfiguration>(layout, blockIndex);
      EXPECT_EQ(expectedLayout.width.get(), 640 + configurationCount_);
      EXPECT_EQ(expectedLayout.height.get(), 480 + configurationCount_);
      EXPECT_EQ(expectedLayout.bytesPerPixels.get(), 1);
      EXPECT_EQ(expectedLayout.format.get(), 1);
      EXPECT_EQ(expectedLayout.cameraId.get(), 123456);
      EXPECT_EQ(expectedLayout.cameraSerial.get(), 11);
      vector<float> calibrationData;
      EXPECT_TRUE(expectedLayout.calibration.get(calibrationData));
      size_t countExpect = SantaCruzCamera::kCalibrationDataSize;
      EXPECT_EQ(calibrationData.size(), countExpect);
      for (size_t k = 0; k < countExpect; k++) {
        EXPECT_EQ(calibrationData[k], static_cast<float>((k < 6) ? k + 1 : 0));
      }
    }
    return true;
  }

  bool onUnsupportedBlock(
      const CurrentRecord& record,
      size_t size,
      const ContentBlock& contentBlock) override {
    unsupportedBlockCount_++;
    return RecordFormatStreamPlayer::onUnsupportedBlock(record, size, contentBlock);
  }

  bool onImageRead(const CurrentRecord& record, size_t blockIndex, const ContentBlock& contentBlock)
      override {
    EXPECT_EQ(record.recordType, Record::Type::DATA);
    vector<int8_t> buffer(contentBlock.getBlockSize());
    EXPECT_GT(buffer.size(), 0);
    int readStatus = record.reader->read(buffer);
    EXPECT_EQ(readStatus, 0);
    if (readStatus == 0) {
      if (record.formatVersion == SantaCruzCamera::VRSDataV2::kVersion ||
          record.formatVersion == SantaCruzCamera::DataLayoutData::kVersion) {
        EXPECT_EQ(contentBlock.image().getWidth(), 640 + configurationCount_);
        EXPECT_EQ(contentBlock.image().getHeight(), 480 + configurationCount_);
        fixedImageCount_++;
        EXPECT_EQ(blockIndex, 1);
        for (size_t k = 0; k < buffer.size(); ++k) {
          EXPECT_EQ(buffer[k], static_cast<int8_t>(k));
        }
      } else {
        variableImageCount_++;
        EXPECT_EQ(record.formatVersion, 100);
        EXPECT_EQ(blockIndex, 1);
        for (size_t k = 0; k < buffer.size(); ++k) {
          EXPECT_EQ(buffer[k], static_cast<int8_t>(k + 1));
        }
      }
    }
    return true;
  }

  bool processRecordHeader(const CurrentRecord& record, DataReference& outDataReference) override {
    if (record.recordType == Record::Type::DATA) {
      if (record.reader->looksCompressed()) {
        usesCompression = true;
      }
    }
    return RecordFormatStreamPlayer::processRecordHeader(record, outDataReference);
  }

  int createVrsFile() {
    RecordFileWriter fileWriter;
    tag_conventions::addOsFingerprint(fileWriter);
    tag_conventions::addCaptureTime(fileWriter);
    tag_conventions::addTagSet(fileWriter, {"tag1", "tag2", "tag3"});
    fileWriter.setTag("bad_tag", kBadString1);
    setRecordableIsActive(true);
    SantaCruzCamera::DataLayoutConfiguration config;
    this->addRecordFormat(
        Record::Type::CONFIGURATION,
        SantaCruzCamera::DataLayoutConfiguration::kVersion,
        config.getContentBlock(),
        {&config});
    // We create 3 types of data records
    // 1 - variable size data records, with a size specified in the datalayout of the data record!
    VariableImageSpec varImageSpec;
    ContentBlock rawImage(ImageFormat::RAW);
    this->addRecordFormat(
        Record::Type::DATA,
        kVariableImageRecordFormatVersion,
        varImageSpec.getContentBlock() + rawImage,
        {&varImageSpec});
    // 2 - fixed size data records (size in config record), datalayout style
    SantaCruzCamera::DataLayoutData santaCruzDataLayoutData;
    this->addRecordFormat(
        Record::Type::DATA,
        SantaCruzCamera::DataLayoutData::kVersion,
        SantaCruzCamera::DataLayoutData().getContentBlock() + rawImage,
        {&santaCruzDataLayoutData});
    // 3 - fixed size data records (size in config record), VRS 1.0 style
    SantaCruzCamera::DataLayoutDataV2 santaCruzDataLayoutv2;
    this->addRecordFormat(
        Record::Type::DATA,
        SantaCruzCamera::DataLayoutDataV2::kVersion,
        santaCruzDataLayoutv2.getContentBlock() + rawImage,
        {&santaCruzDataLayoutv2});
    configurationCount_ = 0;
    fixedImageCount_ = 0;
    variableImageCount_ = 0;
    createStateRecord();
    for (uint32_t frame = 0; frame < kFrameCount; frame++) {
      createFixedFrame();
      createVariableFrame();
    }
    this->setTag("some_tag_name", "some_tag_value");
    this->setTag("some_bad_tag", kBadString2);
    OtherRecordable other1(1);
    other1.setTag("which", "other1");
    other1.setTag("other_tag", "tag value");
    fileWriter.addRecordable(&other1);
    fileWriter.createFileAsync(fileName_);
    fileWriter.addRecordable(this);
    OtherRecordable other2(2);
    other2.setTag("which", "other2");
    fileWriter.addRecordable(&other2);
    other1.createDataRecord();
    other2.createDataRecord();
    other1.createDataRecord();
    other1.createDataRecord();
    fileWriter.purgeOldRecords(0);
    return fileWriter.waitForFileClosed();
  }

  int readVrsFile(time_t timeBefore) {
    unsupportedBlockCount_ = 0;
    configurationCount_ = 0;
    fixedImageCount_ = 0;
    variableImageCount_ = 0;
    RecordFileReader filePlayer;
    RETURN_ON_FAILURE(filePlayer.openFile(fileName_));
    EXPECT_TRUE(filePlayer.hasIndex());

    // Check some of the file's tags
    string tag;
    tag = filePlayer.getTag(tag_conventions::kOsFingerprint);
    EXPECT_GT(tag.size(), 1); // almost any value will do...
    tag = filePlayer.getTag(tag_conventions::kCaptureTimeEpoch);
    time_t epoch = static_cast<time_t>(atoll(tag.c_str()));
    EXPECT_GE(epoch, timeBefore);
    EXPECT_LE(epoch, timeBefore + 20); // let's give it 20 seconds!
    tag = filePlayer.getTag(tag_conventions::kTagSet);
    EXPECT_EQ(strcmp(tag.c_str(), "{\"tags\":[\"tag1\",\"tag2\",\"tag3\"]}"), 0);
    EXPECT_EQ(
        helpers::make_printable(filePlayer.getTag("bad_tag")),
        helpers::make_printable(kBadString1));
    vector<string> tags;
    tag_conventions::parseTagSet(tag, tags);
    EXPECT_EQ(tags.size(), 3);
    EXPECT_EQ(strcmp(tags[0].c_str(), "tag1"), 0);
    EXPECT_EQ(strcmp(tags[1].c_str(), "tag2"), 0);
    EXPECT_EQ(strcmp(tags[2].c_str(), "tag3"), 0);

    const set<StreamId>& streamIds = filePlayer.getStreams();
    EXPECT_EQ(streamIds.size(), 3);

    // Check the stream's tags
    StreamId id = filePlayer.getStreamForType(RecordableTypeId::UnitTest1, 0);
    EXPECT_EQ(id.getTypeId(), RecordableTypeId::UnitTest1);
    EXPECT_TRUE(id.isValid());
    filePlayer.setStreamPlayer(id, this);
    EXPECT_EQ(filePlayer.getTags(id).vrs.size(), 10);
    EXPECT_EQ(filePlayer.getTags(id).user.size(), 2);
    EXPECT_EQ(filePlayer.getTag(id, "some_tag_name"), "some_tag_value");
    EXPECT_EQ(
        helpers::make_printable(filePlayer.getTag(id, "some_bad_tag")),
        helpers::make_printable(kBadString2));

    // Look for the "other" recordables & prepare to read them
    StreamId other1 = filePlayer.getStreamForTag("which", "other1", RecordableTypeId::UnitTest2);
    EXPECT_EQ(other1.getTypeId(), RecordableTypeId::UnitTest2);
    EXPECT_TRUE(other1.isValid());
    EXPECT_EQ(strcmp(filePlayer.getTag(other1, "other_tag").c_str(), "tag value"), 0);
    OtherStreamPlayer streamPlayerOther1;
    filePlayer.setStreamPlayer(other1, &streamPlayerOther1);
    StreamId other2 = filePlayer.getStreamForTag("which", "other2");
    EXPECT_EQ(other2.getTypeId(), RecordableTypeId::UnitTest2);
    EXPECT_TRUE(other2.isValid());
    EXPECT_EQ(strcmp(filePlayer.getTag(other2, "other_tag").c_str(), ""), 0);
    OtherStreamPlayer streamPlayerOther2;
    filePlayer.setStreamPlayer(other2, &streamPlayerOther2);

    // Let's try reading an image before reading the configuration record
    // DataLayout should not be able to figure it out, and will issue a warning
    auto dataRecord = filePlayer.getRecord(id, Record::Type::DATA, 0);
    EXPECT_TRUE(dataRecord != nullptr);
    if (dataRecord != nullptr) {
      EXPECT_EQ(filePlayer.readRecord(*dataRecord), 0);
      EXPECT_EQ(unsupportedBlockCount_, 1);
      EXPECT_EQ(fixedImageCount_, 0);
      EXPECT_EQ(variableImageCount_, 0);
      EXPECT_EQ(configurationCount_, 0);
      EXPECT_TRUE(usesCompression);

      usesCompression = false;

      EXPECT_TRUE(filePlayer.readFirstConfigurationRecord(id));
      EXPECT_EQ(configurationCount_, 1);
      EXPECT_EQ(filePlayer.readRecord(*dataRecord), 0);
      EXPECT_EQ(configurationCount_, 1); // no increase
      EXPECT_EQ(unsupportedBlockCount_, 1); // no increase
      EXPECT_EQ(fixedImageCount_, 1);
      EXPECT_EQ(variableImageCount_, 0);
      EXPECT_TRUE(usesCompression);

      fixedImageCount_ = 0;
      configurationCount_ = 0;
      unsupportedBlockCount_ = 0;
      usesCompression = false;

      EXPECT_TRUE(filePlayer.readFirstConfigurationRecordsForType(RecordableTypeId::UnitTest1));
      EXPECT_EQ(configurationCount_, 1);

      fixedImageCount_ = 0;
      configurationCount_ = 0;
      unsupportedBlockCount_ = 0;
      usesCompression = false;
    }

    EXPECT_EQ(filePlayer.readAllRecords(), 0);
    EXPECT_EQ(unsupportedBlockCount_, 0);
    EXPECT_EQ(kFrameCount, fixedImageCount_);
    EXPECT_EQ(kFrameCount, variableImageCount_);
    EXPECT_TRUE(usesCompression);

    // Prove that the "other" stream players were properly decoded too, both config & data
    EXPECT_EQ(streamPlayerOther1.cameraId, 1);
    EXPECT_EQ(streamPlayerOther1.frameCounter, 3);
    EXPECT_EQ(streamPlayerOther2.cameraId, 2);
    EXPECT_EQ(streamPlayerOther2.frameCounter, 1);
    EXPECT_FALSE(streamPlayerOther1.usesCompression); // records too small
    EXPECT_FALSE(streamPlayerOther2.usesCompression); // records too small
    return filePlayer.closeFile();
  }

 private:
  string fileName_;
  size_t unsupportedBlockCount_{};
  uint32_t configurationCount_{};
  uint32_t fixedImageCount_{};
  uint32_t variableImageCount_{};
  VariableImageSpec imageSpec_;
  bool usesCompression{false};
};

struct DataLayoutFileTester : testing::Test {
  static const string& getTestFilePath() {
    static const string sTestFilePath = os::getTempFolder() + "DataLayoutTest.vrs";
    return sTestFilePath;
  }

  DataLayoutFileTester() : recordable{getTestFilePath()} {}

  DataLayoutFileTest recordable;
};

} // namespace

TEST_F(DataLayoutFileTester, createAndReadDataLayoutFile) {
  // arvr::logging::Channel::setGlobalLevel(arvr::logging::Level::Debug);
  time_t timeBefore = time(nullptr);
  ASSERT_EQ(recordable.createVrsFile(), 0);

  EXPECT_EQ(recordable.readVrsFile(timeBefore), 0);

  os::remove(DataLayoutFileTester::getTestFilePath());
}
