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

#include <TestDataDir/TestDataDir.h>

#include <vrs/DataLayoutConventions.h>
#include <vrs/FileHandler.h>
#include <vrs/FileHandlerFactory.h>
#include <vrs/RecordFileReader.h>
#include <vrs/RecordFileWriter.h>
#include <vrs/RecordFormatStreamPlayer.h>
#include <vrs/os/Utils.h>

// This is more a code sample than an actual unit test
// It demonstrates how to create a VRS file with an image stream, using RecordFormat & DataLayout.
// The produced VRS file can actually be played like a video by VRSplayer.

using namespace std;
using namespace vrs;

namespace {

const uint32_t kFrameWidth = 640;
const uint32_t kFrameHeight = 480;
const uint32_t kPixelByteSize = 1;

const uint32_t kFrameRate = 30; // Hz
const uint32_t kImagesPerTimestamp = 4; // to test ordering of records with identical tstamps
const uint32_t kFrameCount = kFrameRate * 5; // 5 seconds worth of frames
const double kInterFrameDelay = 1.0 / kFrameRate;

const uint32_t kConfigurationVersion = 1;
const uint32_t kDataVersion = 1;

const double kStartTimestamp = 1543864285;

using datalayout_conventions::ImageSpecType;

double getFrameTimestamp(uint32_t frameNumber) {
  uint32_t frameGroup = frameNumber / kImagesPerTimestamp;
  return kStartTimestamp + frameGroup * kInterFrameDelay;
}

class ImageStreamConfiguration : public AutoDataLayout {
 public:
  // Define the image format following conventions
  DataPieceValue<ImageSpecType> width{datalayout_conventions::kImageWidth};
  DataPieceValue<ImageSpecType> height{datalayout_conventions::kImageHeight};
  DataPieceEnum<PixelFormat, ImageSpecType> pixelFormat{datalayout_conventions::kImagePixelFormat};

  // Some user code fields...
  DataPieceString cameraSerial{"camera_serial"};

  AutoDataLayoutEnd endLayout;
};

class ImageStreamMetaData : public AutoDataLayout {
 public:
  // Some user code fields...
  DataPieceValue<uint64_t> frameCounter{"frame_counter"};

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
        metadata_.getContentBlock() + ContentBlock(ImageFormat::RAW), // metadata + image
        {&metadata_});
  }

  const Record* createConfigurationRecord() override {
    // record the actual image format
    config_.width.set(kFrameWidth);
    config_.height.set(kFrameHeight);
    config_.pixelFormat.set(PixelFormat::GREY8);

    // set some additional config info
    config_.cameraSerial.stage("my_fake_camera_serial_number");
    return createRecord(
        kStartTimestamp, Record::Type::CONFIGURATION, kConfigurationVersion, DataSource(config_));
  }

  const Record* createStateRecord() override {
    // Not used, but we still need to create a record
    return createRecord(kStartTimestamp, Record::Type::STATE, 0);
  }

  const Record* createFrame(uint32_t frameNumber) {
    // simulate some image content
    const size_t frameBufferSize = kFrameWidth * kFrameHeight * kPixelByteSize;
    vector<uint8_t> frameBuffer(frameBufferSize);
    for (uint32_t n = 0; n < frameBufferSize; ++n) {
      frameBuffer[n] = static_cast<uint8_t>(frameNumber + n);
    }

    // update the metadata
    metadata_.frameCounter.set(frameNumber);

    // create the record
    return createRecord(
        getFrameTimestamp(frameNumber),
        Record::Type::DATA,
        kDataVersion,
        DataSource(metadata_, frameBuffer));
  }

 private:
  ImageStreamConfiguration config_;
  ImageStreamMetaData metadata_;
};

class ImageStreamPlayer : public RecordFormatStreamPlayer {
 public:
  bool onDataLayoutRead(const CurrentRecord& record, size_t blockIndex, DataLayout& dl) override {
    if (record.recordType == Record::Type::DATA) {
      ImageStreamMetaData& data = getExpectedLayout<ImageStreamMetaData>(dl, blockIndex);
      uint64_t frameCounter = 0;
      bool foundFrameCounter = data.frameCounter.get(frameCounter);
      EXPECT_TRUE(foundFrameCounter);
      if (foundFrameCounter) {
        EXPECT_EQ(frameCounter, expectedFrameCounter);
        expectedFrameCounter++;
      }
    }
    return false; // don't bother reading the images
  }
  uint64_t expectedFrameCounter = 0;
};

