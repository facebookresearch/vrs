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

#include <chrono>

#include <gtest/gtest.h>

#define DEFAULT_LOG_CHANNEL "ImageStreamTest"
#include <logging/Log.h>
#include <logging/Verify.h>

#include <vrs/DataLayout.h>
#include <vrs/DataLayoutConventions.h>
#include <vrs/DataPieces.h>
#include <vrs/RecordFileReader.h>
#include <vrs/RecordFileWriter.h>
#include <vrs/RecordFormatStreamPlayer.h>
#include <vrs/Recordable.h>
#include <vrs/os/Utils.h>
#include <vrs/utils/PixelFrame.h>

#define ASSERT_OK(call) ASSERT_EQ((call), 0)
#define EXPECT_OK(call) EXPECT_EQ((call), 0)

namespace {

using namespace std;
using namespace vrs;
using namespace vrs::utils;
using namespace vrs::datalayout_conventions;

inline double getTimestampSec() {
  using namespace std::chrono;
  return duration_cast<duration<double>>(steady_clock::now().time_since_epoch()).count();
}

inline string makeFlavor() {
  static int flavorIndex = 0;
  return "test_image_stream/" + std::to_string(++flavorIndex);
}

/**
 * @brief A template class for testing image streams.
 *
 * TestImageStream is a template class that extends the Recordable class to facilitate
 * testing of image streams. It supports different image formats and configurations
 * through the use of template parameters. The class manages the creation of configuration
 * and data records, and provides a player for reading image data.
 * The goal is to make it easy to generate many different image formats and configurations, proving
 * that vrs properly saves the image format definitions and read them back.
 *
 * @tparam IMAGE_TEST A struct that defines the image test configuration and data record
 *         formats. It must provide ConfigRecord and DataRecord classes with specific
 *         methods and properties.
 */
template <typename IMAGE_TEST>
class TestImageStream : public Recordable {
  static const uint32_t kConfigurationRecordFormatVersion = 1;
  static const uint32_t kDataRecordFormatVersion = 1;

  class Player : public RecordFormatStreamPlayer {
   public:
    explicit Player(typename IMAGE_TEST::DataRecord& dataRecord) : dataRecord_{dataRecord} {}
    bool onImageRead(const CurrentRecord& record, size_t idx, const ContentBlock& cb) override {
      XR_LOGI("onImageRead: {} - {}", record.streamId.getNumericName(), cb.image().asString());
      const auto& imageSpec = cb.image();
      if (imageSpec.getImageFormat() == ImageFormat::CUSTOM_CODEC ||
          imageSpec.getImageFormat() == ImageFormat::VIDEO) {
        EXPECT_FALSE(imageSpec.getCodecName().empty());
      }
      if (imageSpec.getImageFormat() == ImageFormat::VIDEO) {
        EXPECT_GT(imageSpec.getWidth(), 0);
        EXPECT_GT(imageSpec.getHeight(), 0);
        EXPECT_NE(imageSpec.getPixelFormat(), PixelFormat::UNDEFINED);
      }
      return dataRecord_.onImageRead(record, idx, cb);
    }
    bool onUnsupportedBlock(const CurrentRecord& record, size_t idx, const ContentBlock& cb)
        override {
      ADD_FAILURE() << "Unsupported block: " << cb.asString() << " in stream "
                    << record.streamId.getNumericName();
      return false;
    }

   private:
    typename IMAGE_TEST::DataRecord& dataRecord_;
  };

 public:
  TestImageStream()
      : Recordable(RecordableTypeId::ImageStream, makeFlavor()), player_{dataRecord_} {
    setCompression(CompressionPreset::ZstdMedium);
    addRecordFormat(
        Record::Type::CONFIGURATION,
        kConfigurationRecordFormatVersion,
        configRecord_.getContentBlock(),
        {&configRecord_});
    addRecordFormat(
        Record::Type::DATA,
        kDataRecordFormatVersion,
        dataRecord_.getContentBlock() + dataRecord_.getRecordFormatImageContentBlock(),
        {&dataRecord_});
  }
  const Record* createConfigurationRecord() override {
    configRecord_.init();
    return createRecord(
        getTimestampSec(),
        Record::Type::CONFIGURATION,
        kConfigurationRecordFormatVersion,
        DataSource(configRecord_));
  }
  const Record* createStateRecord() override {
    return createRecord(getTimestampSec(), Record::Type::STATE, 0);
  }
  void createDataRecord(uint64_t frameCount) {
    static int sFameCount = 0;
    vector<uint8_t> pixels = dataRecord_.makeImage(sFameCount++);
    createRecord(
        getTimestampSec(),
        Record::Type::DATA,
        kDataRecordFormatVersion,
        DataSource(dataRecord_, {pixels.data(), pixels.size()}));
  }
  StreamPlayer* getPlayer() {
    return &player_;
  }

