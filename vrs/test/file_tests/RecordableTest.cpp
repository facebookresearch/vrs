// (c) Facebook, Inc. and its affiliates. Confidential and proprietary.

#include <random>
#include <thread>

#include <gtest/gtest.h>

#include <portability/Filesystem.h>
#include <test_helpers/GTestMacros.h>
#include <vrs/RecordFileReader.h>
#include <vrs/RecordFileWriter.h>
#include <vrs/Recordable.h>
#include <vrs/StreamPlayer.h>
#include <vrs/os/Time.h>
#include <vrs/os/Utils.h>

using namespace std;
using namespace vrs;

namespace {

// Frame 0 is a random noise, just large enough to make that we attempt to compress it
// leading to compressed data larger than the source data
static const uint32_t kFrame0Size = 320 * 240;
static vector<uint8_t> sFrame0;

// Compress using different presets
static std::vector<vrs::CompressionPreset> sCompression = {
    vrs::CompressionPreset::Lz4Fast,
    vrs::CompressionPreset::ZstdFast,
    vrs::CompressionPreset::ZstdLight};

static size_t sCompressionIndex = 0;

void initFrame0() {
  std::default_random_engine generator;
  std::uniform_int_distribution<uint32_t> distribution;
  sFrame0.resize(kFrame0Size);
  for (uint32_t k = 0; k < kFrame0Size; k++) {
    sFrame0[k] = static_cast<uint8_t>(distribution(generator));
  }
}

#define FOUR_CHAR_CODE(a, b, c, d)                                                                 \
  (static_cast<uint32_t>(a) | (static_cast<uint32_t>(b) << 8) | (static_cast<uint32_t>(c) << 16) | \
   (static_cast<uint32_t>(d) << 24))

class Recordable_2 : public Recordable, StreamPlayer {
  const uint32_t kStateVersion = 1;
  const uint32_t kConfigurationVersion = 1;
  const uint32_t kDataVersion = 1;

 public:
  Recordable_2() : Recordable(RecordableTypeId::UnitTest2) {
    setCompression(sCompression[sCompressionIndex++ % sCompression.size()]);
  }
  void createDataRecord(double timestamp) {
    createRecord(timestamp, Record::Type::DATA, kDataVersion);
  }
  const Record* createStateRecord() override {
    return createRecord(-1, Record::Type::STATE, kStateVersion);
  }
  const Record* createConfigurationRecord() override {
    return createRecord(-2, Record::Type::CONFIGURATION, kConfigurationVersion);
  }
  bool processStateHeader(const CurrentRecord&, DataReference&) override {
    GTEST_NONFATAL_FAILURE_("unexpected read");
    return false;
  }
  bool processConfigurationHeader(const CurrentRecord&, DataReference&) override {
    GTEST_NONFATAL_FAILURE_("unexpected read");
    return false;
  }
  bool processDataHeader(const CurrentRecord&, DataReference&) override {
    GTEST_NONFATAL_FAILURE_("unexpected read");
    return false;
  }
};

class RecordableTest : public Recordable, StreamPlayer {
  const uint32_t kFrameWidth = 320;
  const uint32_t kFrameHeight = 240;
  const uint32_t kFrameRate = 15000;
  const uint32_t kFrameCount = 150;
  const uint32_t kFrameSaveFrequency = 50; // save every X frames
  const uint32_t kThreadCount = thread::hardware_concurrency();
  const double kPrerollTime = 0.5;
  const uint32_t kRecordable2RecordFrameFrequency =
      10; // Save one frame every X frame of the other type

  const uint32_t kStateVersion = FOUR_CHAR_CODE('S', 't', 'a', 't');
  const uint32_t kConfigurationVersion = FOUR_CHAR_CODE('C', 'o', 'n', 'f');
  const uint32_t kDataVersion = FOUR_CHAR_CODE('D', 'a', 't', 'a');

  Recordable_2 otherRecordable_;

  const IndexRecord::RecordInfo* expectedRecord_ = {};
  bool expectRecordReadMetaDataOnly = false;
  int expectedFound = {};
  RecordFormat configurationFormat{ContentType::DATA_LAYOUT};
  RecordFormat stateFormat{ContentType::EMPTY};
  RecordFormat dataFormat1{"custom/size=50+image/raw/200x300/pixel=bgr8"};
  RecordFormat dataFormat2{"data_layout/size=10+image/raw/10x20/pixel=grey8"};

