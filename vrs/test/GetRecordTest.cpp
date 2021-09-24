// (c) Facebook, Inc. and its affiliates. Confidential and proprietary.

#include <random>

#include <gtest/gtest.h>

#include <TestDataDir/TestDataDir.h>
#include <vrs/RecordFileReader.h>

using namespace std;
using namespace vrs;

namespace {
struct GetRecordTester : testing::Test {
  std::string kTestFile = string(coretech::getTestDataDir()) + "/VRS_Files/VRSTestRecording.vrs";
};
} // namespace

// legacy implementation, without caching
static const IndexRecord::RecordInfo* getRecord(
    vrs::RecordFileReader& file,
    StreamId streamId,
    Record::Type recordType,
    uint32_t indexNumber) {
  auto& index = file.getIndex(streamId);
  if (indexNumber >= index.size()) {
    return nullptr;
  }
  uint32_t hitCount = 0;
  for (auto record : index) {
    if (record->recordType == recordType && hitCount++ == indexNumber) {
      return record;
    }
  }
  return nullptr;
}

inline void
check(vrs::RecordFileReader& file, StreamId id, Record::Type type, uint32_t indexNumber) {
  // Compare the old method and the new method
  const IndexRecord::RecordInfo* ref = getRecord(file, id, type, indexNumber);
  EXPECT_EQ(ref, file.getRecord(id, type, indexNumber));
  EXPECT_EQ(ref, file.getRecord(id, type, indexNumber));
  EXPECT_EQ(getRecord(file, id, type, indexNumber + 1), file.getRecord(id, type, indexNumber + 1));
  EXPECT_EQ(ref, file.getRecord(id, type, indexNumber));
}

void checkIndex(vrs::RecordFileReader& file, uint32_t recordIndex) {
  const auto& index = file.getIndex();
  const IndexRecord::RecordInfo& record = index[recordIndex];
  const auto& streamIndex = file.getIndex(record.streamId);
  const IndexRecord::RecordInfo* r = file.getRecordByTime(record.timestamp);
  EXPECT_NE(r, nullptr);
  // always find the first record with that timestamp
  uint32_t i = file.getRecordIndex(r);
  EXPECT_FALSE(i > 0 && index[i - 1].timestamp == record.timestamp);
  // our original index may not be the first with that timestamp, so look for it
  while (i != recordIndex && i + 1 < index.size() && index[i + 1].timestamp == record.timestamp) {
    i++;
  }
  EXPECT_EQ(i, recordIndex);

  // test slightly different timestamps
  r = file.getRecordByTime(nextafter(record.timestamp, record.timestamp - 1));
  EXPECT_LE(file.getRecordIndex(r), recordIndex);
  r = file.getRecordByTime(nextafter(record.timestamp, record.timestamp + 1));
  EXPECT_GT(file.getRecordIndex(r), recordIndex);

  // search with record type
  r = file.getRecordByTime(record.recordType, record.timestamp);
  EXPECT_NE(r, nullptr);
  i = file.getRecordIndex(r);
  // always find the first record with that timestamp
  EXPECT_FALSE(
      i > 0 && index[i - 1].recordType == record.recordType &&
      index[i - 1].timestamp == record.timestamp);
  // our original index may not be the first with that timestamp, so look for it
  while (i != recordIndex && i + 1 < index.size() && index[i - 1].recordType == record.recordType &&
         index[i + 1].timestamp == record.timestamp) {
    i++;
  }
  EXPECT_EQ(i, recordIndex);
  // test slightly different timestamps
  r = file.getRecordByTime(record.recordType, nextafter(record.timestamp, record.timestamp - 1));
  EXPECT_LE(file.getRecordIndex(r), recordIndex);
  r = file.getRecordByTime(record.recordType, nextafter(record.timestamp, record.timestamp + 1));
  EXPECT_GT(file.getRecordIndex(r), recordIndex);

  // search with stream Id
  r = file.getRecordByTime(record.streamId, record.timestamp);
  EXPECT_NE(r, nullptr);
  i = file.getRecordIndex(r);
  // always find the first record with that timestamp
  EXPECT_FALSE(
      i > 0 && index[i - 1].streamId == record.streamId &&
      index[i - 1].timestamp == record.timestamp);
  // our original index may not be the first with that timestamp, so look for it
  while (i != recordIndex && i + 1 < index.size() && index[i - 1].streamId == record.streamId &&
         index[i + 1].timestamp == record.timestamp) {
    i++;
  }
  EXPECT_EQ(i, recordIndex);

  // Check getRecordStreamIndex()
  EXPECT_EQ(r, streamIndex[file.getRecordStreamIndex(r)]);

  // test slightly different timestamps
  r = file.getRecordByTime(record.streamId, nextafter(record.timestamp, record.timestamp - 1));
  EXPECT_LE(file.getRecordIndex(r), recordIndex);
  r = file.getRecordByTime(record.streamId, nextafter(record.timestamp, record.timestamp + 1));
  EXPECT_GT(file.getRecordIndex(r), recordIndex);

  // search with stream Id & record type
  r = file.getRecordByTime(record.streamId, record.recordType, record.timestamp);
  EXPECT_NE(r, nullptr);
  i = file.getRecordIndex(r);
  // always find the first record with that timestamp
  EXPECT_FALSE(
      i > 0 && index[i - 1].streamId == record.streamId &&
      index[i - 1].recordType == record.recordType && index[i - 1].timestamp == record.timestamp);
  // our original index may not be the first with that timestamp, so look for it
  while (i != recordIndex && i + 1 < index.size() && index[i - 1].streamId == record.streamId &&
         index[i - 1].recordType == record.recordType &&
         index[i + 1].timestamp == record.timestamp) {
    i++;
  }
  EXPECT_EQ(i, recordIndex);
  // test slightly different timestamps
  r = file.getRecordByTime(
      record.streamId, record.recordType, nextafter(record.timestamp, record.timestamp - 1));
  EXPECT_NE(r, nullptr);
  i = file.getRecordIndex(r);
  EXPECT_LE(i, recordIndex);
  r = file.getRecordByTime(
      record.streamId, record.recordType, nextafter(record.timestamp, record.timestamp + 1));
  EXPECT_GT(file.getRecordIndex(r), recordIndex);
}

