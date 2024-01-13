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
#include <cstddef>
#include <cstdlib>
#include <map>
#include <memory>
#include <string>

#include <fmt/format.h>
#include <gtest/gtest.h>

#define DEFAULT_LOG_CHANNEL "MultiRecordFileReaderTest"
#include <logging/Checks.h>
#include <logging/Log.h>
#include <vrs/DataLayout.h>
#include <vrs/DataPieces.h>
#include <vrs/ErrorCode.h>
#include <vrs/MultiRecordFileReader.h>
#include <vrs/RecordFileWriter.h>
#include <vrs/StreamId.h>
#include <vrs/StreamPlayer.h>
#include <vrs/TagConventions.h>
#include <vrs/os/Utils.h>

using namespace vrs;
using namespace std;

using UniqueStreamId = vrs::MultiRecordFileReader::UniqueStreamId;

static double getTimestampSec() {
  using namespace chrono;
  return duration_cast<duration<double>>(steady_clock::now().time_since_epoch()).count();
}

static bool isTimestampLE(const IndexRecord::RecordInfo* lhs, const IndexRecord::RecordInfo* rhs) {
  return lhs == rhs || lhs->timestamp <= rhs->timestamp;
}

class MyMetadata : public AutoDataLayout {
 public:
  DataPieceValue<uint32_t> sensorValue{"my_sensor"};

  AutoDataLayoutEnd endLayout;
};

constexpr auto kTestRecordableTypeId = RecordableTypeId::UnitTestRecordableClass;
constexpr const char* kTestFlavor = "team/vrs/test/multi-test";

class TestRecordable : public Recordable {
  static const uint32_t kDataRecordFormatVersion = 1;

 public:
  TestRecordable() : Recordable(kTestRecordableTypeId, kTestFlavor) {
    // define your RecordFormat & DataLayout definitions for this stream
    addRecordFormat(
        Record::Type::DATA, // the type of records this definition applies to
        kDataRecordFormatVersion, // a record format version
        metadata_.getContentBlock(), // the RecordFormat definition
        {&metadata_}); // the DataLayout definition for the datalayout content block declared above.
  }

  const Record* createConfigurationRecord() override {
    double someTimeInSec = getTimestampSec();
    return createRecord(someTimeInSec, Record::Type::CONFIGURATION, 0);
  }

  const Record* createStateRecord() override {
    double someTimeInSec = getTimestampSec();
    return createRecord(someTimeInSec, Record::Type::STATE, 0);
  }

  const Record* createData(double timestamp, uint32_t sensorValue) {
    metadata_.sensorValue.set(sensorValue); // Record the value we want to save in the record
    return createRecord(
        timestamp, Record::Type::DATA, kDataRecordFormatVersion, DataSource(metadata_));
  }

  void createRandomData() {
    uint32_t sensorValue = static_cast<uint32_t>(rand());
    this->createData(getTimestampSec(), sensorValue);
  }

  const Record* createDefaultRecord(Record::Type type) {
    double someTimeInSec = getTimestampSec();
    return createRecord(someTimeInSec, type, 0);
  }

 private:
  MyMetadata metadata_;
};

struct VrsFileBuilder {
  explicit VrsFileBuilder(string path) : path_{std::move(path)} {
    XR_CHECK_FALSE(os::isFile(path_));
    fileWriter_.addRecordable(&recordable_);
    recordable_.setRecordableIsActive(true);
    recordable_.createConfigurationRecord();
    recordable_.createStateRecord();
  }

  void build() {
    XR_CHECK_FALSE(os::isFile(path_));
    auto result = fileWriter_.writeToFile(path_);
    XR_CHECK_EQ(SUCCESS, result);
    XR_LOGI("Created VRS File successfully: {}", path_);
  }

  TestRecordable recordable_;
  RecordFileWriter fileWriter_;
  const string path_;
};

const auto kDefaultSessionId = "TestSessionId";
const auto kDefaultCaptureTimeEpoch = "12345";
const map<string, string>& getDefaultTags() {
  static const map<string, string>& defaultTags{
      {tag_conventions::kSessionId, kDefaultSessionId},
      {tag_conventions::kCaptureTimeEpoch, kDefaultCaptureTimeEpoch}};
  return defaultTags;
};

void createVRSFileSynchronously(
    const string& path,
    const size_t numRandomDataRecords,
    const map<string, string>& fileTags = {},
    const map<string, string>& streamTags = {}) {
  VrsFileBuilder fileBuilder(path);
  for (size_t i = 0; i < numRandomDataRecords; i++) {
    fileBuilder.recordable_.createRandomData();
  }
  for (const auto& [tagKey, tagValue] : fileTags) {
    fileBuilder.fileWriter_.setTag(tagKey, tagValue);
  }
  for (const auto& [tagKey, tagValue] : streamTags) {
    fileBuilder.recordable_.setTag(tagKey, tagValue);
  }
  fileBuilder.build();
}

