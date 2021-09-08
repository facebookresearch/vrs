// Facebook Technologies, LLC Proprietary and Confidential.

#include <cstdio>

#include <gtest/gtest.h> // IWYU pragma: keep

#include <vrs/DataLayoutConventions.h>
#include <vrs/RecordFileReader.h>
#include <vrs/RecordFileWriter.h>
#include <vrs/RecordFormatStreamPlayer.h>
#include <vrs/os/Utils.h>
#include <vrs/utils/DecoderFactory.h>
#include <vrs/utils/PixelFrame.h>
#include <vrs/utils/VideoFrameHandler.h>
#include <vrs/utils/VideoRecordFormatStreamPlayer.h>
#include <vrs/utils/test/chess_codec/ChessCodec.h>

// This test demonstrates how to create a VRS file with a video image stream, using RecordFormat &
// DataLayout.

using namespace vrs;
using namespace vrs::DataLayoutConventions;

namespace {

const uint32_t kChessSquareSideSize = 20; // arbitrary pixel count
const uint32_t kChessSquareSideCount = 8; // a real chess board would be 8 squares by side
const uint8_t kBoardBlackValue = 1; // avoid 0, which is a common default value...
const uint8_t kBoardWhiteValue = 254; // avoid 255, to make white more "special"...
const uint8_t kBoardInitValue = 128; // arbitrary

const uint32_t kFrameWidth = kChessSquareSideSize * kChessSquareSideCount;
const uint32_t kFrameHeight = kChessSquareSideSize * kChessSquareSideCount;

const uint32_t kFrameRate = 50; // Hz
// We'll generate one I-frame to reset the board then enough P-frames to set all the squares
const uint32_t kKeyFrameRate = 1 + kChessSquareSideCount * kChessSquareSideCount;
const uint32_t kFrameCount = kKeyFrameRate * 6;
const double kInterFrameDelay = 1.0 / kFrameRate;

const uint32_t kConfigurationVersion = 1;
const uint32_t kDataVersion = 1;

const double kStartTimestamp = 1000;

double getFrameTimestamp(uint32_t frameNumber) {
  return kStartTimestamp + frameNumber * kInterFrameDelay;
}

class ImageStreamConfiguration : public AutoDataLayout {
 public:
  // Define the image format following DataLayout conventions
  DataPieceValue<ImageSpecType> width{kImageWidth};
  DataPieceValue<ImageSpecType> height{kImageHeight};
  DataPieceEnum<PixelFormat, ImageSpecType> pixelFormat{kImagePixelFormat};

  DataPieceString codecName{kImageCodecName};

  AutoDataLayoutEnd endLayout;
};

class ImageStreamMetaData : public AutoDataLayout {
 public:
  // Tell how this image fits in terms of I-frames (keyframes) & P-frames (intermediate frames)
  DataPieceValue<ImageSpecType> keyFrameIndex{kImageKeyFrameIndex};
  DataPieceValue<double> keyFrameTimestamp{kImageKeyFrameTimeStamp};

  AutoDataLayoutEnd endLayout;
};

class ImageStream : public Recordable {
 public:
  ImageStream() : Recordable(RecordableTypeId::ImageStream) {
    setCompression(CompressionPreset::ZstdFast);

    // Tell how the records look like,
    // so that generic tools like VRSplayer can read the file as if it was a video file!
    addRecordFormat(
        Record::Type::CONFIGURATION,
        kConfigurationVersion,
        config_.getContentBlock(), // only metadata
        {&config_});
    addRecordFormat(
        Record::Type::DATA,
        kDataVersion,
        metadata_.getContentBlock() + ContentBlock(ImageFormat::VIDEO), // metadata + image
        {&metadata_});
    makeSpiral(trajectories_[4]);
  }

  const Record* createConfigurationRecord() override {
    // record the actual image format
    config_.width.set(kFrameWidth);
    config_.height.set(kFrameHeight);
    config_.pixelFormat.set(PixelFormat::GREY8);
    config_.codecName.stage(vrs::utils::test::kChessCodecName);

    // set some additional config info
    return createRecord(
        kStartTimestamp, Record::Type::CONFIGURATION, kConfigurationVersion, DataSource(config_));
  }

