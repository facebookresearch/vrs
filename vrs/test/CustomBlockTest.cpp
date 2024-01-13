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

#include <gtest/gtest.h>

#include <vrs/DataLayoutConventions.h>
#include <vrs/RecordFileReader.h>
#include <vrs/RecordFileWriter.h>
#include <vrs/RecordFormatStreamPlayer.h>
#include <vrs/os/Utils.h>

using namespace std;
using namespace vrs;

namespace {

const uint32_t kConfigurationVersion = 1;
const uint32_t kDataVersion = 1;
const uint32_t kStateVersion = 1;

const uint32_t kFrameWidth = 640;
const uint32_t kFrameHeight = 480;
const uint32_t kPixelByteSize = 1;

const size_t kConfigCustomBlockSize0 = 190;
const size_t kStateCustomBlockSize1 = 25;
const size_t kStateCustomBlockSize3 = 39;
const size_t kDataCustomBlockSize1 = 37;
const size_t kDataCustomBlockSize3 = 125;
const size_t kDataCustomBlockSize4 = 9;

const double kStartTimestamp = 1543864285;

const size_t kRecordSetCount = 3;

using namespace datalayout_conventions;

// Generate/check a custom block of data with a (very) pseudo random pattern
struct CustomBlob {
  explicit CustomBlob(size_t size) {
    blob.resize(size);
    for (size_t k = 0; k < size; k++) {
      blob[k] = dataAt(k, size);
    }
  }
  static void checkData(vector<uint8_t>& blob) {
    for (size_t k = 0; k < blob.size(); k++) {
      EXPECT_EQ(blob[k], dataAt(k, blob.size()));
    }
  }
  // (very) pseudo random pattern
  static uint8_t dataAt(size_t k, size_t maxSize) {
    return static_cast<uint8_t>(k ^ maxSize);
  }
  vector<uint8_t> blob;
};

// Datalayout giving specs of both an image block and a custom block size
// Attention! The custom block must be immediately after the metadata block
class ImageAndCustomBlockMetadata : public AutoDataLayout {
 public:
  ImageAndCustomBlockMetadata() {
    someData.set(kStartTimestamp);
    width.set(kFrameWidth);
    height.set(kFrameHeight);
    pixelFormat.set(PixelFormat::GREY8);
    someString.stage("hello");
  }
  void checkData() {
    EXPECT_NEAR(someData.get(), kStartTimestamp, 0.0000001);
    EXPECT_EQ(width.get(), kFrameWidth);
    EXPECT_EQ(height.get(), kFrameHeight);
    EXPECT_EQ(pixelFormat.get(), PixelFormat::GREY8);
    EXPECT_STREQ(someString.get().c_str(), "hello");
  }
  // The order of the fields doesn't matter, only their existence
  DataPieceValue<double> someData{"some_data"};
  DataPieceValue<ImageSpecType> width{kImageWidth};
  DataPieceValue<ImageSpecType> height{kImageHeight};
  DataPieceEnum<PixelFormat, ImageSpecType> pixelFormat{kImagePixelFormat};
  DataPieceValue<ContentBlockSizeType> nextContentBlockSize{kNextContentBlockSize};
  DataPieceString someString{"some_string"};

  AutoDataLayoutEnd endLayout;
};

// Datalayout with a custom block size
class CustomBlockSizeMetadata : public AutoDataLayout {
 public:
  DataPieceValue<ContentBlockSizeType> nextContentBlockSize{kNextContentBlockSize};
  DataPieceValue<float> someData{"some_data"};
  DataPieceArray<Point2Df> sameArray{"some_array", 5};