 public:
  RecordableTest() : Recordable(RecordableTypeId::UnitTest1) {
    setCompression(sCompression[sCompressionIndex++ % sCompression.size()]);
  }

  const Record* createStateRecord() override {
    // "old" timestamp to force testing out of order records system!
    return createRecord(-1, Record::Type::STATE, kStateVersion);
  }

  const Record* createConfigurationRecord() override {
    // "old" timestamp to force testing out of order records system!
    return createRecord(-2, Record::Type::CONFIGURATION, kConfigurationVersion);
  }

  const Record* createFrame(uint32_t frameNumber) {
    uint32_t frameSize = getSizeOfFrame(frameNumber);
    vector<uint8_t> buffer(frameSize);
    for (uint32_t n = 0; n < frameSize; ++n) {
      buffer[n] = getByteOfFrame(frameNumber, n);
    }
    return createRecord(
        getFrameTimestamp(frameNumber),
        Record::Type::DATA,
        kDataVersion,
        DataSource(frameNumber, buffer));
  }

  bool processStateHeader(const CurrentRecord& record, DataReference&) override {
    if (expectedRecord_) {
      checkExpectedRecord(record, Record::Type::STATE);
    } else {
      EXPECT_LE(lastTimestamp_, record.timestamp);
    }
    lastTimestamp_ = record.timestamp;
    EXPECT_EQ(record.formatVersion, kStateVersion);
    EXPECT_EQ(record.recordSize, 0);
    return false;
  }

  void processState(const CurrentRecord&, uint32_t) override {
    FAIL();
  }

  bool processConfigurationHeader(const CurrentRecord& record, DataReference&) override {
    if (expectedRecord_) {
      checkExpectedRecord(record, Record::Type::CONFIGURATION);
    } else {
      EXPECT_LE(lastTimestamp_, record.timestamp);
    }
    lastTimestamp_ = record.timestamp;
    EXPECT_EQ(record.formatVersion, kConfigurationVersion);
    EXPECT_EQ(record.recordSize, 0);
    return false;
  }

  void processConfiguration(const CurrentRecord&, uint32_t) override {
    FAIL();
  }

  bool processDataHeader(const CurrentRecord& record, DataReference& outDataReference) override {
    if (expectedRecord_) {
      checkExpectedRecord(record, Record::Type::DATA);
    } else {
      EXPECT_LE(lastTimestamp_, record.timestamp);
    }
    lastTimestamp_ = record.timestamp;
    EXPECT_EQ(record.formatVersion, kDataVersion);
    if (expectedRecord_ && expectRecordReadMetaDataOnly) {
      outDataReference.useObject(frameNumber_);
    } else {
      // handles init & data increase situations!
      uint32_t bufferSize = record.recordSize - sizeof(frameNumber_);
      if (bufferSize > readBuffer_.size()) {
        readBuffer_.resize(bufferSize);
      }
      outDataReference.useObject(frameNumber_, readBuffer_.data(), bufferSize);
    }
    return true;
  }

  void processData(const CurrentRecord& record, uint32_t bytesWrittenCount) override {
    checkExpectedRecord(record, Record::Type::DATA);
    EXPECT_EQ(getFrameTimestamp(frameNumber_), record.timestamp);
    uint32_t frameSize = bytesWrittenCount - sizeof(uint32_t);
    if (expectedRecord_ && expectRecordReadMetaDataOnly) {
      EXPECT_EQ(frameSize, 0);
    } else {
      EXPECT_EQ(frameSize, getSizeOfFrame(frameNumber_));
      for (uint32_t n = 0; n < frameSize; ++n) {
        EXPECT_EQ(readBuffer_.at(n), getByteOfFrame(frameNumber_, n));
      }
    }
  }

  static const char* getTag(const map<string, string>& tags, std::string name) {
    auto iter = tags.find(name);
    if (iter != tags.end()) {
      return iter->second.c_str();
    }
    return "";
  }

#define TAG_EQU(_tags, _name, _value) EXPECT_STREQ(getTag(_tags, _name), _value)