  const Record* createStateRecord() override {
    // Not used, but we still need to create a record
    return createRecord(kStartTimestamp, Record::Type::STATE, 0);
  }

  const Record* createFrame(uint32_t frameNumber) {
    uint32_t keyFrameIndex = frameNumber % kKeyFrameRate;
    metadata_.keyFrameIndex.set(keyFrameIndex);
    metadata_.keyFrameTimestamp.set(
        kStartTimestamp + (frameNumber / kKeyFrameRate) * kInterFrameDelay * kKeyFrameRate);

    // update the metadata
    if (keyFrameIndex == 0) {
      vrs::utils::test::IFrameData iFrame;
      iFrame.value = kBoardInitValue;
      return createRecord(
          getFrameTimestamp(frameNumber),
          Record::Type::DATA,
          kDataVersion,
          DataSource(metadata_, DataSourceChunk(&iFrame, sizeof(iFrame))));
    }
    keyFrameIndex--;
    vrs::utils::test::PFrameData pFrame = {};
    uint32_t cycleIndex = frameNumber / kKeyFrameRate;
    if (cycleIndex == 0) {
      pFrame.x = keyFrameIndex % kChessSquareSideCount;
      pFrame.y = keyFrameIndex / kChessSquareSideCount;
    } else if (cycleIndex == 1) {
      pFrame.x = keyFrameIndex / kChessSquareSideCount;
      pFrame.y = keyFrameIndex % kChessSquareSideCount;
    } else if (cycleIndex == 2) {
      pFrame.x = keyFrameIndex % kChessSquareSideCount;
      pFrame.y = keyFrameIndex / kChessSquareSideCount;
      if (pFrame.y % 2 == 1) {
        pFrame.x = kChessSquareSideCount - 1 - pFrame.x;
      }
    } else if (cycleIndex == 3) {
      pFrame.x = keyFrameIndex / kChessSquareSideCount;
      pFrame.y = keyFrameIndex % kChessSquareSideCount;
      if (pFrame.x % 2 == 1) {
        pFrame.y = kChessSquareSideCount - 1 - pFrame.y;
      }
    } else {
      vector<pair<uint32_t, uint32_t>>& trajectory = trajectories_[cycleIndex];
      if (trajectory.empty()) {
        pFrame.x = keyFrameIndex % kChessSquareSideCount;
        pFrame.y = keyFrameIndex / kChessSquareSideCount;
      } else {
        pFrame.x = trajectory[keyFrameIndex].first;
        pFrame.y = trajectory[keyFrameIndex].second;
      }
    }
    pFrame.expectedValue = ((pFrame.x % 2) ^ (pFrame.y % 2)) ? kBoardBlackValue : kBoardWhiteValue;
    pFrame.incrementValue = pFrame.expectedValue - kBoardInitValue;
    pFrame.xMax = kChessSquareSideCount;
    pFrame.yMax = kChessSquareSideCount;
    return createRecord(
        getFrameTimestamp(frameNumber),
        Record::Type::DATA,
        kDataVersion,
        DataSource(metadata_, DataSourceChunk(&pFrame, sizeof(pFrame))));
  }
  void makeSpiral(vector<pair<uint32_t, uint32_t>>& trajectory) {
    trajectory.reserve(kChessSquareSideCount * kChessSquareSideCount);
    int64_t minx = 0, miny = 0, maxx = kChessSquareSideCount - 1, maxy = kChessSquareSideCount - 1;
    int64_t x = 0, y = 0, incx = 1, incy = 0;
    while (trajectory.size() < kChessSquareSideCount * kChessSquareSideCount) {
      if (trajectory.empty() || trajectory.back() != pair<uint32_t, uint32_t>(x, y)) {
        trajectory.emplace_back(x, y);
      }
      x += incx, y += incy;
      if (x > maxx) {
        x = maxx, incx = 0, incy = 1, miny++;
      } else if (y > maxy) {
        y = maxy, incx = -1, incy = 0, maxx--;
      } else if (x < minx) {
        x = minx, incx = 0, incy = -1, maxy--;
      } else if (y < miny) {
        y = miny, incx = 1, incy = 0, minx++;
      }
    }
  }

