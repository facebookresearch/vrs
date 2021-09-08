// Facebook Technologies, LLC Proprietary and Confidential.

#include <gtest/gtest.h>

#define DEFAULT_LOG_CHANNEL "xprsTest"
#include <logging/Log.h>

#include <TestDataDir/TestDataDir.h>

#include <vrs/RecordFileWriter.h>
#include <vrs/os/Utils.h>
#include <vrs/utils/CopyRecords.h>
#include <vrs/utils/DecoderFactory.h>
#include <vrs/utils/FilteredVRSFileReader.h>
#include <vrs/utils/PixelFrame.h>
#include <vrs/utils/VideoFrameHandler.h>
#include <vrs/utils/xprs/XprsDecoder.h>
#include <vrs/utils/xprs/XprsEncoder.h>
#include <vrs/utils/xprs/XprsManager.h>

using namespace std;
using namespace vrs;

struct xprsTest : testing::Test {
  static void SetUpTestCase() {
    vrs::utils::DecoderFactory::get().registerDecoderMaker(vrs::vxprs::xprsDecoderMaker);
  }
};

class ImageStreamPlayer : public RecordFormatStreamPlayer {
 public:
  ImageStreamPlayer(const ImageContentBlockSpec& sp) : spec{sp} {}
  bool onDataLayoutRead(const CurrentRecord&, size_t, DataLayout&) override {
    // nothing useful to check...
    return true;
  }
  bool onImageRead(const CurrentRecord& record, size_t, const ContentBlock& contentBlock) override {
    EXPECT_EQ(contentBlock.image().getImageFormat(), ImageFormat::VIDEO);
    frameCounter++;
    int decodeStatus = videoFrameHandler.tryToDecodeFrame(pixelFrame, record.reader, contentBlock);
    EXPECT_EQ(decodeStatus, 0);
    if (decodeStatus == 0) {
      EXPECT_TRUE(pixelFrame.hasSamePixels(spec));
      // XR_LOGI("Format: {} vs {}", pixelFrame.getSpec().asString(), spec.asString());
      videoFrameCounter++;
      return true;
    }
    return false;
  }

  uint32_t videoFrameCounter = 0;
  uint32_t frameCounter = 0;

  vrs::utils::VideoFrameHandler videoFrameHandler;
  vrs::utils::PixelFrame pixelFrame;
  ImageContentBlockSpec spec;
};

TEST_F(xprsTest, grey8EncodeTest) {
  string kSourcePath =
      string(coretech::getTestDataDir()) + "/VRS_Files/ConstellationTelemetryMinimalSlam.vrs";
  string kDestPath = os::getTempFolder() + "ConstellationTelemetryMinimalSlamEncoded.vrs";
  utils::FilteredVRSFileReader filteredReader(kSourcePath);
  ASSERT_EQ(filteredReader.openFile(), 0);
  RecordFileWriter writer;

  utils::CopyOptions options(false);
  vxprs::EncoderOptions encoderOptions;

  EXPECT_EQ(
      utils::copyRecords(
          filteredReader, kDestPath, options, nullptr, vxprs::makeStreamFilter(encoderOptions)),
      0);
  int64_t destSize = os::getFileSize(kDestPath);

  // Naive validation...
  EXPECT_GT(destSize, 700 * 1024);
  EXPECT_LT(destSize, 900 * 1024);

  RecordFileReader reader;
  ASSERT_EQ(reader.openFile(kDestPath), 0);

  ImageContentBlockSpec spec(PixelFormat::GREY8, 640, 480);
  vector<unique_ptr<ImageStreamPlayer>> streamPlayers;
  for (auto id : reader.getStreams()) {
    if (id.getTypeId() == RecordableTypeId::SlamCameraData ||
        id.getTypeId() == RecordableTypeId::ConstellationCameraData) {
      streamPlayers.emplace_back(make_unique<ImageStreamPlayer>(spec));
      reader.setStreamPlayer(id, streamPlayers.back().get());
    }
  }
  EXPECT_EQ(streamPlayers.size(), 8);
  EXPECT_EQ(reader.readAllRecords(), 0);
  for (auto& player : streamPlayers) {
    EXPECT_EQ(player->videoFrameCounter, 12);
    EXPECT_EQ(player->frameCounter, 12);
  }
}