  int readAllRecords(const string& fileName) {
    RecordFileReader filePlayer;
    RETURN_ON_FAILURE(filePlayer.openFile(fileName));
    EXPECT_TRUE(filePlayer.hasIndex());

    // Check the file's tags
    const map<string, string>& fileTags = filePlayer.getTags();
    EXPECT_EQ(fileTags.size(), 3);
    auto iter = fileTags.find("fileTag1");
    EXPECT_NE(iter, fileTags.end());
    EXPECT_EQ(iter->second.compare("fileValue1"), 0);
    iter = fileTags.find("fileTag2");
    EXPECT_EQ(iter->second.compare("fileValue2"), 0);
    iter = fileTags.find("emptyTag");
    EXPECT_EQ(iter->second.compare(""), 0);

    // Check the stream's tags
    const set<StreamId>& streamIds = filePlayer.getStreams();
    EXPECT_EQ(streamIds.size(), 1);
    StreamId id = *streamIds.begin();
    EXPECT_EQ(id.getTypeId(), RecordableTypeId::UnitTest1);
    filePlayer.setStreamPlayer(id, this);
    string name = filePlayer.getOriginalRecordableTypeName(id);
    EXPECT_STREQ(name.c_str(), id.getTypeName().c_str());
    const map<string, string>& streamTags = filePlayer.getTags(id).user;
    EXPECT_EQ(streamTags.size(), 2);
    TAG_EQU(streamTags, "streamTag1", "tagValue1");
    TAG_EQU(streamTags, "streamTag2", "tagValue2");

    // Test a second time, to see if cached values work
    name = filePlayer.getOriginalRecordableTypeName(id);
    EXPECT_STREQ(name.c_str(), id.getTypeName().c_str());
    EXPECT_EQ(filePlayer.getTags(id).user.size(), 2);
    EXPECT_STREQ(filePlayer.getTag(id, "streamTag1").c_str(), "tagValue1");
    EXPECT_STREQ(filePlayer.getTag(id, "streamTag2").c_str(), "tagValue2");

    EXPECT_EQ(filePlayer.getRecordCount(), kFrameCount + 2);
    EXPECT_EQ(filePlayer.getRecordCount(id), kFrameCount + 2);
    EXPECT_EQ(filePlayer.readAllRecords(), 0);
    return filePlayer.closeFile();
  }

  void checkExpectedRecord(const CurrentRecord& record, Record::Type type) {
    if (expectedRecord_ != nullptr) {
      EXPECT_EQ(record.streamId, expectedRecord_->streamId);
      double erts = expectedRecord_->timestamp; // make ubsan happy, not sure why...
      EXPECT_EQ(record.timestamp, erts);
      EXPECT_EQ(type, expectedRecord_->recordType);
      expectedFound++;
    }
  }