 private:
  ImageStreamConfiguration config_;
  ImageStreamMetaData metadata_;
  map<uint32_t, vector<pair<uint32_t, uint32_t>>> trajectories_;
};

class SequenceImageStreamPlayer : public RecordFormatStreamPlayer {
 public:
  bool onDataLayoutRead(const CurrentRecord&, size_t, DataLayout&) override {
    // nothing useful to check...
    return true;
  }
  bool onImageRead(const CurrentRecord& record, size_t, const ContentBlock& contentBlock) override {
    videoFrameCounter++;
    // Validate info about frame position
    const auto& img = contentBlock.image();
    if (img.getKeyFrameIndex() == 0) {
      keyFrameCounter++;
      keyFrameIndexCounter = 0;
      currentKeyFrameTimestamp = record.timestamp;
      EXPECT_NEAR(img.getKeyFrameTimestamp(), record.timestamp, 0.000001);
    } else {
      keyFrameIndexCounter++;
      EXPECT_NEAR(currentKeyFrameTimestamp, img.getKeyFrameTimestamp(), 0.000001);
    }
    EXPECT_EQ(img.getKeyFrameIndex(), keyFrameIndexCounter);
    bool validFrame =
        (videoFrameHandler.tryToDecodeFrame(pixelFrame, record.reader, contentBlock) == 0);
    if (validFrame) {
      frameCounter++;
    }
    EXPECT_TRUE(validFrame);
    return validFrame;
  }

  uint32_t keyFrameCounter = 0;
  uint32_t keyFrameIndexCounter = 0;
  double currentKeyFrameTimestamp = 0;
  uint32_t videoFrameCounter = 0;
  uint32_t frameCounter = 0;

  vrs::utils::VideoFrameHandler videoFrameHandler;
  vrs::utils::PixelFrame pixelFrame;
};

#define OFFICIAL_BUILD 1

struct VideoFileTest : testing::Test {
  static void SetUpTestCase() {
    vrs::utils::DecoderFactory::get().registerDecoderMaker(&vrs::utils::test::chessDecoderMaker);

    // Create VRS file holding all the records in memory before writing them all in a single call.
    RecordFileWriter fileWriter;

    // add a stream to the file
    ImageStream imageStream;
    fileWriter.addRecordable(&imageStream);

    imageStream.createConfigurationRecord();
    imageStream.createStateRecord();
    for (uint32_t frameIndex = 0; frameIndex < kFrameCount; ++frameIndex) {
      imageStream.createFrame(frameIndex);
    }

    EXPECT_EQ(fileWriter.writeToFile(kTestFilePath), 0);
  }

  static void TearDownTestCase() {
#if OFFICIAL_BUILD
    os::remove(kTestFilePath);
#endif
  }

  static const std::string kTestFilePath;
};

#if OFFICIAL_BUILD
const std::string VideoFileTest::kTestFilePath = os::getHomeFolder() + "video_file_test.vrs";
#else
const std::string VideoFileTest::kTestFilePath = "/Users/gberenger/ovrsource/video_file_test.vrs";
#endif

} // namespace

TEST_F(VideoFileTest, sequenceTest) {
  // Verify that the file was created, and looks like we think it should
  RecordFileReader reader;
  int openFileStatus = reader.openFile(kTestFilePath);
  EXPECT_EQ(openFileStatus, 0);
  if (openFileStatus != 0) {
    return;
  }

  EXPECT_EQ(reader.getStreams().size(), 1);

  SequenceImageStreamPlayer imageStreamPlayer;
  reader.setStreamPlayer(*reader.getStreams().begin(), &imageStreamPlayer);
  reader.readAllRecords();
  reader.closeFile();
  EXPECT_EQ(imageStreamPlayer.videoFrameCounter, kFrameCount);
  EXPECT_EQ(imageStreamPlayer.keyFrameCounter, kFrameCount / kKeyFrameRate);
}

class RandomAccessImageStreamPlayer : public RecordFormatStreamPlayer {
 public:
  bool onDataLayoutRead(const CurrentRecord&, size_t, DataLayout&) override {
    datalayoutCount++;
    return true;
  }
  bool onImageRead(const CurrentRecord& record, size_t, const ContentBlock& contentBlock) override {
    videoFrameCount++;
    if (videoFrameHandler.tryToDecodeFrame(pixelFrame, record.reader, contentBlock) == 0) {
      goodVideoFrameCount++;
      return true;
    }
    return false;
  }