 private:
  typename IMAGE_TEST::ConfigRecord configRecord_;
  typename IMAGE_TEST::DataRecord dataRecord_;
  Player player_;
};

struct ImageStreamTest : testing::Test {};

struct RawImageTest {
  class DataRecord : public AutoDataLayout {
   public:
    DataPieceValue<int64_t> counter_{"counter"};

    AutoDataLayoutEnd end;

    static ImageContentBlockSpec getRecordFormatImageContentBlock() {
      return {PixelFormat::GREY8, 640, 480};
    }
    vector<uint8_t> makeImage(int index) {
      counter_.set(index);
      vector<uint8_t> image_(640 * 480);
      for (size_t i = 0; i < image_.size(); i++) {
        image_[i] = static_cast<uint8_t>(index + i);
      }
      return image_;
    }

    static bool onImageRead(const CurrentRecord& record, size_t idx, const ContentBlock& cb) {
      EXPECT_EQ(cb, ContentBlock(getRecordFormatImageContentBlock()));
      PixelFrame frame;
      EXPECT_TRUE(frame.readFrame(record.reader, cb));
      return false;
    }
  };

  class ConfigRecord : public AutoDataLayout {
   public:
    DataPieceValue<ImageSpecType> width{kImageWidth};
    DataPieceValue<ImageSpecType> height{kImageHeight};
    DataPieceEnum<PixelFormat, ImageSpecType> pixelFormat{kImagePixelFormat};

    AutoDataLayoutEnd end;

    void init() {
      ImageContentBlockSpec spec = DataRecord::getRecordFormatImageContentBlock();
      width.set(spec.getWidth());
      height.set(spec.getHeight());
      pixelFormat.set(spec.getPixelFormat());
    }
  };
};

struct JpgImageTest {
  class DataRecord : public AutoDataLayout {
   public:
    DataPieceValue<int64_t> counter_{"counter"};

    AutoDataLayoutEnd end;

    static ImageContentBlockSpec getRecordFormatImageContentBlock() {
      return {ImageFormat::JPG};
    }
    vector<uint8_t> makeImage(int index) {
      counter_.set(index);
      vector<uint8_t> image_(10 * (index + 1));
      for (size_t i = 0; i < image_.size(); i++) {
        image_[i] = static_cast<uint8_t>(index + i);
      }
      return image_;
    }

    static bool onImageRead(const CurrentRecord& record, size_t idx, const ContentBlock& cb) {
      string foundFormat = cb.image().asString();
      string expectedFormat = "jpg"; // pixel format & image dimensions are not picked up!
      EXPECT_EQ(foundFormat, expectedFormat);
      vector<uint8_t> image(cb.getBlockSize());
      EXPECT_OK(record.reader->read(image));
      return false;
    }
  };

  class ConfigRecord : public AutoDataLayout {
   public:
    DataPieceValue<ImageSpecType> width{kImageWidth};
    DataPieceValue<ImageSpecType> height{kImageHeight};
    DataPieceEnum<PixelFormat, ImageSpecType> pixelFormat{kImagePixelFormat};
    DataPieceString codecName{kImageCodecName}; // ignored
    DataPieceValue<ImageSpecType> codecQuality{kImageCodecQuality}; // ignored

    AutoDataLayoutEnd end;

    void init() {
      width.set(640);
      height.set(480);
      pixelFormat.set(PixelFormat::GREY8);
      codecName.stage("my_jpg_codec");
      codecQuality.set(42);
    }
  };
};

struct CustomCodecImageTest {
  class DataRecord : public AutoDataLayout {
   public:
    DataPieceValue<int64_t> counter_{"counter"};

    AutoDataLayoutEnd end;

    static ImageContentBlockSpec getRecordFormatImageContentBlock() {
      return {ImageFormat::CUSTOM_CODEC, "acodec"};
    }
    vector<uint8_t> makeImage(int index) {
      counter_.set(index);
      vector<uint8_t> image_(10 * (index + 1));
      for (size_t i = 0; i < image_.size(); i++) {
        image_[i] = static_cast<uint8_t>(index + i);
      }
      return image_;
    }

    static bool onImageRead(const CurrentRecord& record, size_t idx, const ContentBlock& cb) {
      string foundFormat = cb.image().asString();
      string expectedFormat = "custom_codec/640x480/pixel=grey8/codec=acodec";
      EXPECT_EQ(foundFormat, expectedFormat);
      vector<uint8_t> image(cb.getBlockSize());
      EXPECT_OK(record.reader->read(image));
      return false;
    }
  };