struct SimpleFileHandlerTest : testing::Test {
  // Demonstrate how to create a VRS file,
  // holding all the records in memory before writing them all in a single call.
  static int createFileAtOnce(const string& filePath) {
    // Create a container to hold all references to all the streams we want to record
    RecordFileWriter fileWriter;

    // set some file tags
    fileWriter.setTag("purpose", "this is a test");

    // add a stream to the file
    ImageStream imageStream;
    fileWriter.addRecordable(&imageStream);

    // add tags to the stream, maybe to describe it if there are more than one of the same type
    imageStream.setTag("camera_role", "fake device");

    // Create records: when proceeded synchronously, you're in charge of creating every record
    imageStream.createConfigurationRecord();
    imageStream.createStateRecord();
    for (uint32_t frameIndex = 0; frameIndex < kFrameCount; ++frameIndex) {
      imageStream.createFrame(frameIndex);
    }

    // At this point, all the records are in memory, waiting...

    // create the file & write all the records created above in one shot
    EXPECT_EQ(fileWriter.writeToFile(filePath), 0);
    return 0;
  }

  // Demonstrate how to create a VRS file,
  // writing records in the background as we create more.
  // Even if all your records fit in memory, this version is much faster, so you should use it!
  static int createFileStreamingToDisk(const string& filePath) {
    // Create a container to hold all references to all the streams we want to record
    RecordFileWriter fileWriter;

    // set some file tags
    fileWriter.setTag("purpose", "this is a test");

    // add a stream to the file
    ImageStream imageStream;
    fileWriter.addRecordable(&imageStream);

    // add tags to the stream, maybe to describe it if there are more than one of the same type
    imageStream.setTag("camera_role", "fake device");

    // create the file, but we're not writing records to it yet.
    // When using Async files, config & state records are created automatically.
    EXPECT_EQ(fileWriter.createFileAsync(filePath), 0);

    for (uint32_t frameIndex = 0; frameIndex < kFrameCount; ++frameIndex) {
      imageStream.createFrame(frameIndex);

      if (frameIndex % 5 == 0) {
        // every 5th record, push all the records to be written in a background thread
        fileWriter.writeRecordsAsync(numeric_limits<double>::max());
      }
    }

    // At this point, all the records are being written to disk in the background...

    // Request to close the file, without waiting for the operation to complete.
    EXPECT_EQ(fileWriter.closeFileAsync(), 0);

    // Wait synchronously for the whole thing to complete:
    // you didn't need to call closeFileAsync() if you did not have something else to do...
    EXPECT_EQ(fileWriter.waitForFileClosed(), 0);
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

    ImageStreamPlayer imageStreamPlayer;
    reader.setStreamPlayer(*reader.getStreams().begin(), &imageStreamPlayer);
    reader.readAllRecords();

    // 1 config record + 1 state record + kFrameCount images
    size_t kTotalRecordCount = 1 + 1 + kFrameCount; // what we expect
    size_t actualRecordCount = reader.getIndex().size(); // what the file contains
    EXPECT_EQ(actualRecordCount, kTotalRecordCount);
    reader.closeFile();
  }
};

} // namespace

TEST_F(SimpleFileHandlerTest, simpleCreation) {
  const string testPath = os::getTempFolder() + "SyncSimpleFileHandlerTest.vrs";
  ASSERT_EQ(createFileAtOnce(testPath), 0);

  checkFileHandler(testPath);

  os::remove(testPath);
}

TEST_F(SimpleFileHandlerTest, asyncCreation) {
  const string testPath = os::getTempFolder() + "AsyncSimpleFileHandlerTest.vrs";
  ASSERT_EQ(createFileStreamingToDisk(testPath), 0);

  checkFileHandler(testPath);

  os::remove(testPath);
}

TEST_F(SimpleFileHandlerTest, openFileWithJsonPath) {
  const string testPath = os::getTempFolder() + "VRSJsonFilePathTest.vrs";
  ASSERT_EQ(createFileAtOnce(testPath), 0);

  const string jsonPath = FileSpec({testPath}).toJson();
  checkFileHandler(jsonPath);

  os::remove(testPath);
}