TEST_F(GetRecordTester, GetRecordTest) {
  vrs::RecordFileReader file;
  EXPECT_EQ(file.openFile(kTestFile), 0);
  EXPECT_EQ(file.getRecordCount(), 3390);
  EXPECT_EQ(file.getStreams().size(), 6);

  std::mt19937 gen(123456); // constant seed, to always do the same tests

  // random type generator
  vector<Record::Type> types = {
      Record::Type::CONFIGURATION, Record::Type::STATE, Record::Type::DATA};
  std::uniform_int_distribution<size_t> typeIndex(0, types.size() - 1);
  // random stream id generator
  vector<StreamId> ids;
  std::copy(file.getStreams().begin(), file.getStreams().end(), std::back_inserter(ids));
  std::uniform_int_distribution<size_t> idIndex(0, ids.size() - 1);
  for (int k = 0; k < 5000; k++) {
    StreamId id = ids[idIndex(gen)];
    // random index generator
    std::uniform_int_distribution<uint32_t> indexNumber(0, file.getIndex(id).size());
    check(file, id, types[typeIndex(gen)], indexNumber(gen));
  }

  // test seekRecordIndex methods
  const auto& index = file.getIndex();
  EXPECT_EQ(index.size(), 3390);

  // Asking for the index of nullptr is safe, but you get an out of bound index.
  EXPECT_GE(file.getRecordIndex(nullptr), index.size());

  uint32_t midIndex = static_cast<uint32_t>(index.size() / 2);
  const IndexRecord::RecordInfo* record = &index[midIndex];
  const IndexRecord::RecordInfo* rec = file.getRecordByTime(record->timestamp);
  EXPECT_EQ(record, rec);
  EXPECT_EQ(file.getRecordIndex(rec), midIndex);

  rec = file.getRecordByTime(index[midIndex].recordType, index[midIndex].timestamp);

  EXPECT_EQ(file.getRecordIndex(rec), midIndex);

  rec = file.getRecordByTime(index[midIndex].streamId, index[midIndex].timestamp);
  EXPECT_EQ(file.getRecordIndex(rec), midIndex);

  rec = file.getRecordByTime(index[0].timestamp - 100);
  EXPECT_EQ(file.getRecordIndex(rec), 0);
  rec = file.getRecordByTime(Record::Type::CONFIGURATION, index[0].timestamp - 100);
  EXPECT_EQ(file.getRecordIndex(rec), 131);
  rec = file.getRecordByTime(Record::Type::STATE, index[0].timestamp - 100);
  EXPECT_EQ(file.getRecordIndex(rec), 132);

  // records 1963 & 1964 have identical timestamps
  rec = file.getRecordByTime(index[1963].timestamp);
  EXPECT_EQ(file.getRecordIndex(rec), 1963);
  rec = file.getRecordByTime(index[1964].timestamp);
  EXPECT_EQ(file.getRecordIndex(rec), 1963);

  // test timestamps slightly greater or lesser
  rec = file.getRecordByTime(nextafter(index[1963].timestamp, index[1963].timestamp - 1.));
  EXPECT_EQ(file.getRecordIndex(rec), 1963);
  rec = file.getRecordByTime(nextafter(index[1963].timestamp, index[1963].timestamp + 1.));
  EXPECT_EQ(file.getRecordIndex(rec), 1965);

  rec = file.getRecordByTime(index[1964].recordType, index[1964].timestamp);
  EXPECT_EQ(index[1963].recordType, index[1964].recordType);
  EXPECT_EQ(file.getRecordIndex(rec), 1963);
  rec = file.getRecordByTime(index[1964].streamId, index[1964].timestamp);
  EXPECT_EQ(file.getRecordIndex(rec), 1964);

  for (uint32_t i = 0; i < index.size(); i++) {
    checkIndex(file, i);
  }
}