string getOsTempPath(const string& path) {
  return os::pathJoin(os::getTempFolder(), path);
}

vector<string> getOsTempPaths(size_t n) {
  vector<string> paths;
  paths.reserve(n);
  auto timestamp = getTimestampSec();
  for (size_t i = 0; i < n; i++) {
    paths.push_back(
        getOsTempPath(fmt::format("MultiRecordFileReaderTest-{}-{}.vrs", timestamp, i)));
  }
  return paths;
}

void removeFiles(const vector<string>& paths) {
  for (const auto& path : paths) {
    os::remove(path);
  }
}

vector<unique_ptr<VrsFileBuilder>> createFileBuilders(const vector<string>& filePaths) {
  vector<unique_ptr<VrsFileBuilder>> fileBuilders;
  fileBuilders.reserve(filePaths.size());
  for (const auto& path : filePaths) {
    fileBuilders.emplace_back(make_unique<VrsFileBuilder>(path));
  }
  return fileBuilders;
}

void assertEmptyStreamTags(const MultiRecordFileReader& reader, UniqueStreamId stream) {
  const auto& streamTags = reader.getTags(stream);
  ASSERT_TRUE(streamTags.user.empty());
}

void assertEmptyStreamTags(const MultiRecordFileReader& reader) {
  for (auto stream : reader.getStreams()) {
    assertEmptyStreamTags(reader, stream);
  }
}

class TestStreamPlayer : public StreamPlayer {
 public:
  bool processRecordHeader(const CurrentRecord&, DataReference&) override {
    return true;
  }

  void processRecord(const CurrentRecord& record, uint32_t) override {
    lastRecord = {record.timestamp, record.streamId, record.recordType};
    recordsProcessedCount++;
  }

  void validateLastRecord(const IndexRecord::RecordInfo* expectedRecord) {
    ASSERT_EQ(
        (RecordSignature{
            expectedRecord->timestamp, expectedRecord->streamId, expectedRecord->recordType}),
        lastRecord);
  }

  uint32_t getRecordsProcessedCount() {
    return recordsProcessedCount;
  }

 private:
  using RecordSignature = tuple<double, StreamId, Record::Type>;
  RecordSignature lastRecord;
  uint32_t recordsProcessedCount = 0;
};

class MultiRecordFileReaderTest : public testing::Test {};

TEST_F(MultiRecordFileReaderTest, invalidFilePaths) {
  const auto invalidPath = "invalidPath";
  ASSERT_NE(SUCCESS, MultiRecordFileReader().open(invalidPath));
  const auto invalidFileSpec = FileSpec({invalidPath});
  ASSERT_NE(SUCCESS, MultiRecordFileReader().open(invalidFileSpec));
  ASSERT_NE(SUCCESS, MultiRecordFileReader().open(vector<string>{"invalidPath1", "invalidPath2"}));
  ASSERT_NE(SUCCESS, MultiRecordFileReader().open(vector<string>{}));
}

TEST_F(MultiRecordFileReaderTest, relatedFiles) {
  constexpr auto relatedFileCount = 6;
  constexpr auto numRecords = 4;
  const auto relatedFilePaths = getOsTempPaths(relatedFileCount);
  // Use either empty or default (same) tag values for these files so they are considered related
  for (size_t i = 0; i < relatedFilePaths.size(); i++) {
    if (i % 2 == 0) {
      createVRSFileSynchronously(relatedFilePaths[i], numRecords);
    } else {
      createVRSFileSynchronously(relatedFilePaths[i], numRecords, getDefaultTags());
    }
  }
  MultiRecordFileReader reader;
  vector<FileSpec> relatedFileSpecs;
  relatedFileSpecs.reserve(relatedFilePaths.size());
  for (const auto& path : relatedFilePaths) {
    relatedFileSpecs.push_back(FileSpec({path}));
  }
  ASSERT_EQ(SUCCESS, reader.open(relatedFileSpecs));
  ASSERT_EQ(getDefaultTags(), reader.getTags());
  // Test getTag() for fileTags
  const auto tagNameValuePair = getDefaultTags().cbegin();
  ASSERT_EQ(tagNameValuePair->second, reader.getTag(tagNameValuePair->first));
  ASSERT_TRUE(reader.getTag("unkownTag").empty());
  // Now add an unrelated file path to the mix to make sure we are not able to open unrelated files
  const auto unrelatedFilePath =
      getOsTempPath(fmt::format("UnrelatedPath{}.vrs", relatedFilePaths.size()));
  auto mismatchingTags = getDefaultTags();
  // Modify the value of one of kRelatedFileTags to make this file seem unrelated
  mismatchingTags[MultiRecordFileReader::kRelatedFileTags[0]].append("_unrelated");
  createVRSFileSynchronously(unrelatedFilePath, numRecords, mismatchingTags);
  auto unrelatedFilePaths = relatedFilePaths;
  unrelatedFilePaths.push_back(unrelatedFilePath);
  ASSERT_NE(SUCCESS, MultiRecordFileReader().open(unrelatedFilePaths));
  removeFiles(unrelatedFilePaths);
}