  AutoDataLayoutEnd endLayout;
};

// All the same as DataSource, except that we write the chunks in a different order
class DataRecordDataSource : public DataSource {
 public:
  DataRecordDataSource(
      DataLayout& dl1,
      const DataSourceChunk& cb1,
      DataLayout& dl2,
      const DataSourceChunk& cb2,
      const DataSourceChunk& cb3)
      : DataSource(dl1, dl2, cb1, cb2, cb3) {}
  void copyTo(uint8_t* buffer) const override {
    dataLayout1_.fillAndAdvanceBuffer(buffer);
    if (chunk1_.size() > 0) {
      chunk1_.fillAndAdvanceBuffer(buffer);
    }
    dataLayout2_.fillAndAdvanceBuffer(buffer);
    if (chunk2_.size() > 0) {
      chunk2_.fillAndAdvanceBuffer(buffer);
    }
    if (chunk3_.size() > 0) {
      chunk3_.fillAndAdvanceBuffer(buffer);
    }
  }
};

// Stream demonstrating different custom content blocks, which size are determined in diffent ways.
// Config, state & data records all work the same: here, it just makes things a little simpler.
class CustomStreams : public Recordable {
 public:
  CustomStreams() : Recordable(RecordableTypeId::UnitTest1) {
    // Record with 1 custom content block, which size is determined using the size of the record
    addRecordFormat(
        Record::Type::CONFIGURATION,
        kConfigurationVersion,
        ContentBlock(ContentType::CUSTOM), // Single custom block, no size needed
        {});
    // Record with a datalayout containing the next content block's size and the spec of an image,
    // then a custom block size (which size was in the datalayout just before -- no choice),
    // then an image which spec (and size) were described in the datalayout,
    // then a custom block, which size is determined by using what's left to read in the record.
    addRecordFormat(
        Record::Type::STATE,
        kStateVersion,
        imageAndCustomBlockMetadata_.getContentBlock() + ContentBlock(ContentType::CUSTOM) +
            ContentBlock(ImageFormat::RAW) +
            ContentBlock(ContentType::CUSTOM), // datalayout + custom + image + custom
        {&imageAndCustomBlockMetadata_});
    // Record with a datalayout with the size of the next content block,
    // then a custom block which size was in the datalayout just before,
    // then another datalayout with the size of the next content block,
    // then a custom block which size was in the datalayout just before,
    // then a custom block, which size is determined by using what's left to read in the record...
    addRecordFormat(
        Record::Type::DATA,
        kDataVersion,
        customBlockSizeMetadata_.getContentBlock() + ContentBlock(ContentType::CUSTOM) +
            customBlockSizeMetadata2_.getContentBlock() + ContentBlock(ContentType::CUSTOM) +
            ContentBlock(ContentType::CUSTOM), // metadata + custom + metadata + custom + custom
        {&customBlockSizeMetadata_, nullptr, &customBlockSizeMetadata2_});
  }

  const Record* createConfigurationRecord() override {
    CustomBlob config(kConfigCustomBlockSize0);
    return createRecord(
        kStartTimestamp,
        Record::Type::CONFIGURATION,
        kConfigurationVersion,
        DataSource(config.blob));
  }

  const Record* createStateRecord() override {
    CustomBlob custom1(kStateCustomBlockSize1);
    CustomBlob image2(kFrameWidth * kFrameHeight * kPixelByteSize);
    CustomBlob custom3(kStateCustomBlockSize3);
    imageAndCustomBlockMetadata_.nextContentBlockSize.set(kStateCustomBlockSize1);
    imageAndCustomBlockMetadata_.width.set(kFrameWidth);
    imageAndCustomBlockMetadata_.height.set(kFrameHeight);
    imageAndCustomBlockMetadata_.pixelFormat.set(PixelFormat::GREY8);
    return createRecord(
        kStartTimestamp + 1, // timestamps don't matter in this test
        Record::Type::STATE,
        kStateVersion,
        DataSource(imageAndCustomBlockMetadata_, custom1.blob, image2.blob, custom3.blob));
  }

  const Record* createDataRecord() {
    customBlockSizeMetadata_.nextContentBlockSize.set(kDataCustomBlockSize1);
    CustomBlob custom1(kDataCustomBlockSize1);
    customBlockSizeMetadata2_.nextContentBlockSize.set(kDataCustomBlockSize3);
    CustomBlob custom3(kDataCustomBlockSize3);
    CustomBlob custom4(kDataCustomBlockSize4);
    return createRecord(
        kStartTimestamp + 2, // timestamps don't matter in this test
        Record::Type::DATA,
        kDataVersion,
        DataRecordDataSource(
            customBlockSizeMetadata_,
            custom1.blob,
            customBlockSizeMetadata2_,
            custom3.blob,
            custom4.blob));
  }