  bool isMissingFrames() const {
    return videoFrameHandler.isMissingFrames();
  }
  int readMissingFrames(
      RecordFileReader& fileReader,
      const IndexRecord::RecordInfo& record,
      bool exactFrame = true) {
    return videoFrameHandler.readMissingFrames(fileReader, record, exactFrame);
  }
  void resetCounts() {
    datalayoutCount = 0;
    videoFrameCount = 0;
    goodVideoFrameCount = 0;
  }

  uint32_t datalayoutCount = 0;
  uint32_t videoFrameCount = 0;
  uint32_t goodVideoFrameCount = 0;

  vrs::utils::VideoFrameHandler videoFrameHandler;
  vrs::utils::PixelFrame pixelFrame;
};

TEST_F(VideoFileTest, randomAccessTest) {
  // Verify that the file was created, and looks like we think it should
  RecordFileReader reader;
  int openFileStatus = reader.openFile(kTestFilePath);
  EXPECT_EQ(openFileStatus, 0);
  if (openFileStatus != 0) {
    return;
  }

  EXPECT_EQ(reader.getStreams().size(), 1);

  RandomAccessImageStreamPlayer imageStreamPlayer;
  StreamId imageStreamId = *reader.getStreams().begin();
  reader.setStreamPlayer(imageStreamId, &imageStreamPlayer);

  // Read config record
  {
    ASSERT_TRUE(reader.readFirstConfigurationRecord(imageStreamId));
    EXPECT_EQ(imageStreamPlayer.datalayoutCount, 1);
    EXPECT_EQ(imageStreamPlayer.videoFrameCount, 0);
    EXPECT_EQ(imageStreamPlayer.goodVideoFrameCount, 0);
  }

  // Read the second key frame, the first frame of the second group: should be no problem
  {
    auto secondKeyFrame = reader.getRecord(imageStreamId, Record::Type::DATA, kKeyFrameRate);
    ASSERT_NE(secondKeyFrame, nullptr);
    imageStreamPlayer.resetCounts();
    ASSERT_EQ(reader.readRecord(*secondKeyFrame), 0);
    EXPECT_FALSE(imageStreamPlayer.isMissingFrames());
    EXPECT_EQ(imageStreamPlayer.datalayoutCount, 1);
    EXPECT_EQ(imageStreamPlayer.videoFrameCount, 1);
    EXPECT_EQ(imageStreamPlayer.goodVideoFrameCount, 1);
  }

  // try to jump the last frame of the previous group: should fail and need to read the whole group
  {
    auto lastGroup1Frame = reader.getRecord(imageStreamId, Record::Type::DATA, kKeyFrameRate - 1);
    ASSERT_NE(lastGroup1Frame, nullptr);
    imageStreamPlayer.resetCounts();
    ASSERT_EQ(reader.readRecord(*lastGroup1Frame), 0);
    EXPECT_TRUE(imageStreamPlayer.isMissingFrames());
    EXPECT_EQ(imageStreamPlayer.datalayoutCount, 1);
    EXPECT_EQ(imageStreamPlayer.videoFrameCount, 1);
    EXPECT_EQ(imageStreamPlayer.goodVideoFrameCount, 0);

    imageStreamPlayer.resetCounts();
    ASSERT_EQ(imageStreamPlayer.readMissingFrames(reader, *lastGroup1Frame), 0);
    // we have to read the whole group
    EXPECT_EQ(imageStreamPlayer.datalayoutCount, kKeyFrameRate);
    EXPECT_EQ(imageStreamPlayer.videoFrameCount, kKeyFrameRate);
    EXPECT_EQ(imageStreamPlayer.goodVideoFrameCount, kKeyFrameRate);
  }

  // try to jump the 2nd frame of a different group
  {
    auto group2frame2 = reader.getRecord(imageStreamId, Record::Type::DATA, 2 * kKeyFrameRate + 1);
    ASSERT_NE(group2frame2, nullptr);
    imageStreamPlayer.resetCounts();
    ASSERT_EQ(reader.readRecord(*group2frame2), 0);
    EXPECT_TRUE(imageStreamPlayer.isMissingFrames());
    EXPECT_EQ(imageStreamPlayer.datalayoutCount, 1);
    EXPECT_EQ(imageStreamPlayer.videoFrameCount, 1);
    EXPECT_EQ(imageStreamPlayer.goodVideoFrameCount, 0);

    imageStreamPlayer.resetCounts();
    ASSERT_EQ(imageStreamPlayer.readMissingFrames(reader, *group2frame2), 0);
    // we have to read 2 frames
    EXPECT_EQ(imageStreamPlayer.datalayoutCount, 2);
    EXPECT_EQ(imageStreamPlayer.videoFrameCount, 2);
    EXPECT_EQ(imageStreamPlayer.goodVideoFrameCount, 2);
  }

  // try to jump the 5th frame of the same group, 3 frames further
  {
    auto group2frame2 = reader.getRecord(imageStreamId, Record::Type::DATA, 2 * kKeyFrameRate + 4);
    ASSERT_NE(group2frame2, nullptr);
    imageStreamPlayer.resetCounts();
    ASSERT_EQ(reader.readRecord(*group2frame2), 0);
    EXPECT_TRUE(imageStreamPlayer.isMissingFrames());
    EXPECT_EQ(imageStreamPlayer.datalayoutCount, 1);
    EXPECT_EQ(imageStreamPlayer.videoFrameCount, 1);
    EXPECT_EQ(imageStreamPlayer.goodVideoFrameCount, 0);

    imageStreamPlayer.resetCounts();
    ASSERT_EQ(imageStreamPlayer.readMissingFrames(reader, *group2frame2), 0);
    // we have to read 3 frames only, because we have read the 2 first frames of the group already
    EXPECT_EQ(imageStreamPlayer.datalayoutCount, 3);
    EXPECT_EQ(imageStreamPlayer.videoFrameCount, 3);
    EXPECT_EQ(imageStreamPlayer.goodVideoFrameCount, 3);
  }

  reader.closeFile();
}