vector<double> getNonDecreasingTimestamps(
    const size_t count,
    const double startTimestamp = 0,
    const size_t maxIncrement = 10) {
  vector<double> timestamps;
  timestamps.reserve(count);
  double timestamp = startTimestamp;
  while (timestamps.size() < count) {
    timestamps.emplace_back(timestamp);
    timestamp += (rand() % maxIncrement);
  }
  return timestamps;
}

TEST_F(MultiRecordFileReaderTest, multiFile) {
  const auto expectedTimestamps = getNonDecreasingTimestamps(50);
  const auto filePaths = getOsTempPaths(4);
  auto fileBuilders = createFileBuilders(filePaths);
  // Spray these timestamps in the form of data records across these VRS files
  for (const auto& timestamp : expectedTimestamps) {
    const auto builderIndex = rand() % fileBuilders.size();
    fileBuilders[builderIndex]->recordable_.createData(timestamp, timestamp);
  }
  for (auto& builder : fileBuilders) {
    builder->build();
  }
  MultiRecordFileReader reader;
  ASSERT_FALSE(reader.isOpened());
  ASSERT_EQ(SUCCESS, reader.open(filePaths));
  ASSERT_TRUE(reader.isOpened());
  ASSERT_LT(0, reader.getTotalSourceSize());
  assertEmptyStreamTags(reader);
  TestStreamPlayer streamPlayer;
  for (auto stream : reader.getStreams()) {
    reader.setStreamPlayer(stream, &streamPlayer);
    // validate serial numbers
    EXPECT_EQ(stream, reader.getStreamForSerialNumber(reader.getSerialNumber(stream)));
  }
  // Validate that Data Record timestamps match with expectedTimestamps
  auto timestampIt = expectedTimestamps.cbegin();
  auto recordIt = reader.recordIndex_->cbegin();
  while (timestampIt != expectedTimestamps.cend() && recordIt != reader.recordIndex_->cend()) {
    const auto& record = (*recordIt);
    reader.readRecord(*record);
    streamPlayer.validateLastRecord(record);
    if (record->recordType != Record::Type::DATA) {
      recordIt++;
      continue;
    }
    ASSERT_EQ(*timestampIt, record->timestamp)
        << fmt::format("expectedTimestamps: {}", fmt::join(expectedTimestamps, ", "));
    timestampIt++;
    recordIt++;
  }
  ASSERT_TRUE(timestampIt == expectedTimestamps.cend())
      << fmt::format("Timestamp not found in index. Missing timestamp: {}", *timestampIt);
  // Check for any extra records in the index
  while (recordIt != reader.recordIndex_->cend()) {
    ASSERT_TRUE((*recordIt)->recordType != Record::Type::DATA) << fmt::format(
        "Extra record found in index. Unexpected Record timestamp: {}", (*recordIt)->timestamp);
    recordIt++;
  }
  // Validate getRecordIndex(), getReader(), getRecordByTime(timestamp)
  const auto& recordIndex = *reader.recordIndex_;
  for (size_t index = 0; index < reader.getRecordCount(); index++) {
    const auto* record = recordIndex[index];
    ASSERT_EQ(index, reader.getRecordIndex(record));
    ASSERT_NE(nullptr, reader.getReader(record));
    ASSERT_TRUE(isTimestampLE(reader.getRecordByTime(record->timestamp), record));
  }
  const double lastTimestamp = reader.getRecord(reader.getRecordCount() - 1)->timestamp;
  ASSERT_EQ(nullptr, reader.getRecordByTime(lastTimestamp + 10));
  // Validate getRecordByTime(stream, timestamp)
  for (auto stream : reader.getStreams()) {
    const auto& streamIndex = reader.getIndex(stream);
    const size_t position = streamIndex.size() / 2;
    const IndexRecord::RecordInfo* record = streamIndex[position];
    ASSERT_TRUE(isTimestampLE(reader.getRecordByTime(stream, record->timestamp), record));
    ASSERT_TRUE(isTimestampLE(
        reader.getRecordByTime(stream, record->timestamp - numeric_limits<double>::epsilon()),
        record));
  }
  ASSERT_EQ(reader.getRecordCount(), reader.getRecordIndex(nullptr));
  ASSERT_EQ(nullptr, reader.getReader(nullptr));
  IndexRecord::RecordInfo unknownRecord;
  ASSERT_EQ(reader.getRecordCount(), reader.getRecordIndex(&unknownRecord));
  ASSERT_EQ(nullptr, reader.getReader(&unknownRecord));
  // Validate readFirstConfigurationRecord()
  auto stream0 = *reader.getStreams().cbegin();
  TestStreamPlayer stream0Player;
  reader.setStreamPlayer(stream0, &stream0Player);
  reader.readFirstConfigurationRecord(stream0, &stream0Player);
  ASSERT_EQ(1, stream0Player.getRecordsProcessedCount());
  const auto* firstConfigRecord = reader.getRecord(stream0, Record::Type::CONFIGURATION, 0);
  stream0Player.validateLastRecord(firstConfigRecord);
  // Validate readFirstConfigurationRecords()
  TestStreamPlayer allStreamsPlayer1, allStreamsPlayer2;
  for (auto stream : reader.getStreams()) {
    reader.setStreamPlayer(stream, &allStreamsPlayer1);
    reader.setStreamPlayer(stream, &allStreamsPlayer2);
  }
  reader.readFirstConfigurationRecords(&allStreamsPlayer1);
  ASSERT_EQ(reader.getStreams().size(), allStreamsPlayer1.getRecordsProcessedCount());
  // Validate readFirstConfigurationRecordsForType()
  reader.readFirstConfigurationRecordsForType(
      RecordableTypeId::AccelerometerRecordableClass, &allStreamsPlayer2);
  ASSERT_EQ(0, allStreamsPlayer2.getRecordsProcessedCount())
      << "When the given RecordableTypeId does not match with any streams, no "
         "records should be processed.";
  reader.readFirstConfigurationRecordsForType(stream0.getTypeId(), &allStreamsPlayer2);
  ASSERT_EQ(reader.getStreams().size(), allStreamsPlayer2.getRecordsProcessedCount());
  // Validate close
  ASSERT_EQ(SUCCESS, reader.close());
  ASSERT_FALSE(reader.isOpened());
  removeFiles(filePaths);
}