TEST_F(SimpleFileHandlerTest, openFileWithJsonPathForExistingFiles) {
  vrs::RecordFileReader file;
  string kChunkedFile = os::pathJoin(coretech::getTestDataDir(), "VRS_Files/chunks.vrs");
  string kChunkedFile2 = os::pathJoin(coretech::getTestDataDir(), "VRS_Files/chunks.vrs_1");
  string kChunkedFile3 = os::pathJoin(coretech::getTestDataDir(), "VRS_Files/chunks.vrs_2");
  const string jsonPath = FileSpec({kChunkedFile, kChunkedFile2, kChunkedFile3}).toJson();
  ASSERT_EQ(file.openFile(jsonPath), 0);
  EXPECT_EQ(file.getRecordCount(), 306); // number of records if all chunks are found
  EXPECT_EQ(file.getFileChunks().size(), 3);
}

TEST_F(SimpleFileHandlerTest, encodeDecode) {
  FileSpec spec;
  spec.uri = "my uri";
  spec.fileName = "file name";
  spec.fileHandlerName = "filehandler";
  spec.chunks = {"one", "two", "three"};
  spec.chunkSizes = {1, 2, 3};
  spec.extras = {{"hello", "bonjour"}, {"bye", "au revoir"}};
  string json = spec.toJson();
  FileSpec other;
  ASSERT_TRUE(other.fromJson(json));
  EXPECT_EQ(spec, other);
}

static void invalidate(FileSpec& spec) {
  spec.uri = "this is";
  spec.fileName = "a set of";
  spec.fileHandlerName = "non-sensical values";
  spec.chunks = {"to make", "sure that", "we set all the fields"};
  spec.chunkSizes = {-1, -2, -3};
}

inline string escape(const string& str) {
  string out;
  for (char c : str) {
    switch (c) {
      case '\\':
        out += "\\\\";
        break;
      case '"':
        out += "\\\"";
        break;
      default:
        out.push_back(c);
    }
  }
  return out;
}

// Fake delegator for testing
class PastisDelegator : public FileDelegator {
 public:
  int delegateOpen(const FileSpec& fileSpec, unique_ptr<FileHandler>& outNewDelegate) override {
    outNewDelegate = make_unique<DiskFile>();
    return 0;
  }
};