 private:
  ImageAndCustomBlockMetadata imageAndCustomBlockMetadata_;
  CustomBlockSizeMetadata customBlockSizeMetadata_;
  CustomBlockSizeMetadata customBlockSizeMetadata2_;
};

class CustomStreamPlayer : public RecordFormatStreamPlayer {
 public:
  bool onDataLayoutRead(const CurrentRecord& record, size_t blockIndex, DataLayout& dl) override {
    if (record.recordType == Record::Type::CONFIGURATION) {
      GTEST_NONFATAL_FAILURE_("No datalayout expected for config records");
      return false;
    } else if (record.recordType == Record::Type::STATE) {
      EXPECT_EQ(blockIndex, 0);
      ImageAndCustomBlockMetadata& data =
          getExpectedLayout<ImageAndCustomBlockMetadata>(dl, blockIndex);
      data.checkData();
    } else if (record.recordType == Record::Type::DATA) {
      EXPECT_TRUE(blockIndex == 0 || blockIndex == 2);
    }
    return true;
  }
  bool onImageRead(const CurrentRecord& record, size_t blockIndex, const ContentBlock& contentBlock)
      override {
    EXPECT_EQ(record.recordType, Record::Type::STATE);
    EXPECT_EQ(blockIndex, 2);
    size_t size = contentBlock.getBlockSize();
    if (size == ContentBlock::kSizeUnknown) {
      GTEST_NONFATAL_FAILURE_("Unknown image size!");
      return false;
    }
    vector<uint8_t> image(size);
    EXPECT_EQ(record.reader->read(image), 0);
    CustomBlob::checkData(image);
    stateImageCount++;
    return true;
  }
  bool onCustomBlockRead(
      const CurrentRecord& record,
      size_t blockIndex,
      const ContentBlock& contentBlock) override {
    size_t size = contentBlock.getBlockSize();
    if (size == ContentBlock::kSizeUnknown) {
      GTEST_NONFATAL_FAILURE_("Unknown custom block size!");
      return false;
    }
    vector<uint8_t> customData(size);
    EXPECT_EQ(record.reader->read(customData), 0);
    CustomBlob::checkData(customData);
    if (record.recordType == Record::Type::CONFIGURATION) {
      EXPECT_EQ(blockIndex, 0);
      configCustom0Count++;
    } else if (record.recordType == Record::Type::STATE) {
      if (blockIndex == 1) {
        EXPECT_EQ(size, kStateCustomBlockSize1);
        stateCustom1Count++;
      } else if (blockIndex == 3) {
        EXPECT_EQ(size, kStateCustomBlockSize3);
        stateCustom3Count++;
      } else {
        GTEST_NONFATAL_FAILURE_("Unexpected custom state block index");
        return false;
      }
    } else if (record.recordType == Record::Type::DATA) {
      if (blockIndex == 1) {
        EXPECT_EQ(size, kDataCustomBlockSize1);
        dataCustom1Count++;
      } else if (blockIndex == 3) {
        EXPECT_EQ(size, kDataCustomBlockSize3);
        dataCustom3Count++;
      } else if (blockIndex == 4) {
        EXPECT_EQ(size, kDataCustomBlockSize4);
        dataCustom4Count++;
      } else {
        GTEST_NONFATAL_FAILURE_("Unexpected custom data block index");
        return false;
      }
    }
    return true;
  }

  uint64_t configCustom0Count = 0;
  uint64_t stateImageCount = 0;
  uint64_t stateCustom1Count = 0;
  uint64_t stateCustom3Count = 0;
  uint64_t dataCustom1Count = 0;
  uint64_t dataCustom3Count = 0;
  uint64_t dataCustom4Count = 0;
};

struct CustomBlockTest : testing::Test {
  static int createFileAtOnce(const string& filePath) {
    RecordFileWriter fileWriter;
    fileWriter.setTag("purpose", "this is a test");
    CustomStreams imageStream;
    fileWriter.addRecordable(&imageStream);
    for (size_t s = 0; s < kRecordSetCount; s++) {
      imageStream.createConfigurationRecord();
      imageStream.createStateRecord();
      imageStream.createDataRecord();
    }
    EXPECT_EQ(fileWriter.writeToFile(filePath), 0);
    return 0;
  }

  static void checkFileHandler(const string& filePath) {
    // Verify that the file was created, and looks like we think it should
    RecordFileReader reader;
    int openFileStatus = reader.openFile(filePath);
    EXPECT_EQ(openFileStatus, 0);
    if (openFileStatus != 0) {
      return;
    }

    EXPECT_EQ(reader.getStreams().size(), 1);

    CustomStreamPlayer streamPlayer;
    reader.setStreamPlayer(*reader.getStreams().begin(), &streamPlayer);
    reader.readAllRecords();

    size_t actualRecordCount = reader.getIndex().size();
    EXPECT_EQ(actualRecordCount, 3 * kRecordSetCount);

    EXPECT_EQ(streamPlayer.configCustom0Count, kRecordSetCount);

    EXPECT_EQ(streamPlayer.stateImageCount, kRecordSetCount);
    EXPECT_EQ(streamPlayer.stateCustom1Count, kRecordSetCount);
    EXPECT_EQ(streamPlayer.stateCustom3Count, kRecordSetCount);

    EXPECT_EQ(streamPlayer.dataCustom1Count, kRecordSetCount);
    EXPECT_EQ(streamPlayer.dataCustom3Count, kRecordSetCount);
    EXPECT_EQ(streamPlayer.dataCustom4Count, kRecordSetCount);
    reader.closeFile();
  }
};

} // namespace

TEST_F(CustomBlockTest, simpleCreation) {
  const string testPath = os::getTempFolder() + "CustomBlockTest_simpleCreation.vrs";
  ASSERT_EQ(createFileAtOnce(testPath), 0);

  checkFileHandler(testPath);

  os::remove(testPath);
}

TEST_F(CustomBlockTest, DataSourceChunkTest) {
  int anint = 0;
  DataSourceChunk intdl(anint);
  EXPECT_EQ(intdl.size(), sizeof(int));

  const size_t vSize = 123;
  vector<char> v(vSize);
  DataSourceChunk vds(v);
  EXPECT_EQ(vds.size(), vSize);

  // If making changes to DataSourceChunk, make sure this doesn't compile...
#if 0
  int* pint;
  DataSourceChunk pintdl(pint);
#endif
#if 0
  DataSourceChunk dsc(vds);

  class SampleDL : public AutoDataLayout {
    DataPieceValue<uint8_t> int8;
    AutoDataLayoutEnd end;
  };
  SampleDL dl;
  DataSourceChunk slds(dl);
#endif
}