TEST_F(MultiRecordFileReaderTest, singleFile) {
  const auto filePaths = getOsTempPaths(1);
  constexpr auto numConfigRecords = 1;
  constexpr auto numStateRecords = 1;
  constexpr auto numDataRecords = 14;
  constexpr auto numTotalRecords = numDataRecords + numStateRecords + numConfigRecords;
  const string expectedStreamTag("expectedStreamTag");
  const string expectedStreamTagValue("expectedStreamTagValue");
  const map<string, string> expectedStreamTags = {{expectedStreamTag, expectedStreamTagValue}};
  createVRSFileSynchronously(
      filePaths.front(), numDataRecords, getDefaultTags(), expectedStreamTags);
  MultiRecordFileReader reader;
  ASSERT_EQ(0, reader.getRecordCount());
  ASSERT_FALSE(reader.isOpened());
  ASSERT_EQ(SUCCESS, reader.open(filePaths));
  ASSERT_TRUE(reader.isOpened());
  ASSERT_LT(0, reader.getTotalSourceSize());
  ASSERT_EQ(getDefaultTags(), reader.getTags());
  // getStreams() validation
  const auto& streams = reader.getStreams();
  ASSERT_EQ(1, streams.size());
  ASSERT_EQ(numTotalRecords, reader.getRecordCount());
  // validate serial numbers
  for (const auto& streamId : streams) {
    EXPECT_EQ(streamId, reader.getStreamForSerialNumber(reader.getSerialNumber(streamId)));
  }
  const auto stream = *streams.begin();
  ASSERT_EQ(numTotalRecords, reader.getRecordCount(stream));
  ASSERT_EQ(numConfigRecords, reader.getRecordCount(stream, Record::Type::CONFIGURATION));
  ASSERT_EQ(numStateRecords, reader.getRecordCount(stream, Record::Type::STATE));
  ASSERT_EQ(numDataRecords, reader.getRecordCount(stream, Record::Type::DATA));
  TestStreamPlayer streamPlayer;
  reader.setStreamPlayer(stream, &streamPlayer);
  // getStreams(RecordableTypeId, flavor) validation
  ASSERT_EQ(0, reader.getStreams(RecordableTypeId::AccelerometerRecordableClass).size());
  ASSERT_EQ(1, reader.getStreams(RecordableTypeId::Undefined).size());
  ASSERT_EQ(1, reader.getStreams(kTestRecordableTypeId).size());
  ASSERT_EQ(0, reader.getStreams(kTestRecordableTypeId, "unknownFlavor").size());
  ASSERT_EQ(1, reader.getStreams(kTestRecordableTypeId, kTestFlavor).size());
  ASSERT_EQ(kTestFlavor, reader.getFlavor(stream));
  // getTags() and getStreamForTag() validation
  ASSERT_EQ(expectedStreamTags, reader.getTags(stream).user);
  ASSERT_EQ(stream, reader.getStreamForTag(expectedStreamTag, expectedStreamTagValue));
  ASSERT_FALSE(reader.getStreamForTag(expectedStreamTag, "unexpectedValue").isValid());
  // Unknown Stream Record Counts validation
  static const UniqueStreamId unknownStream;
  ASSERT_EQ(0, reader.getRecordCount(unknownStream));
  ASSERT_EQ(0, reader.getRecordCount(unknownStream, Record::Type::CONFIGURATION));
  // getRecord(), getRecordIndex(), readRecord() validation
  const IndexRecord::RecordInfo* firstRecord = reader.getRecord(0);
  ASSERT_EQ(firstRecord, reader.getRecord(stream, 0));
  ASSERT_EQ(firstRecord, reader.getRecord(stream, firstRecord->recordType, 0));
  ASSERT_NE(firstRecord, reader.getRecord(stream, Record::Type::UNDEFINED, 0));
  ASSERT_EQ(firstRecord, reader.getRecordByTime(stream, firstRecord->timestamp));
  EXPECT_EQ(firstRecord->streamId, reader.getUniqueStreamId(firstRecord));
  reader.readRecord(*firstRecord);
  streamPlayer.validateLastRecord(firstRecord);
  constexpr uint32_t indexToValidate = numTotalRecords / 2;
  const auto* record = reader.getRecord(indexToValidate);
  ASSERT_EQ(indexToValidate, reader.getRecordIndex(record));
  ASSERT_EQ(record, reader.getRecord(stream, indexToValidate));
  ASSERT_EQ(record, reader.getRecordByTime(stream, record->timestamp));
  ASSERT_EQ(
      record,
      reader.getRecordByTime(stream, record->timestamp - numeric_limits<double>::epsilon()));
  EXPECT_EQ(record->streamId, reader.getUniqueStreamId(record));
  ASSERT_EQ(nullptr, reader.getRecordByTime(unknownStream, record->timestamp));
  ASSERT_EQ(nullptr, reader.getRecord(numTotalRecords));
  ASSERT_EQ(nullptr, reader.getRecord(unknownStream, indexToValidate));
  ASSERT_EQ(nullptr, reader.getRecord(unknownStream, Record::Type::DATA, indexToValidate));
  ASSERT_EQ(nullptr, reader.getLastRecord(unknownStream, Record::Type::DATA));
  reader.readRecord(*record);
  streamPlayer.validateLastRecord(record);
  IndexRecord::RecordInfo unknownRecord;
  ASSERT_EQ(numTotalRecords, reader.getRecordIndex(&unknownRecord));
  ASSERT_EQ(numTotalRecords, reader.getIndex(stream).size());
  // getLastRecord() validation
  const IndexRecord::RecordInfo* lastRecord = reader.getRecord(numTotalRecords - 1);
  ASSERT_EQ(lastRecord, reader.getLastRecord(stream, lastRecord->recordType));
  ASSERT_EQ(nullptr, reader.getLastRecord(stream, Record::Type::UNDEFINED));
  // getRecordFormats() validation
  RecordFormatMap recordFormatMap;
  reader.getRecordFormats(stream, recordFormatMap);
  ASSERT_LT(0, recordFormatMap.size());
  reader.getRecordFormats(unknownStream, recordFormatMap);
  ASSERT_EQ(0, recordFormatMap.size());
  // Validation after closing
  ASSERT_EQ(SUCCESS, reader.close());
  ASSERT_FALSE(reader.isOpened());
  ASSERT_EQ(0, reader.getRecordCount());
  ASSERT_EQ(0, reader.getRecordCount(stream));
  ASSERT_EQ(0, reader.getRecordCount(stream, Record::Type::CONFIGURATION));
  ASSERT_EQ(0, reader.getRecordCount(stream, Record::Type::STATE));
  ASSERT_EQ(0, reader.getRecordCount(stream, Record::Type::DATA));
  assertEmptyStreamTags(reader, stream);
  assertEmptyStreamTags(reader);
  ASSERT_TRUE(reader.getStreams().empty());
  ASSERT_EQ(nullptr, reader.getRecord(stream, indexToValidate));
  ASSERT_EQ(nullptr, reader.getRecord(unknownStream, indexToValidate));
  ASSERT_EQ(nullptr, reader.getRecord(unknownStream, Record::Type::DATA, indexToValidate));
  ASSERT_EQ(nullptr, reader.getLastRecord(unknownStream, Record::Type::DATA));
  ASSERT_TRUE(reader.getIndex(stream).empty());
  reader.getRecordFormats(stream, recordFormatMap);
  ASSERT_EQ(0, recordFormatMap.size());
  reader.getRecordFormats(unknownStream, recordFormatMap);
  ASSERT_EQ(0, recordFormatMap.size());
  removeFiles(filePaths);
}