TEST_F(xprsTest, raw10CopyEncodeTest) {
  string kSourcePath = string(coretech::getTestDataDir()) + "/VRS_Files/arcata_raw10.vrs";
  string kDestPath = os::getTempFolder() + "arcata_raw10.vrs";
  utils::FilteredVRSFileReader filteredReader(kSourcePath);
  ASSERT_EQ(filteredReader.openFile(), 0);
  RecordFileWriter writer;

  utils::CopyOptions options(false);
  vxprs::EncoderOptions encoderOptions;

  EXPECT_EQ(
      utils::copyRecords(
          filteredReader, kDestPath, options, nullptr, vxprs::makeStreamFilter(encoderOptions)),
      0);
  int64_t destSize = os::getFileSize(kDestPath);

  // Naive validation...
  EXPECT_GT(destSize, 400 * 1024);
  EXPECT_LT(destSize, 600 * 1024);

  RecordFileReader reader;
  ASSERT_EQ(reader.openFile(kDestPath), 0);

  // RAW10 images are encoded & decoded as GREY10!
  ImageContentBlockSpec spec(PixelFormat::GREY10, 1280, 1024); // first stream, large images
  vector<unique_ptr<ImageStreamPlayer>> streamPlayers;
  for (auto id : reader.getStreams()) {
    if (id.getTypeId() == RecordableTypeId::DeviceIndependentMonochrome10BitImage) {
      streamPlayers.emplace_back(make_unique<ImageStreamPlayer>(spec));
      reader.setStreamPlayer(id, streamPlayers.back().get());
      spec = {PixelFormat::GREY10, 640, 480}; // next stream, small images
    }
  }
  EXPECT_EQ(streamPlayers.size(), 2);
  EXPECT_EQ(reader.readAllRecords(), 0);
  for (auto& player : streamPlayers) {
    EXPECT_EQ(player->videoFrameCounter, 3);
    EXPECT_EQ(player->frameCounter, 3);
  }
}

TEST_F(xprsTest, rgb8EncodeTest) {
  string kSourcePath = string(coretech::getTestDataDir()) + "/VRS_Files/rgb8.vrs";
  string kDestPath = os::getTempFolder() + "rgb8.vrs";
  utils::FilteredVRSFileReader filteredReader(kSourcePath);
  ASSERT_EQ(filteredReader.openFile(), 0);
  RecordFileWriter writer;

  utils::CopyOptions options(false);
  vxprs::EncoderOptions encoderOptions;

  EXPECT_EQ(
      utils::copyRecords(
          filteredReader, kDestPath, options, nullptr, vxprs::makeStreamFilter(encoderOptions)),
      0);
  int64_t destSize = os::getFileSize(kDestPath);

  // Naive validation...
  EXPECT_GT(destSize, 85 * 1024);
  EXPECT_LT(destSize, 100 * 1024);

  RecordFileReader reader;
  ASSERT_EQ(reader.openFile(kDestPath), 0);

  ImageContentBlockSpec spec(PixelFormat::RGB8, 1224, 1024);
  vector<unique_ptr<ImageStreamPlayer>> streamPlayers;
  for (auto id : reader.getStreams()) {
    if (id.getTypeId() == RecordableTypeId::EyeTrackingCamera) {
      streamPlayers.emplace_back(make_unique<ImageStreamPlayer>(spec));
      reader.setStreamPlayer(id, streamPlayers.back().get());
    }
  }
  EXPECT_EQ(streamPlayers.size(), 1);
  EXPECT_EQ(reader.readAllRecords(), 0);
  for (auto& player : streamPlayers) {
    EXPECT_EQ(player->videoFrameCounter, 3);
    EXPECT_EQ(player->frameCounter, 3);
  }
}