  int readTwoRecordablesByIndex(const string& fileName) {
    RecordFileReader filePlayer;
    RETURN_ON_FAILURE(filePlayer.openFile(fileName));
    EXPECT_TRUE(filePlayer.hasIndex());
    const set<StreamId>& streamIds = filePlayer.getStreams();
    EXPECT_EQ(streamIds.size(), 2);

    // test getStreamForType()
    StreamId recordable1 = filePlayer.getStreamForType(RecordableTypeId::UnitTest1, 0);
    EXPECT_TRUE(recordable1.isValid());
    StreamId recordable2 = filePlayer.getStreamForType(RecordableTypeId::UnitTest2, 0);
    EXPECT_TRUE(recordable2.isValid());
    StreamId badRecordable = filePlayer.getStreamForType(RecordableTypeId::UnitTest2, 1);
    EXPECT_TRUE(!badRecordable.isValid());
    badRecordable = filePlayer.getStreamForType(RecordableTypeId::Cv1Camera, 0);

    // test getStreamForTag()
    EXPECT_EQ(
        filePlayer.getStreamForTag("streamTag1", "tagValue1", RecordableTypeId::UnitTest1),
        recordable1);
    EXPECT_EQ(filePlayer.getStreamForTag("streamTag1", "tagValue1"), recordable1);
    EXPECT_FALSE(filePlayer.getStreamForTag("streamTag1", "tagValue1", RecordableTypeId::UnitTest2)
                     .isValid());
    EXPECT_FALSE(filePlayer.getStreamForTag("streamTag1", "tagValue2", RecordableTypeId::UnitTest1)
                     .isValid());
    EXPECT_FALSE(filePlayer.getStreamForTag("streamTag1", "tagValue2").isValid());
    EXPECT_TRUE(!badRecordable.isValid());

    filePlayer.setStreamPlayer(recordable1, this);
    EXPECT_EQ(
        filePlayer.getRecordCount(recordable1),
        kFrameCount + 2); // number of records for our main stream only
    EXPECT_EQ(filePlayer.getRecordCount(recordable1, Record::Type::DATA), kFrameCount);
    EXPECT_EQ(filePlayer.getRecordCount(recordable1, Record::Type::CONFIGURATION), 1);
    EXPECT_EQ(filePlayer.getRecordCount(recordable1, Record::Type::STATE), 1);
    EXPECT_EQ(filePlayer.getRecordCount(recordable2, Record::Type::CONFIGURATION), 1);
    EXPECT_EQ(filePlayer.getRecordCount(recordable2, Record::Type::STATE), 1);
    uint32_t recordCount = filePlayer.getRecordCount();
    EXPECT_EQ(
        recordCount,
        filePlayer.getRecordCount(recordable1) + filePlayer.getRecordCount(recordable2));
    auto index = filePlayer.getIndex(recordable1);
    // Read records backwards, half of them full frame, half meta data only
    for (size_t k = index.size(); k > 0; k--) {
      expectedRecord_ = index[k - 1];
      expectRecordReadMetaDataOnly = (k % 2) != 0;
      expectedFound = 0;
      EXPECT_EQ(filePlayer.readRecord(*expectedRecord_), 0);
      if (expectedRecord_->recordType == Record::Type::DATA) {
        EXPECT_EQ(expectedFound, 2); // header + data
      } else {
        EXPECT_EQ(expectedFound, 1); // header only
      }
    }
    expectedRecord_ = nullptr;
    expectRecordReadMetaDataOnly = false;
    // Check tags of second stream
    const StreamTags& streamTags = filePlayer.getTags(recordable1);
    EXPECT_EQ(streamTags.user.size(), 2);
    TAG_EQU(streamTags.user, "streamTag1", "tagValue1");
    TAG_EQU(streamTags.user, "streamTag2", "tagValue2");
    EXPECT_EQ(streamTags.vrs.size(), 5); // name + 4 record formats
    RecordFormat format;
    filePlayer.getRecordFormat(recordable1, Record::Type::CONFIGURATION, 1, format);
    EXPECT_EQ(format, configurationFormat);
    filePlayer.getRecordFormat(recordable1, Record::Type::STATE, 1, format);
    EXPECT_EQ(format, stateFormat);
    filePlayer.getRecordFormat(recordable1, Record::Type::DATA, 1, format);
    EXPECT_EQ(format, dataFormat1);
    filePlayer.getRecordFormat(recordable1, Record::Type::DATA, 2, format);
    EXPECT_EQ(format, dataFormat2);
    RecordFormatMap formats;
    EXPECT_EQ(filePlayer.getRecordFormats(recordable1, formats), 4);
    EXPECT_EQ(formats[make_pair(Record::Type::CONFIGURATION, 1)], configurationFormat);
    EXPECT_EQ(formats[make_pair(Record::Type::STATE, 1)], stateFormat);
    EXPECT_EQ(formats[make_pair(Record::Type::DATA, 1)], dataFormat1);
    EXPECT_NE(formats[make_pair(Record::Type::DATA, 1)], dataFormat2);
    EXPECT_EQ(formats[make_pair(Record::Type::DATA, 2)], dataFormat2);
    EXPECT_NE(formats[make_pair(Record::Type::DATA, 3)], dataFormat1);
    EXPECT_NE(formats[make_pair(Record::Type::DATA, 3)], dataFormat2);
    const StreamTags& streamTags2 = filePlayer.getTags(recordable2);
    EXPECT_EQ(streamTags2.user.size(), 2);
    TAG_EQU(streamTags2.user, "tag2Tag1", "tag2Value1");
    TAG_EQU(streamTags2.user, "tag2Tag2", "tag2Value2");
    EXPECT_EQ(streamTags2.vrs.size(), 1); // name + 0 record formats
    filePlayer.getRecordFormat(recordable2, Record::Type::CONFIGURATION, 1, format);
    EXPECT_EQ(format, ContentType::CUSTOM);
    filePlayer.getRecordFormat(recordable2, Record::Type::STATE, 1, format);
    EXPECT_EQ(format, ContentType::CUSTOM);
    filePlayer.getRecordFormat(recordable2, Record::Type::DATA, 1, format);
    EXPECT_EQ(format, ContentType::CUSTOM);
    return filePlayer.closeFile();
  }