TEST_F(MultiRecordFileReaderTest, getFirstAndLastRecord) {
  // Set up test VRS files and reader instance.
  const auto expectedTimestamps = getNonDecreasingTimestamps(50);
  const auto filePaths = getOsTempPaths(4);
  auto fileBuilders = createFileBuilders(filePaths);
  // Spray these timestamps in the form of data records across these VRS files
  for (const auto& timestamp : expectedTimestamps) {
    const auto builderIndex = rand() % fileBuilders.size();
    fileBuilders[builderIndex]->recordable_.createData(timestamp, timestamp);
  }
  for (auto& builder : fileBuilders) {
    builder->build();
  }

  MultiRecordFileReader reader;
  ASSERT_FALSE(reader.isOpened());
  ASSERT_EQ(SUCCESS, reader.open(filePaths));
  ASSERT_TRUE(reader.isOpened());
  ASSERT_LT(0, reader.getTotalSourceSize());
  assertEmptyStreamTags(reader);
  TestStreamPlayer streamPlayer;
  for (auto stream : reader.getStreams()) {
    reader.setStreamPlayer(stream, &streamPlayer);
  }

  const auto* lastDataRecord = reader.getLastRecord(Record::Type::DATA);
  const auto* firstDataRecord = reader.getFirstRecord(Record::Type::DATA);
  // Should not exist.
  const auto* firstUndefinedRecord = reader.getFirstRecord(Record::Type::UNDEFINED);
  ASSERT_EQ(
      firstDataRecord->timestamp,
      *min_element(expectedTimestamps.begin(), expectedTimestamps.end()));
  ASSERT_EQ(
      lastDataRecord->timestamp,
      *max_element(expectedTimestamps.begin(), expectedTimestamps.end()));
  ASSERT_EQ(firstUndefinedRecord, nullptr);

  reader.close();
}