class RandomAccessVideoStreamPlayer : public vrs::utils::VideoRecordFormatStreamPlayer {
 public:
  bool onDataLayoutRead(const CurrentRecord&, size_t, DataLayout&) override {
    datalayoutCount++;
    return true;
  }
  bool onImageRead(const CurrentRecord& record, size_t, const ContentBlock& contentBlock) override {
    videoFrameCount++;
    if (tryToDecodeFrame(pixelFrame, record, contentBlock) == 0) {
      goodVideoFrameCount++;
      return true;
    }
    return false;
  }
  void resetCounts() {
    datalayoutCount = 0;
    videoFrameCount = 0;
    goodVideoFrameCount = 0;
  }

  uint32_t datalayoutCount = 0;
  uint32_t videoFrameCount = 0;
  uint32_t goodVideoFrameCount = 0;

  vrs::utils::PixelFrame pixelFrame;
};

TEST_F(VideoFileTest, videoRecordFormatStreamPlayerRandomAccessTest) {
  // Verify that the file was created, and looks like we think it should
  RecordFileReader reader;
  int openFileStatus = reader.openFile(kTestFilePath);
  EXPECT_EQ(openFileStatus, 0);
  if (openFileStatus != 0) {
    return;
  }

  EXPECT_EQ(reader.getStreams().size(), 1);

  RandomAccessVideoStreamPlayer imageStreamPlayer;
  StreamId imageStreamId = *reader.getStreams().begin();
  reader.setStreamPlayer(imageStreamId, &imageStreamPlayer);

  // Read config record
  {
    ASSERT_TRUE(reader.readFirstConfigurationRecord(imageStreamId));
    EXPECT_EQ(imageStreamPlayer.datalayoutCount, 1);
    EXPECT_EQ(imageStreamPlayer.videoFrameCount, 0);
    EXPECT_EQ(imageStreamPlayer.goodVideoFrameCount, 0);
  }

  // Read the second key frame, the first frame of the second group: should be no problem
  {
    auto secondKeyFrame = reader.getRecord(imageStreamId, Record::Type::DATA, kKeyFrameRate);
    ASSERT_NE(secondKeyFrame, nullptr);
    imageStreamPlayer.resetCounts();
    ASSERT_EQ(reader.readRecord(*secondKeyFrame), 0);
    EXPECT_FALSE(imageStreamPlayer.isMissingFrames(imageStreamId));
    EXPECT_EQ(imageStreamPlayer.datalayoutCount, 1);
    EXPECT_EQ(imageStreamPlayer.videoFrameCount, 1);
    EXPECT_EQ(imageStreamPlayer.goodVideoFrameCount, 1);
  }

  // try to jump the last frame of the previous group: should fail and need to read the whole group
  {
    auto lastGroup1Frame = reader.getRecord(imageStreamId, Record::Type::DATA, kKeyFrameRate - 1);
    ASSERT_NE(lastGroup1Frame, nullptr);
    imageStreamPlayer.resetCounts();
    ASSERT_EQ(reader.readRecord(*lastGroup1Frame), 0);
    EXPECT_TRUE(imageStreamPlayer.isMissingFrames(imageStreamId));
    EXPECT_EQ(imageStreamPlayer.datalayoutCount, 1);
    EXPECT_EQ(imageStreamPlayer.videoFrameCount, 1);
    EXPECT_EQ(imageStreamPlayer.goodVideoFrameCount, 0);

    imageStreamPlayer.resetCounts();
    ASSERT_EQ(imageStreamPlayer.readMissingFrames(reader, *lastGroup1Frame), 0);
    // we have to read the whole group
    EXPECT_EQ(imageStreamPlayer.datalayoutCount, kKeyFrameRate);
    EXPECT_EQ(imageStreamPlayer.videoFrameCount, kKeyFrameRate);
    EXPECT_EQ(imageStreamPlayer.goodVideoFrameCount, kKeyFrameRate);
  }

  // try to jump the 2nd frame of a different group
  {
    auto group2frame2 = reader.getRecord(imageStreamId, Record::Type::DATA, 2 * kKeyFrameRate + 1);
    ASSERT_NE(group2frame2, nullptr);
    imageStreamPlayer.resetCounts();
    ASSERT_EQ(reader.readRecord(*group2frame2), 0);
    EXPECT_TRUE(imageStreamPlayer.isMissingFrames(imageStreamId));
    EXPECT_EQ(imageStreamPlayer.datalayoutCount, 1);
    EXPECT_EQ(imageStreamPlayer.videoFrameCount, 1);
    EXPECT_EQ(imageStreamPlayer.goodVideoFrameCount, 0);

    imageStreamPlayer.resetCounts();
    ASSERT_EQ(imageStreamPlayer.readMissingFrames(reader, *group2frame2), 0);
    // we have to read 2 frames
    EXPECT_EQ(imageStreamPlayer.datalayoutCount, 2);
    EXPECT_EQ(imageStreamPlayer.videoFrameCount, 2);
    EXPECT_EQ(imageStreamPlayer.goodVideoFrameCount, 2);
  }

  // try to jump the 5th frame of the same group, 3 frames further
  {
    auto group2frame2 = reader.getRecord(imageStreamId, Record::Type::DATA, 2 * kKeyFrameRate + 4);
    ASSERT_NE(group2frame2, nullptr);
    imageStreamPlayer.resetCounts();
    ASSERT_EQ(reader.readRecord(*group2frame2), 0);
    EXPECT_TRUE(imageStreamPlayer.isMissingFrames(imageStreamId));
    EXPECT_EQ(imageStreamPlayer.datalayoutCount, 1);
    EXPECT_EQ(imageStreamPlayer.videoFrameCount, 1);
    EXPECT_EQ(imageStreamPlayer.goodVideoFrameCount, 0);

    imageStreamPlayer.resetCounts();
    ASSERT_EQ(imageStreamPlayer.readMissingFrames(reader, *group2frame2), 0);
    // we have to read 3 frames only, because we have read the 2 first frames of the group already
    EXPECT_EQ(imageStreamPlayer.datalayoutCount, 3);
    EXPECT_EQ(imageStreamPlayer.videoFrameCount, 3);
    EXPECT_EQ(imageStreamPlayer.goodVideoFrameCount, 3);
  }

  reader.closeFile();
}