  class ConfigRecord : public AutoDataLayout {
   public:
    DataPieceValue<ImageSpecType> width{kImageWidth};
    DataPieceValue<ImageSpecType> height{kImageHeight};
    DataPieceEnum<PixelFormat, ImageSpecType> pixelFormat{kImagePixelFormat};
    DataPieceString codecName{kImageCodecName};
    DataPieceValue<ImageSpecType> codecQuality{kImageCodecQuality};

    AutoDataLayoutEnd end;

    void init() {
      width.set(640);
      height.set(480);
      pixelFormat.set(PixelFormat::GREY8);
      codecName.stage("");
      codecQuality.set(255);
    }
  };
};

struct CustomCodecImageTest2 {
  class DataRecord : public AutoDataLayout {
   public:
    DataPieceValue<int64_t> counter_{"counter"};

    AutoDataLayoutEnd end;

    static ImageContentBlockSpec getImageSpec() {
      return {ImageFormat::CUSTOM_CODEC, "my_custom_codec", 42, PixelFormat::GREY8, 640, 480};
    }
    static ImageContentBlockSpec getRecordFormatImageContentBlock() {
      return {getImageSpec().getImageFormat()};
    }
    vector<uint8_t> makeImage(int index) {
      counter_.set(index);
      vector<uint8_t> image_(10 * (index + 1));
      for (size_t i = 0; i < image_.size(); i++) {
        image_[i] = static_cast<uint8_t>(index + i);
      }
      return image_;
    }

    static bool onImageRead(const CurrentRecord& record, size_t idx, const ContentBlock& cb) {
      string foundFormat = cb.image().asString();
      string expectedFormat = getImageSpec().asString();
      EXPECT_EQ(foundFormat, expectedFormat);
      vector<uint8_t> image(cb.getBlockSize());
      EXPECT_OK(record.reader->read(image));
      return false;
    }
  };

  class ConfigRecord : public AutoDataLayout {
   public:
    DataPieceValue<ImageSpecType> width{kImageWidth};
    DataPieceValue<ImageSpecType> height{kImageHeight};
    DataPieceEnum<PixelFormat, ImageSpecType> pixelFormat{kImagePixelFormat};
    DataPieceString codecName{kImageCodecName};
    DataPieceValue<ImageSpecType> codecQuality{kImageCodecQuality};

    AutoDataLayoutEnd end;

    void init() {
      ImageContentBlockSpec spec = DataRecord::getImageSpec();
      width.set(spec.getWidth());
      height.set(spec.getHeight());
      pixelFormat.set(spec.getPixelFormat());
      codecName.stage(spec.getCodecName());
      codecQuality.set(spec.getCodecQuality());
    }
  };
};

struct CustomCodecImageTest3 {
  class DataRecord : public AutoDataLayout {
   public:
    DataPieceValue<int64_t> counter_{"counter"};

    AutoDataLayoutEnd end;

    static ImageContentBlockSpec getImageSpec() {
      return {ImageFormat::CUSTOM_CODEC, "my_custom_codec", 42, PixelFormat::GREY8, 640, 480};
    }
    static ImageContentBlockSpec getRecordFormatImageContentBlock() {
      ImageContentBlockSpec spec = DataRecord::getImageSpec();
      return {spec.getImageFormat(), spec.getCodecName()};
    }
    vector<uint8_t> makeImage(int index) {
      counter_.set(index);
      vector<uint8_t> image_(10 * (index + 1));
      for (size_t i = 0; i < image_.size(); i++) {
        image_[i] = static_cast<uint8_t>(index + i);
      }
      return image_;
    }

    static bool onImageRead(const CurrentRecord& record, size_t idx, const ContentBlock& cb) {
      string foundFormat = cb.image().asString();
      string expectedFormat = getImageSpec().asString();
      EXPECT_EQ(foundFormat, expectedFormat);
      vector<uint8_t> image(cb.getBlockSize());
      EXPECT_OK(record.reader->read(image));
      return false;
    }
  };

  class ConfigRecord : public AutoDataLayout {
   public:
    DataPieceValue<ImageSpecType> width{kImageWidth};
    DataPieceValue<ImageSpecType> height{kImageHeight};
    DataPieceEnum<PixelFormat, ImageSpecType> pixelFormat{kImagePixelFormat};
    DataPieceString codecName{kImageCodecName};
    DataPieceValue<ImageSpecType> codecQuality{kImageCodecQuality};

    AutoDataLayoutEnd end;

    void init() {
      ImageContentBlockSpec spec = DataRecord::getImageSpec();
      width.set(spec.getWidth());
      height.set(spec.getHeight());
      pixelFormat.set(spec.getPixelFormat());
      codecName.stage(spec.getCodecName());
      codecQuality.set(spec.getCodecQuality());
    }
  };
};

struct CustomCodecImageTest4 {
  // the codec name is in the data record
  class DataRecord : public AutoDataLayout {
   public:
    DataPieceValue<int64_t> counter_{"counter"};
    DataPieceString codecName{kImageCodecName};