/// Helps test various StreamId related methods and collision handling logic.
/// - Creates X files on the fly
/// - Uses X unique Recordables - one per file
/// - Uses 1 common (colliding) Recordable which writes to all files
/// - Each file will have a deterministic number of Config, State and Data records per stream
/// - We store the expected number of records in the form of stream tags which will be used later
/// for validation
/// - We validate that MultiRecordFileReader is able to serve all these Streams after disambiguating
/// internally and match the record counts of each type
class StreamIdCollisionTester {
 public:
  StreamIdCollisionTester() : filePaths_(getOsTempPaths(kFilePathCount)) {
    for (size_t i = 0; i < filePaths_.size(); i++) {
      auto& filePath = filePaths_[i];
      XR_CHECK_FALSE(os::isFile(filePath));
      auto& fileWriter = fileWriters_[i];
      auto& uniqueRecordable = uniqueRecordables_[i];
      createRecords(i, uniqueRecordable);
      createRecords(i, commonRecordable_);
      auto result = fileWriter.writeToFile(filePath);
      XR_CHECK_EQ(SUCCESS, result);
      XR_LOGI("Created VRS File successfully with {} records: {}", totalRecordCount_, filePath);
    }
    test();
  }

  ~StreamIdCollisionTester() {
    removeFiles(filePaths_);
  }

  void test() {
    ASSERT_EQ(SUCCESS, reader_.open(filePaths_));
    ASSERT_EQ(totalRecordCount_, reader_.getRecordCount());
    const auto& streams = reader_.getStreams();
    ASSERT_EQ(
        // no. of unique streams + (no. of common streams * no. of files)
        StreamIdCollisionTester::kUniqueStreamCount + (1 * StreamIdCollisionTester::kFilePathCount),
        streams.size());
    // Create a copy for manipulation
    auto remainingStreams = streams;
    // Ensure that all the expected streams are present and have expected number of records
    validateUniqueStreams(remainingStreams);
    validateCommonStreams(remainingStreams);
    validateGetStreamsByTypeFlavor();
    validateGetRecord();
    validateGetRecordFormats();
    close();
  }

 private:
  static constexpr size_t kFilePathCount = 5;
  // Streams without any collisions across files
  static constexpr auto& kUniqueStreamCount = kFilePathCount;

  static constexpr auto kExpectedRecordCountTagPrefix = "expectedRecordCount";
  static inline const string kOriginalStreamIdTag = "originalStreamId";

  static size_t getExpectedRecordsCount(size_t fileIndex, Record::Type type) {
    auto baseCount = fileIndex * kFilePathCount + 1;
    switch (type) {
      case Record::Type::CONFIGURATION:
        return baseCount;
      case Record::Type::STATE:
        return baseCount + 1;
      case Record::Type::DATA:
        return baseCount + 2;
      default:
        XR_FATAL_ERROR("Unexpected RecordType {}", type);
    }
  }

  void close() {
    ASSERT_EQ(SUCCESS, reader_.close());
    ASSERT_EQ(0, reader_.getRecordCount());
    const auto& stream = uniqueRecordables_[0].getStreamId();
    ASSERT_EQ(0, reader_.getRecordCount(stream));
    ASSERT_EQ(0, reader_.getRecordCount(stream, Record::Type::CONFIGURATION));
    ASSERT_EQ(0, reader_.getRecordCount(stream, Record::Type::STATE));
    ASSERT_EQ(0, reader_.getRecordCount(stream, Record::Type::DATA));
    assertEmptyStreamTags(reader_, stream);
    assertEmptyStreamTags(reader_);
    ASSERT_TRUE(reader_.getStreams().empty());
  }