  int rebuildIndex(const string& fileName) {
    RecordFileReader filePlayer;
    RETURN_ON_FAILURE(filePlayer.openFile(fileName));
    EXPECT_TRUE(filePlayer.hasIndex());
    const set<StreamId> streamIds = filePlayer.getStreams(); // copy!
    const vector<IndexRecord::RecordInfo> writtenIndex = filePlayer.getIndex(); // copy!
    filePlayer.closeFile();

    // truncate the file to corrupt its index
    int64_t fileSize = os::getFileSize(fileName);
    ::filesystem::resize_file(fileName, static_cast<uintmax_t>(fileSize - 1));
    EXPECT_EQ(fileSize, os::getFileSize(fileName) + 1);

    RETURN_ON_FAILURE(filePlayer.openFile(fileName));
    EXPECT_FALSE(filePlayer.hasIndex());
    EXPECT_EQ(streamIds, filePlayer.getStreams());
    EXPECT_EQ(writtenIndex, filePlayer.getIndex());
    filePlayer.closeFile();
    EXPECT_EQ(fileSize, os::getFileSize(fileName) + 1); // no change

    RETURN_ON_FAILURE(filePlayer.openFile(fileName, true));
    EXPECT_TRUE(filePlayer.hasIndex());
    EXPECT_EQ(streamIds, filePlayer.getStreams());
    EXPECT_EQ(writtenIndex, filePlayer.getIndex());
    EXPECT_EQ(fileSize, os::getFileSize(fileName)); // same as before truncation
    return filePlayer.closeFile();
  }

  void sharedTestFileInit(RecordFileWriter& fileWriter) {
    setRecordableIsActive(true);
    fileWriter.addRecordable(this);
    setTag("streamTag1", "tagValue1");
    setTag("streamTag2", "tagValue2");
    fileWriter.setTag("fileTag1", "fileValue1");
    fileWriter.setTag("fileTag2", "fileValue2");
    fileWriter.setTag("emptyTag", "");
  }

  int noThreadCreateRecords(const string& fileName, size_t maxChunkSize) {
    RecordFileWriter fileWriter;
    sharedTestFileInit(fileWriter);
    createConfigurationRecord();
    createStateRecord();
    // create frames in non-linear order, to exercise sorting code a bit
    for (uint32_t frame = 0; frame < kFrameCount; ++frame) {
      if (frame % 2) {
        createFrame(frame);
      } else {
        createFrame(kFrameCount - frame);
      }
    }
    fileWriter.setMaxChunkSizeMB(maxChunkSize);
    return fileWriter.writeToFile(fileName);
  }

  struct Thread_param {
    RecordFileWriter* fileWriter;
    uint32_t threadIndex;
    atomic<int>* myCounter;
    atomic<int>* limitCounter;
  };

  void createRecordsThreadTask(Thread_param* param) {
    double startTime = os::getTimestampSec();
    for (uint32_t frame = 0; frame < kFrameCount; ++frame) {
      if ((frame % kThreadCount) == param->threadIndex) {
        createFrame(frame);
      }
      if (((frame + 1) % kFrameSaveFrequency) == 0) {
        int error = param->fileWriter->writeRecordsAsync(getFrameTimestamp(frame) - kPrerollTime);
        if (error != 0) {
          EXPECT_EQ(error, 0);
          break;
        }
        double wallDuration = os::getTimestampSec() - startTime;
        double frameDuration = static_cast<double>(frame) / kFrameRate;
        if (wallDuration < frameDuration) {
          const double sleepDuration = frameDuration - wallDuration;
          std::this_thread::sleep_for(std::chrono::duration<double>(sleepDuration));
        }
      }
      // each thread has its own counter, and checks that it is not too far ahead of another thread,
      // which could lead to records being written out of order and fail the test
      param->myCounter->operator++();
      while (param->myCounter->load() > param->limitCounter->load() + 5) {
        this_thread::yield();
      }
    }
  }