TEST_F(SimpleFileHandlerTest, pathJsonUriParse) {
  FileHandlerFactory::getInstance().registerExtraDelegator(
      "dealer", "pastis", make_unique<PastisDelegator>());

  vector<string> chunks;
  vector<int64_t> chunkSizes;
  FileSpec spec;

  invalidate(spec);
  string path = "/this/is/a/file/path";
  EXPECT_EQ(spec.fromPathJsonUri(path), 0);
  EXPECT_TRUE(spec.uri.empty());
  EXPECT_EQ(spec.fileHandlerName, DiskFile::staticName());
  chunks = {path};
  EXPECT_EQ(spec.chunks, chunks);
  EXPECT_TRUE(spec.chunkSizes.empty());
  EXPECT_TRUE(spec.fileName.empty());
  EXPECT_FALSE(spec.hasChunkSizes());
  EXPECT_EQ(spec.getFileSize(), -1);
  EXPECT_EQ(spec.getSourceLocation(), DiskFile::staticName());
  EXPECT_EQ(spec.toJson(), "{\"chunks\":[\"" + escape(path) + "\"],\"storage\":\"diskfile\"}");
  EXPECT_EQ(spec.getEasyPath(), path);

  invalidate(spec);
  string filePath = "/this/is/a/file/path";
  path = "diskfile:" + filePath;
  EXPECT_EQ(spec.fromPathJsonUri(path), 0);
  EXPECT_FALSE(spec.uri.empty());
  EXPECT_EQ(spec.fileHandlerName, DiskFile::staticName());
  chunks = {filePath};
  EXPECT_EQ(spec.chunks, chunks);
  EXPECT_TRUE(spec.chunkSizes.empty());
  EXPECT_TRUE(spec.fileName.empty());
  EXPECT_FALSE(spec.hasChunkSizes());
  EXPECT_EQ(spec.getFileSize(), -1);
  EXPECT_EQ(spec.getSourceLocation(), DiskFile::staticName()); // keep file location hidden
  EXPECT_EQ(
      spec.toJson(),
      "{\"chunks\":[\"" + escape(filePath) +
          "\"],\"storage\":\"diskfile\","
          "\"source_uri\":\"diskfile:/this/is/a/file/path\"}");
  EXPECT_EQ(spec.getEasyPath(), path);

  invalidate(spec);
  path = "/this/is/a/file/path:with:colons";
  EXPECT_EQ(spec.fromPathJsonUri(path), 0);
  EXPECT_TRUE(spec.uri.empty());
  EXPECT_EQ(spec.fileHandlerName, DiskFile::staticName());
  chunks = {path};
  EXPECT_EQ(spec.chunks, chunks);
  EXPECT_TRUE(spec.chunkSizes.empty());
  EXPECT_TRUE(spec.fileName.empty());
  EXPECT_FALSE(spec.hasChunkSizes());
  EXPECT_EQ(spec.getFileSize(), -1);
  EXPECT_EQ(spec.getSourceLocation(), DiskFile::staticName());
  EXPECT_EQ(spec.toJson(), "{\"chunks\":[\"" + escape(path) + "\"],\"storage\":\"diskfile\"}");
  EXPECT_EQ(spec.getEasyPath(), path);

  invalidate(spec);
  path = "A:\\\\this\\is\\a\\windows\\path";
  EXPECT_EQ(spec.fromPathJsonUri(path), 0);
  EXPECT_TRUE(spec.uri.empty());
  EXPECT_EQ(spec.fileHandlerName, DiskFile::staticName());
  chunks = {path};
  EXPECT_EQ(spec.chunks, chunks);
  EXPECT_TRUE(spec.chunkSizes.empty());
  EXPECT_TRUE(spec.fileName.empty());
  EXPECT_FALSE(spec.hasChunkSizes());
  EXPECT_EQ(spec.getFileSize(), -1);
  EXPECT_EQ(spec.getSourceLocation(), DiskFile::staticName());
  EXPECT_EQ(spec.toJson(), "{\"chunks\":[\"" + escape(path) + "\"],\"storage\":\"diskfile\"}");

  invalidate(spec);
  path = "mystorage:123456";
  EXPECT_EQ(spec.fromPathJsonUri(path), 0);
  EXPECT_EQ(spec.uri, path);
  EXPECT_EQ(spec.fileHandlerName, "mystorage");
  chunks = {"123456"};
  EXPECT_EQ(spec.chunks, chunks);
  EXPECT_TRUE(spec.chunkSizes.empty());
  EXPECT_TRUE(spec.fileName.empty());
  EXPECT_FALSE(spec.hasChunkSizes());
  EXPECT_EQ(spec.getFileSize(), -1);
  EXPECT_EQ(spec.getSourceLocation(), path);
  EXPECT_EQ(
      spec.toJson(),
      "{\"chunks\":[\"123456\"],\"storage\":\"mystorage\",\"source_uri\":\"mystorage:123456\"}");
  EXPECT_EQ(spec.getEasyPath(), "mystorage:123456");

  invalidate(spec);
  path =
      "{\"filename\":\"myfile.vrs\",\"storage\":\"http\",\"source_uri\":\"mystorage:123456\","
      "\"chunks\":[\"first chunk\",\"second chunk\"],\"chunk_sizes\":[12345,6789]}";
  EXPECT_EQ(spec.fromPathJsonUri(path), 0);
  EXPECT_EQ(spec.uri, "mystorage:123456");
  EXPECT_EQ(spec.fileHandlerName, "http");
  chunks = {"first chunk", "second chunk"};
  EXPECT_EQ(spec.chunks, chunks);
  chunkSizes = {12345, 6789};
  EXPECT_EQ(spec.chunkSizes, chunkSizes);
  EXPECT_EQ(spec.fileName, "myfile.vrs");
  EXPECT_TRUE(spec.hasChunkSizes());
  EXPECT_EQ(spec.getFileSize(), 12345 + 6789);
  EXPECT_EQ(spec.getSourceLocation(), "mystorage:123456");
  EXPECT_EQ(
      spec.toJson(),
      "{\"chunks\":[\"first chunk\",\"second chunk\"],\"chunk_sizes\":[12345,6789],"
      "\"storage\":\"http\",\"filename\":\"myfile.vrs\",\"source_uri\":\"mystorage:123456\"}");
  EXPECT_EQ(spec.getEasyPath(), "uri: mystorage:123456, name: myfile.vrs");

  invalidate(spec);

#define PATH1                                 \
  "//interncache-atn.fbcdn.net/v/t63.8864-7/" \
  "10000000_118246862403539_6451070337772683264_n.jpg"
#define CHUNK1                                                                                \
  "https:" PATH1                                                                              \
  "?_nc_sid=6ee997&efg=eyJ1cmxnZW4iOiJwaHBfdXJsZ2VuX2NsaWVudC9lbnRfZ2VuL0VudEdhaWFSZWNvcmRpb" \
  "mdGaWxlIn0%3D&_nc_ht=interncache-atn&oh=f2c6e5306b40c4fc788580a1897852cb&oe=5EE6111C"
#define CHUNK2                                                                                    \
  "https://interncache-atn.fbcdn.net/v/t63.8864-7/10000000_333293020806762_3599126999991320576_n" \
  ".jpg?_nc_sid=6ee997&efg=eyJ1cmxnZW4iOiJwaHBfdXJsZ2VuX2NsaWVudC9lbnRfZ2VuL0VudEdhaWFSZWNvcmRpb" \
  "mdGaWxlIn0%3D&_nc_ht=interncache-atn&oh=dc18ba6e0feebbae5815c04c53e5b93f&oe=5EE84225"
#define CHUNK3                                                                                   \
  "https://interncache-atn.fbcdn.net/v/t63.8864-7/10000000_389143991841345_897914354052104192_n" \
  ".jpg?_nc_sid=6ee997&efg=eyJ1cmxnZW4iOiJwaHBfdXJsZ2VuX2NsaWVudC9lbnRfZ2VuL0VudEdhaWFSZWNvcmRp" \
  "bmdGaWxlIn0%3D&_nc_ht=interncache-atn&oh=9e7b2e1dd75bd0994a1417323305ecf5&oe=5EE63484"
  path = "{\"chunks\":[\"" CHUNK1 "\",\"" CHUNK2 "\",\"" CHUNK3
         "\"],\"chunk_sizes\":[1073741824,23598876,3265687],\"storage\":\"http\",\"filename\":"
         "\"VRSLargeTestFile.vrs\",\"source_uri\":\"mystorage:480864042405253\",\"version\":\"1\"}";
  EXPECT_EQ(spec.fromPathJsonUri(path), 0);
  EXPECT_EQ(spec.uri, "mystorage:480864042405253");
  EXPECT_EQ(spec.fileHandlerName, "http");
  chunks = {CHUNK1, CHUNK2, CHUNK3};
  EXPECT_EQ(spec.chunks, chunks);
  chunkSizes = {1073741824, 23598876, 3265687};
  EXPECT_EQ(spec.chunkSizes, chunkSizes);
  EXPECT_EQ(spec.fileName, "VRSLargeTestFile.vrs");
  EXPECT_TRUE(spec.hasChunkSizes());
  EXPECT_EQ(spec.getFileSize(), 1100606387);
  EXPECT_EQ(spec.getSourceLocation(), "mystorage:480864042405253");
  EXPECT_EQ(
      spec.toJson(),
      "{\"chunks\":[\"" CHUNK1 "\",\"" CHUNK2 "\",\"" CHUNK3
      "\"],\"chunk_sizes\":[1073741824,23598876,3265687],\"storage\":\"http\",\"filename\":"
      "\"VRSLargeTestFile.vrs\",\"source_uri\":\"mystorage:480864042405253\",\"version\":\"1\"}");
  EXPECT_EQ(spec.getEasyPath(), "uri: mystorage:480864042405253, name: VRSLargeTestFile.vrs");

  // remove uri
  path = "{\"chunks\":[\"" CHUNK1 "\",\"" CHUNK2 "\",\"" CHUNK3
         "\"],\"chunk_sizes\":[1073741824,23598876,3265687],\"storage\":\"http\",\"filename\":"
         "\"VRSLargeTestFile.vrs\",\"version\":\"1\"}";
  EXPECT_EQ(spec.fromPathJsonUri(path), 0);
  EXPECT_EQ(spec.uri, "");
  EXPECT_EQ(spec.fileHandlerName, "http");
  chunks = {CHUNK1, CHUNK2, CHUNK3};
  EXPECT_EQ(spec.chunks, chunks);
  chunkSizes = {1073741824, 23598876, 3265687};
  EXPECT_EQ(spec.chunkSizes, chunkSizes);
  EXPECT_EQ(spec.fileName, "VRSLargeTestFile.vrs");
  EXPECT_TRUE(spec.hasChunkSizes());
  EXPECT_EQ(spec.getFileSize(), 1100606387);
  EXPECT_EQ(spec.getSourceLocation(), "http");
  EXPECT_EQ(
      spec.toJson(),
      "{\"chunks\":[\"" CHUNK1 "\",\"" CHUNK2 "\",\"" CHUNK3
      "\"],\"chunk_sizes\":[1073741824,23598876,3265687],\"storage\":\"http\",\"filename\":"
      "\"VRSLargeTestFile.vrs\",\"version\":\"1\"}");
  EXPECT_EQ(spec.getEasyPath(), "storage: http, name: VRSLargeTestFile.vrs");

  // remove uri & filename
  path =
      "{\"chunks\":[\"" CHUNK1 "\",\"" CHUNK2 "\",\"" CHUNK3
      "\"],\"chunk_sizes\":[1073741824,23598876,3265687],\"storage\":\"http\",\"version\":\"1\"}";
  EXPECT_EQ(spec.fromPathJsonUri(path), 0);
  EXPECT_EQ(spec.uri, "");
  EXPECT_EQ(spec.fileHandlerName, "http");
  chunks = {CHUNK1, CHUNK2, CHUNK3};
  EXPECT_EQ(spec.chunks, chunks);
  chunkSizes = {1073741824, 23598876, 3265687};
  EXPECT_EQ(spec.chunkSizes, chunkSizes);
  EXPECT_EQ(spec.fileName, "");
  EXPECT_TRUE(spec.hasChunkSizes());
  EXPECT_EQ(spec.getFileSize(), 1100606387);
  EXPECT_EQ(spec.getSourceLocation(), "http");
  EXPECT_EQ(
      spec.toJson(),
      "{\"chunks\":[\"" CHUNK1 "\",\"" CHUNK2 "\",\"" CHUNK3
      "\"],\"chunk_sizes\":[1073741824,23598876,3265687],\"storage\":\"http\",\"version\":\"1\"}");
  EXPECT_EQ(
      spec.getEasyPath(),
      "{\"chunks\":[\"https://interncach...a1897852cb&oe=5EE6111C\",\"https://interncach...4c53e5b9"
      "3f&oe=5EE84225\",\"https://interncach...323305ecf5&oe=5EE63484\"],\"storage\":\"http\"}");

  EXPECT_EQ(spec.fromPathJsonUri(CHUNK1), 0);
  EXPECT_EQ(spec.uri, CHUNK1);
  EXPECT_EQ(spec.fileHandlerName, "https");
  chunks = {PATH1};
  EXPECT_EQ(spec.chunks, chunks);
  EXPECT_TRUE(spec.chunkSizes.empty());
  EXPECT_EQ(spec.fileName, "");
  EXPECT_FALSE(spec.hasChunkSizes());
  EXPECT_EQ(spec.getFileSize(), -1);
  EXPECT_EQ(spec.getSourceLocation(), "https://interncache-atn.fbcdn.net");
  const char* json =
      "{\"chunks\":[\"" PATH1 "\"],\"storage\":\"https\",\"source_uri\":\"" CHUNK1
      "\",\"_nc_ht\":\"interncache-atn\",\"_nc_sid\":\"6ee997\",\"efg\":\"eyJ1cmxnZW4iOiJwaHBfdXJs"
      "Z2VuX2NsaWVudC9lbnRfZ2VuL0VudEdhaWFSZWNvcmRpbmdGaWxlIn0=\",\"oe\":\"5EE6111C\",\"oh\":\"f2c"
      "6e5306b40c4fc788580a1897852cb\"}";
  EXPECT_EQ(spec.toJson(), json);
  EXPECT_EQ(spec.getEasyPath(), CHUNK1);

#define PATH "//domain/folder/dir/file.ext"
#define URI "nfs:" PATH
#define URIQ URI "?q=1"
  EXPECT_EQ(spec.fromPathJsonUri(URIQ), 0);
  EXPECT_EQ(spec.uri, URIQ);
  EXPECT_EQ(spec.fileHandlerName, "nfs");
  chunks = {PATH};
  EXPECT_EQ(spec.chunks, chunks);
  EXPECT_TRUE(spec.chunkSizes.empty());
  EXPECT_EQ(spec.fileName, "");
  EXPECT_FALSE(spec.hasChunkSizes());
  EXPECT_EQ(spec.getFileSize(), -1);
  EXPECT_EQ(spec.getSourceLocation(), "nfs://domain");
  json = "{\"chunks\":[\"" PATH "\"],\"storage\":\"nfs\",\"source_uri\":\"" URIQ "\",\"q\":\"1\"}";
  EXPECT_EQ(spec.toJson(), json);
  EXPECT_EQ(spec.getEasyPath(), URIQ);

#define PATH_PARAM PATH "?p1=val&p2=otherval"
  EXPECT_EQ(spec.fromPathJsonUri(PATH_PARAM), 0);
  EXPECT_EQ(spec.uri, PATH_PARAM);
  EXPECT_EQ(spec.fileHandlerName, "diskfile");
  chunks = {PATH};
  EXPECT_EQ(spec.chunks, chunks);
  EXPECT_TRUE(spec.chunkSizes.empty());
  EXPECT_EQ(spec.fileName, "");
  EXPECT_FALSE(spec.hasChunkSizes());
  EXPECT_EQ(spec.getFileSize(), -1);
  EXPECT_EQ(spec.getSourceLocation(), "diskfile");
  json =
      "{\"chunks\":[\"//domain/folder/dir/file.ext\"],\"storage\":\"diskfile\",\"source_uri\":\"//d"
      "omain/folder/dir/file.ext?p1=val&p2=otherval\",\"p1\":\"val\",\"p2\":\"otherval\"}";
  EXPECT_EQ(spec.toJson(), json);
  EXPECT_EQ(spec.getEasyPath(), PATH_PARAM);

  // Windows. :-(
#define WINDOWS_PATH "\\\\?\\D:\\folder\\dir\\file.ext"
  EXPECT_EQ(spec.fromPathJsonUri(WINDOWS_PATH), 0);
  EXPECT_TRUE(spec.uri.empty());
  EXPECT_EQ(spec.fileHandlerName, "diskfile");
  chunks = {WINDOWS_PATH};
  EXPECT_EQ(spec.chunks, chunks);
  EXPECT_TRUE(spec.chunkSizes.empty());
  EXPECT_EQ(spec.fileName, "");
  EXPECT_FALSE(spec.hasChunkSizes());
  EXPECT_EQ(spec.getFileSize(), -1);
  EXPECT_EQ(spec.getSourceLocation(), "diskfile");
  json = "{\"chunks\":[\"\\\\\\\\?\\\\D:\\\\folder\\\\dir\\\\file.ext\"],\"storage\":\"diskfile\"}";
  EXPECT_EQ(spec.toJson(), json);
  EXPECT_EQ(spec.getEasyPath(), WINDOWS_PATH);

  const string kPastisUri = "diskfile:/dir/file.vrs?dealer=pastis";
  EXPECT_EQ(spec.fromPathJsonUri(kPastisUri), 0);
  EXPECT_EQ(spec.uri, kPastisUri);
  EXPECT_EQ(spec.fileHandlerName, "diskfile");
  chunks = {"/dir/file.vrs"};
  EXPECT_EQ(spec.chunks, chunks);
  EXPECT_TRUE(spec.chunkSizes.empty());
  EXPECT_EQ(spec.fileName, "");
  EXPECT_FALSE(spec.hasChunkSizes());
  EXPECT_EQ(spec.getFileSize(), -1);
  EXPECT_EQ(spec.getSourceLocation(), "diskfile");
  json =
      "{\"chunks\":[\"/dir/file.vrs\"],\"storage\":\"diskfile\",\"source_uri\":\"diskfile"
      ":/dir/file.vrs?dealer=pastis\",\"dealer\":\"pastis\"}";
  EXPECT_EQ(spec.toJson(), json);
  EXPECT_EQ(spec.getEasyPath(), kPastisUri);

  // remove mention of the delegator
  spec.uri = "diskfile:/dir/file.vrs";
  EXPECT_EQ(spec.getEasyPath(), "diskfile:/dir/file.vrs with delegator dealer=pastis");

  spec.fileName = "good_stuff.vrs";
  EXPECT_EQ(
      spec.getEasyPath(),
      "uri: diskfile:/dir/file.vrs with delegator dealer=pastis, name: good_stuff.vrs");
}