  void createRecords(size_t fileIndex, TestRecordable& recordable) {
    auto& fileWriter = fileWriters_[fileIndex];
    fileWriter.addRecordable(&recordable);
    recordable.setRecordableIsActive(true);
    createRecords(fileIndex, recordable, Record::Type::CONFIGURATION);
    createRecords(fileIndex, recordable, Record::Type::STATE);
    createRecords(fileIndex, recordable, Record::Type::DATA);
  }

  void createRecords(size_t fileIndex, TestRecordable& recordable, Record::Type type) {
    auto expectedRecordCount = getExpectedRecordsCount(fileIndex, type);
    totalRecordCount_ += expectedRecordCount;
    for (size_t recordCount = 0; recordCount < expectedRecordCount; recordCount++) {
      recordable.createDefaultRecord(type);
    }
    auto tagKey = getExpectedRecordCountTagKey(type);
    recordable.setTag(tagKey, to_string(expectedRecordCount));
    recordable.setTag(kOriginalStreamIdTag, recordable.getStreamId().getName());
  }

  static string getExpectedRecordCountTagKey(Record::Type type) {
    return fmt::format("{}{}", kExpectedRecordCountTagPrefix, Record::typeName(type));
  }

  uint32_t getExpectedRecordCount(UniqueStreamId streamId, Record::Type type) const {
    auto tagKey = getExpectedRecordCountTagKey(type);
    auto expectedCountStr = reader_.getTag(streamId, tagKey);
    EXPECT_FALSE(expectedCountStr.empty());
    return stoi(expectedCountStr);
  }

  uint32_t validateRecordCount(UniqueStreamId streamId, Record::Type type) const {
    const auto expectedCount = getExpectedRecordCount(streamId, type);
    EXPECT_EQ(expectedCount, reader_.getRecordCount(streamId, type));
    return expectedCount;
  }

  void validateRecordCount(UniqueStreamId streamId) const {
    uint32_t expectedCount = 0;
    expectedCount += validateRecordCount(streamId, Record::Type::CONFIGURATION);
    expectedCount += validateRecordCount(streamId, Record::Type::STATE);
    expectedCount += validateRecordCount(streamId, Record::Type::DATA);
    EXPECT_EQ(expectedCount, reader_.getRecordCount(streamId));
    EXPECT_EQ(expectedCount, reader_.getIndex(streamId).size());
    static const StreamId unknownStream;
    EXPECT_EQ(0, reader_.getRecordCount(unknownStream));
    EXPECT_EQ(0, reader_.getRecordCount(unknownStream, Record::Type::DATA));
    assertEmptyStreamTags(reader_, unknownStream);
    EXPECT_TRUE(reader_.getIndex(unknownStream).empty());
  }

  void validateUniqueStreams(set<UniqueStreamId>& remainingStreams) const {
    for (const auto& uniqueRecordable : uniqueRecordables_) {
      const auto expectedStreamId = uniqueRecordable.getStreamId();
      auto it = remainingStreams.find(expectedStreamId);
      EXPECT_TRUE(it != remainingStreams.end())
          << "Unable to find StreamId " << expectedStreamId.getName();
      remainingStreams.erase(it);
      validateRecordCount(expectedStreamId);
      EXPECT_EQ(expectedStreamId.getName(), reader_.getTag(expectedStreamId, kOriginalStreamIdTag));
      EXPECT_EQ(
          expectedStreamId,
          reader_.getStreamForTag(kOriginalStreamIdTag, expectedStreamId.getName()));
      EXPECT_FALSE(reader_.getStreamForTag(kOriginalStreamIdTag, "unkownValue").isValid());
      EXPECT_EQ(
          expectedStreamId, reader_.getUniqueStreamId(reader_.getRecord(expectedStreamId, 0)));
    }
  }

  void validateCommonStreams(set<UniqueStreamId>& remainingStreams) const {
    const auto expectedOriginalStreamId = commonRecordable_.getStreamId().getName();
    EXPECT_EQ(kFilePathCount, remainingStreams.size())
        << "The common stream must be split into one unique stream per file";
    for (const auto& commonStreamId : remainingStreams) {
      validateRecordCount(commonStreamId);
      EXPECT_EQ(expectedOriginalStreamId, reader_.getTag(commonStreamId, kOriginalStreamIdTag));
      EXPECT_EQ(commonStreamId, reader_.getUniqueStreamId(reader_.getRecord(commonStreamId, 0)));
    }
    EXPECT_EQ(
        expectedOriginalStreamId,
        reader_.getStreamForTag(kOriginalStreamIdTag, expectedOriginalStreamId).getName());
    EXPECT_FALSE(reader_.getStreamForTag(kOriginalStreamIdTag, "unkownValue").isValid());
  }