  int threadedCreateRecords(const string& fileName, size_t compressPoolSize, size_t maxChunkSize) {
    setCompression(compressPoolSize > 1 ? CompressionPreset::Lz4Tight : CompressionPreset::Lz4Fast);
    RecordFileWriter fileWriter;
    fileWriter.setCompressionThreadPoolSize(compressPoolSize);
    sharedTestFileInit(fileWriter);
    fileWriter.setMaxChunkSizeMB(maxChunkSize);
    RETURN_ON_FAILURE(fileWriter.createFileAsync(fileName));
    atomic<int>* counters = new atomic<int>[kThreadCount];
    vector<Thread_param> threadParams;
    for (uint32_t threadIndex = 0; threadIndex < kThreadCount; threadIndex++) {
      counters[threadIndex] = 0;
      threadParams.push_back(Thread_param{
          &fileWriter,
          threadIndex,
          counters + threadIndex,
          counters + ((threadIndex + 1) % kThreadCount)});
    }
    vector<thread> threads;
    for (uint32_t threadIndex = 0; threadIndex < kThreadCount; threadIndex++) {
      threads.push_back(
          thread{&RecordableTest::createRecordsThreadTask, this, &threadParams[threadIndex]});
    }
    for (uint32_t threadIndex = 0; threadIndex < kThreadCount; threadIndex++) {
      threads[threadIndex].join();
    }
    EXPECT_EQ(fileWriter.closeFileAsync(), 0);
    threads.clear();
    int status = fileWriter.waitForFileClosed();
    delete[] counters;
    return status;
  }

  int createAndWriteTwoRecordablesAsync(const string& fileName) {
    setRecordableIsActive(true);
    setTag("streamTag1", "tagValue1");
    setTag("streamTag2", "tagValue2");
    this->addRecordFormat(Record::Type::CONFIGURATION, 1, configurationFormat);
    this->addRecordFormat(Record::Type::STATE, 1, stateFormat);
    this->addRecordFormat(Record::Type::DATA, 1, dataFormat1);
    this->addRecordFormat(Record::Type::DATA, 2, dataFormat2);
    RecordFileWriter fileWriter;
    fileWriter.addRecordable(this);
    otherRecordable_.setTag("tag2Tag1", "tag2Value1");
    otherRecordable_.setTag("tag2Tag2", "tag2Value2");
    fileWriter.setTag("fileTag1", "fileValue1");
    fileWriter.setTag("fileTag2", "fileValue2");
    fileWriter.setTag("emptyTag", "");
    RETURN_ON_FAILURE(fileWriter.createFileAsync(fileName));
    double startTime = os::getTimestampSec();
    for (uint32_t frame = 0; frame < kFrameCount; ++frame) {
      createFrame(frame);
      if (((frame + 1) % kRecordable2RecordFrameFrequency) == 0) {
        otherRecordable_.createDataRecord(getFrameTimestamp(frame));
      }
      if (((frame + 1) % kFrameSaveFrequency) == 0) {
        RETURN_ON_FAILURE(fileWriter.writeRecordsAsync(getFrameTimestamp(frame) - kPrerollTime));
      }
      double wallDuration = os::getTimestampSec() - startTime;
      double frameDuration = static_cast<double>(frame) / kFrameRate;
      if (wallDuration < frameDuration) {
        const double sleepDuration = frameDuration - wallDuration;
        std::this_thread::sleep_for(std::chrono::duration<double>(sleepDuration));
      }
      // Add second recordable late, to force the use of a tags record
      if (frame == (kFrameCount + 2) / 3) {
        fileWriter.writeRecordsAsync(getFrameTimestamp(frame));
        fileWriter.addRecordable(&otherRecordable_);
      }
    }
    return fileWriter.waitForFileClosed();
  }

  int createShortFile(RecordFileWriter& fileWriter, const string& fileName) {
    fileWriter.addRecordable(this);
    fileWriter.createFileAsync(fileName);
    createFrame(0);
    return fileWriter.waitForFileClosed();
  }

  void checkShortFile(const string& fileName) {
    RecordFileReader file;
    EXPECT_EQ(file.openFile(fileName), 0);
    EXPECT_EQ(file.getStreams().size(), 1);
    EXPECT_EQ(file.getIndex().size(), 3);
    file.closeFile();
  }