    AutoDataLayoutEnd end;

    static ImageContentBlockSpec getImageSpec() {
      return {ImageFormat::CUSTOM_CODEC, "my_custom_codec"};
    }
    static ImageContentBlockSpec getRecordFormatImageContentBlock() {
      return {ImageFormat::CUSTOM_CODEC};
    }
    vector<uint8_t> makeImage(int index) {
      counter_.set(index);
      ImageContentBlockSpec spec = DataRecord::getImageSpec();
      codecName.stage(spec.getCodecName());

      vector<uint8_t> image_(10 * (index + 1));
      for (size_t i = 0; i < image_.size(); i++) {
        image_[i] = static_cast<uint8_t>(index + i);
      }
      return image_;
    }

    static bool onImageRead(const CurrentRecord& record, size_t idx, const ContentBlock& cb) {
      string foundFormat = cb.image().asString();
      string expectedFormat = getImageSpec().asString();
      EXPECT_EQ(foundFormat, expectedFormat);
      vector<uint8_t> image(cb.getBlockSize());
      EXPECT_OK(record.reader->read(image));
      return false;
    }
  };

  class ConfigRecord : public AutoDataLayout {
   public:
    AutoDataLayoutEnd end;
    void init() {}
  };
};

struct CustomCodecImageTest5 {
  // the codec name is in the record format definition, nothing elsewhere
  class DataRecord : public AutoDataLayout {
   public:
    DataPieceValue<int64_t> counter_{"counter"};
    AutoDataLayoutEnd end;

    static ImageContentBlockSpec getRecordFormatImageContentBlock() {
      return {ImageFormat::CUSTOM_CODEC, "my_custom_codec"};
    }
    // NOLINTNEXTLINE(clang-diagnostic-unused-member-function)
    vector<uint8_t> makeImage(int index) {
      counter_.set(index);
      ImageContentBlockSpec spec = DataRecord::getRecordFormatImageContentBlock();

      vector<uint8_t> image_(10 * (index + 1));
      for (size_t i = 0; i < image_.size(); i++) {
        image_[i] = static_cast<uint8_t>(index + i);
      }
      return image_;
    }

    // NOLINTNEXTLINE(clang-diagnostic-unused-member-function)
    static bool onImageRead(const CurrentRecord& record, size_t idx, const ContentBlock& cb) {
      string foundFormat = cb.image().asString();
      string expectedFormat = getRecordFormatImageContentBlock().asString();
      EXPECT_EQ(foundFormat, expectedFormat);
      vector<uint8_t> image(cb.getBlockSize());
      EXPECT_OK(record.reader->read(image));
      return false;
    }
  };

  class ConfigRecord : public AutoDataLayout {
   public:
    AutoDataLayoutEnd end;
    // NOLINTNEXTLINE(clang-diagnostic-unused-member-function)
    void init() {}
  };
};

#define TEST_FORMAT(format)                        \
  TestImageStream<format> imageStream_##format;    \
  fileWriter.addRecordable(&imageStream_##format); \
  players[imageStream_##format.getStreamFlavor()] = imageStream_##format.getPlayer();

TEST_F(ImageStreamTest, testBlockFormat) {
  string path = os::getUniquePath(os::getTempFolder() + "testBlockFormat");

  map<string, StreamPlayer*> players;
  RecordFileWriter fileWriter;

  TEST_FORMAT(RawImageTest);
  TEST_FORMAT(JpgImageTest);
  TEST_FORMAT(CustomCodecImageTest);
  TEST_FORMAT(CustomCodecImageTest2);
  TEST_FORMAT(CustomCodecImageTest3);
  TEST_FORMAT(CustomCodecImageTest4);

  ASSERT_OK(fileWriter.createFileAsync(path));

  for (int k = 0; k < 2; k++) {
    imageStream_RawImageTest.createDataRecord(k);
    imageStream_JpgImageTest.createDataRecord(k);
    imageStream_CustomCodecImageTest.createDataRecord(k);
    imageStream_CustomCodecImageTest2.createDataRecord(k);
    imageStream_CustomCodecImageTest3.createDataRecord(k);
    imageStream_CustomCodecImageTest4.createDataRecord(k);
  }

  ASSERT_OK(fileWriter.waitForFileClosed());

  RecordFileReader fileReader;
  ASSERT_OK(fileReader.openFile(path));
  ASSERT_EQ(fileReader.getStreams().size(), fileWriter.getRecordables().size());
  ASSERT_EQ(fileReader.getStreams().size(), players.size());
  for (StreamId id : fileReader.getStreams()) {
    auto iter = players.find(fileReader.getFlavor(id));
    ASSERT_NE(iter, players.end());
    fileReader.setStreamPlayer(id, iter->second);
  }
  fileReader.readAllRecords();
}

} // namespace