  void validateGetStreamsByTypeFlavor() {
    const auto streamsSet = reader_.getStreams();
    vector<UniqueStreamId> expectedStreams(streamsSet.begin(), streamsSet.end());
    EXPECT_EQ(expectedStreams, reader_.getStreams(RecordableTypeId::Undefined));
    EXPECT_EQ(0, reader_.getStreams(RecordableTypeId::AccelerometerRecordableClass).size());
    EXPECT_EQ(expectedStreams, reader_.getStreams(kTestRecordableTypeId));
    EXPECT_EQ(expectedStreams, reader_.getStreams(kTestRecordableTypeId, kTestFlavor));
    ASSERT_EQ(kTestFlavor, reader_.getFlavor(expectedStreams.front()));
    EXPECT_EQ(0, reader_.getStreams(kTestRecordableTypeId, "unknownFlavor").size());
  }

  void validateGetRecord() {
    const auto* firstRecord = reader_.getRecord(0);
    ASSERT_EQ(0, reader_.getRecordIndex(firstRecord));
    const auto firstStream = firstRecord->streamId;
    ASSERT_EQ(firstRecord, reader_.getRecord(firstStream, 0));
    ASSERT_EQ(firstRecord, reader_.getRecord(firstStream, firstRecord->recordType, 0));
    ASSERT_EQ(nullptr, reader_.getRecord(firstStream, Record::Type::UNDEFINED, 0));
    const auto& firstStreamIndex = reader_.getIndex(firstStream);
    ASSERT_EQ(reader_.getRecordCount(firstStream), firstStreamIndex.size());
    ASSERT_EQ(firstStreamIndex[0], reader_.getRecord(firstStream, 0));
    const IndexRecord::RecordInfo* lastRecord = firstStreamIndex[firstStreamIndex.size() - 1];
    ASSERT_EQ(lastRecord, reader_.getLastRecord(firstStream, lastRecord->recordType));
    ASSERT_EQ(nullptr, reader_.getLastRecord(firstStream, Record::Type::UNDEFINED));
  }

  void validateGetRecordFormats() {
    RecordFormatMap recordFormatMap;
    for (const auto& stream : reader_.getStreams()) {
      reader_.getRecordFormats(stream, recordFormatMap);
      ASSERT_LT(0, recordFormatMap.size());
    }
    UniqueStreamId unknownStreamId;
    reader_.getRecordFormats(unknownStreamId, recordFormatMap);
    ASSERT_EQ(0, recordFormatMap.size());
  }

  // Streams which don't have collisions across files
  TestRecordable uniqueRecordables_[kUniqueStreamCount];
  // Stream which is common across all files
  TestRecordable commonRecordable_;

  RecordFileWriter fileWriters_[kFilePathCount];
  vector<string> filePaths_;

  size_t totalRecordCount_{};
  MultiRecordFileReader reader_;
};

TEST_F(MultiRecordFileReaderTest, streamIdCollision) {
  StreamIdCollisionTester();
}

TEST_F(MultiRecordFileReaderTest, getFileChunks) {
  constexpr auto numDataRecords = 10;
  constexpr auto fileCount = 4;
  const auto filePaths = getOsTempPaths(fileCount);
  for (size_t i = 0; i < filePaths.size(); i++) {
    createVRSFileSynchronously(filePaths[i], numDataRecords);
  }
  // Single file use case
  MultiRecordFileReader singleReader;
  const auto& singleFilePath = filePaths[0];
  ASSERT_EQ(SUCCESS, singleReader.open(singleFilePath));
  ASSERT_FALSE(singleReader.getFileChunks().empty());
  const auto singleFileChunks = singleReader.getFileChunks();
  ASSERT_EQ(1, singleFileChunks.size());
  ASSERT_EQ(singleFilePath, singleFileChunks[0].first);
  RecordFileReader singleReaderExpected;
  ASSERT_EQ(SUCCESS, singleReaderExpected.openFile(singleFilePath));
  const auto singleFileChunksExpected = singleReader.getFileChunks();
  ASSERT_EQ(singleFileChunksExpected, singleFileChunks);
  const auto expectedSize = singleFileChunksExpected[0].second;
  ASSERT_EQ(singleReaderExpected.getTotalSourceSize(), singleReader.getTotalSourceSize());
  ASSERT_EQ(SUCCESS, singleReader.close());
  ASSERT_EQ(SUCCESS, singleReaderExpected.closeFile());
  ASSERT_TRUE(singleReader.getFileChunks().empty());
  // Multi file use case
  MultiRecordFileReader multiReader;
  ASSERT_EQ(SUCCESS, multiReader.open(filePaths));
  const auto fileChunks = multiReader.getFileChunks();
  ASSERT_EQ(filePaths.size(), fileChunks.size());
  for (size_t i = 0; i < filePaths.size(); i++) {
    const auto& fileChunk = fileChunks[i];
    ASSERT_EQ(filePaths[i], fileChunk.first);
    ASSERT_EQ(expectedSize, fileChunk.second);
  }
  ASSERT_EQ(SUCCESS, multiReader.close());
  removeFiles(filePaths);
}