 private:
  uint32_t getSizeOfFrame(uint32_t frameNumber) const {
    if (frameNumber == 0) {
      return kFrame0Size;
    }
    return (frameNumber % 2) ? kFrameWidth * kFrameHeight + (frameNumber % 200)
                             : kFrameWidth * kFrameHeight - (frameNumber % 200);
  }

  uint8_t getByteOfFrame(uint32_t frameNumber, uint32_t byteNumber) {
    if (frameNumber == 0) {
      return sFrame0[byteNumber];
    }
    return sFrame0[(frameNumber ^ (13 * byteNumber)) % sFrame0.size()];
  }

  double getFrameTimestamp(uint32_t frameNumber) {
    return static_cast<double>(frameNumber) / kFrameRate;
  }

 private:
  string typeName_;
  uint32_t frameNumber_;
  vector<uint8_t> readBuffer_;
  double lastTimestamp_ = -DBL_MAX;
};

struct RecordableTester : testing::Test {
  static void SetUpTestCase() {
    // Make sure sFrame0 is initialized before threads start
    initFrame0();
    // arvr::logging::Channel::setGlobalLevel(arvr::logging::Level::Debug);
  }

  RecordableTest recordable;
};

} // namespace

static size_t deleteEveryChunk(const string& path) {
  RecordFileReader file;
  EXPECT_EQ(file.openFile(path), 0);
  vector<std::pair<string, int64_t>> chunks = file.getFileChunks();
  file.closeFile();
  for (auto& chunk : chunks) {
    os::remove(chunk.first);
  }
  return chunks.size();
}

TEST_F(RecordableTester, noTreadCreateAndReadRecords) {
  string testFilePath = os::getTempFolder() + "RecordableTest-a.vrs";

  ASSERT_EQ(recordable.noThreadCreateRecords(testFilePath, 1), 0);
  EXPECT_EQ(recordable.readAllRecords(testFilePath), 0);
  EXPECT_EQ(deleteEveryChunk(testFilePath), 12);
}

TEST_F(RecordableTester, threadedCreateAndReadRecords_0) {
  string testFilePath = os::getTempFolder() + "RecordableTest-b.vrs";

  ASSERT_EQ(recordable.threadedCreateRecords(testFilePath, 0, 2), 0);
  EXPECT_EQ(recordable.readAllRecords(testFilePath), 0);
  EXPECT_EQ(deleteEveryChunk(testFilePath), 6);
}

TEST_F(RecordableTester, threadedCreateAndReadRecords_1) {
  string testFilePath = os::getTempFolder() + "RecordableTest-c.vrs";

  ASSERT_EQ(recordable.threadedCreateRecords(testFilePath, 1, 3), 0);
  EXPECT_EQ(recordable.readAllRecords(testFilePath), 0);
  EXPECT_EQ(deleteEveryChunk(testFilePath), 4);
}

TEST_F(RecordableTester, threadedCreateAndReadRecords_HW) {
  string testFilePath = os::getTempFolder() + "RecordableTest-d.vrs";

  ASSERT_EQ(
      recordable.threadedCreateRecords(testFilePath, RecordFileWriter::kMaxThreadPoolSizeForHW, 2),
      0);
  EXPECT_EQ(recordable.readAllRecords(testFilePath), 0);
  EXPECT_EQ(deleteEveryChunk(testFilePath), 6);
}

TEST_F(RecordableTester, createWriteReadAndRebuildIndexTwoRecordablesAsync) {
  string testFilePath = os::getTempFolder() + "RecordableTest-e.vrs";

  ASSERT_EQ(recordable.createAndWriteTwoRecordablesAsync(testFilePath), 0);
  EXPECT_EQ(recordable.readTwoRecordablesByIndex(testFilePath), 0);
  EXPECT_EQ(recordable.rebuildIndex(testFilePath), 0);
  EXPECT_EQ(deleteEveryChunk(testFilePath), 1);
}

TEST_F(RecordableTester, ReuseRecordFileWriter) {
  string testFilePath = os::getTempFolder() + "RecordableTest-f.vrs";
  RecordFileWriter fileWriter;
  ASSERT_EQ(recordable.createShortFile(fileWriter, testFilePath), 0);
  recordable.checkShortFile(testFilePath);
  ASSERT_EQ(recordable.createShortFile(fileWriter, testFilePath), 0);
  recordable.checkShortFile(testFilePath);
}
