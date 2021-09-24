// (c) Facebook, Inc. and its affiliates. Confidential and proprietary.

#include <gtest/gtest.h>

#include <string>

#include <vrs/Record.h>
#include <vrs/RecordFileWriter.h>
#include <vrs/RecordManager.h>
#include <vrs/StreamId.h>

#include <vrs/test/helpers/VRSTestsHelpers.h>

using namespace vrs;
using namespace vrs::test;

namespace {

struct RecordTester : testing::Test {};

size_t collect(
    RecordFileWriter::RecordBatches& batches,
    vector<pair<RecordManager*, StreamId>> recordManagers,
    double maxTime) {
  batches.emplace_back(new RecordFileWriter::RecordBatch());
  RecordFileWriter::RecordBatch& batch = *batches.back();
  size_t count = 0;
  for (auto r : recordManagers) {
    batch.push_back({r.second, {}});
    list<Record*>& records = batch.back().second;
    r.first->collectOldRecords(maxTime, records);
    for (auto record : records) {
      EXPECT_LE(record->getTimestamp(), maxTime);
    }
    count += records.size();
  }
  return count;
}

bool isProperlySorted(const RecordFileWriter::SortedRecords& sortedRecords) {
  for (auto iter = sortedRecords.begin();
       iter != sortedRecords.end() && (iter + 1) != sortedRecords.end();
       ++iter) {
    // elements should be ordered, and equality is not OK either...
    if (*(iter + 1) < *iter || !(*iter < *(iter + 1))) {
      return false;
    }
  }
  return true;
}

} // namespace

TEST_F(RecordTester, testRecord) {
  EXPECT_EQ(static_cast<uint8_t>(Record::Type::UNDEFINED), 0);
  EXPECT_EQ(static_cast<uint8_t>(Record::Type::STATE), 1);
  EXPECT_EQ(static_cast<uint8_t>(Record::Type::CONFIGURATION), 2);
  EXPECT_EQ(static_cast<uint8_t>(Record::Type::DATA), 3);
  EXPECT_EQ(static_cast<Record::Type>(0), Record::Type::UNDEFINED);
  EXPECT_EQ(static_cast<Record::Type>(1), Record::Type::STATE);
  EXPECT_EQ(static_cast<Record::Type>(2), Record::Type::CONFIGURATION);
  EXPECT_EQ(static_cast<Record::Type>(3), Record::Type::DATA);
}

TEST_F(RecordTester, streamIdTest) {
  StreamId id(RecordableTypeId::UnitTest1, 1);
  EXPECT_EQ(
      id.getNumericName(),
      std::to_string(static_cast<int>(id.getTypeId())) + "-" + std::to_string(id.getInstanceId()));
  EXPECT_EQ(StreamId::fromNumericName(id.getNumericName()), id);
  EXPECT_EQ(StreamId::fromNumericName("1-0"), StreamId(static_cast<RecordableTypeId>(1), 0));
  EXPECT_EQ(StreamId::fromNumericName("123-2"), StreamId(static_cast<RecordableTypeId>(123), 2));
  EXPECT_EQ(
      StreamId::fromNumericName("65535-65535"),
      StreamId(static_cast<RecordableTypeId>(65535), 65535));
  EXPECT_FALSE(StreamId::fromNumericName("-65535-65535").isValid());
  EXPECT_FALSE(StreamId::fromNumericName("65535-").isValid());
  EXPECT_FALSE(StreamId::fromNumericName("65d535-1").isValid());
  EXPECT_FALSE(StreamId::fromNumericName("65535").isValid());
  EXPECT_FALSE(StreamId::fromNumericName("123-45s").isValid());
  EXPECT_FALSE(StreamId::fromNumericName("123-a45").isValid());
}

TEST_F(RecordTester, addRecordBatchesToSortedRecordsTester) {
  RecordFileWriter::SortedRecords sr;
  RecordFileWriter::RecordBatches batches;
  RecordManager recordManagerA;
  RecordManager recordManagerB;
  RecordManager recordManagerC;

  StreamId idA{RecordableTypeId::UnitTest1, 1};
  StreamId idB{RecordableTypeId::UnitTest1, 2};
  StreamId idC{RecordableTypeId::UnitTest2, 1};

  recordManagerA.createRecord(1.5, Record::Type::CONFIGURATION, 1, DataSource());
  for (int t = 1; t < 50; t++) {
    recordManagerA.createRecord(t, Record::Type::DATA, 1, DataSource());
  }

  recordManagerB.createRecord(1, Record::Type::CONFIGURATION, 1, DataSource());
  for (int t = 1; t < 200; t++) {
    recordManagerB.createRecord(0.1 * t + 0.25, Record::Type::DATA, 1, DataSource());
  }

  recordManagerC.createRecord(1, Record::Type::CONFIGURATION, 1, DataSource());
  for (int t = 1; t < 200; t++) {
    recordManagerC.createRecord(0.15 * t + 0.25, Record::Type::DATA, 1, DataSource());
  }

  vector<pair<RecordManager*, StreamId>> recordManagersAll{
      {&recordManagerA, idA}, {&recordManagerB, idB}, {&recordManagerC, idC}};
  vector<pair<RecordManager*, StreamId>> recordManagerAB{
      {&recordManagerA, idA}, {&recordManagerB, idB}};
  vector<pair<RecordManager*, StreamId>> recordManagerAOnly{{&recordManagerA, idA}};
  vector<pair<RecordManager*, StreamId>> recordManagerBOnly{{&recordManagerB, idB}};
  vector<pair<RecordManager*, StreamId>> recordManagerCOnly{{&recordManagerC, idC}};

  EXPECT_EQ(collect(batches, recordManagersAll, 5), 86);
  RecordFileWriterTester::addRecordBatchesToSortedRecords(batches, sr);
  EXPECT_EQ(sr.size(), 86);
  EXPECT_TRUE(isProperlySorted(sr));
  batches.clear();

  EXPECT_EQ(collect(batches, recordManagerAB, 8), 33);
  EXPECT_EQ(collect(batches, recordManagerCOnly, 8), 20);
  recordManagerA.createRecord(6.25, Record::Type::DATA, 1, DataSource());
  recordManagerB.createRecord(4, Record::Type::DATA, 1, DataSource());
  EXPECT_EQ(collect(batches, recordManagersAll, 10), 38);
  RecordFileWriterTester::addRecordBatchesToSortedRecords(batches, sr);
  EXPECT_EQ(sr.size(), 177);
  EXPECT_TRUE(isProperlySorted(sr));
  batches.clear();

  // don't collect anything this time
  EXPECT_EQ(collect(batches, recordManagersAll, 10), 0);
  RecordFileWriterTester::addRecordBatchesToSortedRecords(batches, sr);
  EXPECT_EQ(sr.size(), 177);
  EXPECT_TRUE(isProperlySorted(sr));
  batches.clear();

  recordManagerA.createRecord(2.5, Record::Type::DATA, 1, DataSource());
  recordManagerA.createRecord(3.5, Record::Type::DATA, 1, DataSource());
  EXPECT_EQ(collect(batches, recordManagersAll, 100), 277);
  RecordFileWriterTester::addRecordBatchesToSortedRecords(batches, sr);
  EXPECT_EQ(sr.size(), 454);
  EXPECT_TRUE(isProperlySorted(sr));
  batches.clear();

  for (const auto& r : sr) {
    r.record->recycle();
  }
}

template <class R>
void checkIndexOrder(const vector<R>& records) {
  for (size_t first = 0; first < records.size(); first++) {
    EXPECT_EQ(records[first], records[first]);
    for (size_t second = first + 1; second < records.size(); second++) {
      EXPECT_TRUE(records[first] < records[second]);
      EXPECT_FALSE(records[second] < records[first]);
      EXPECT_FALSE(records[first] == records[second]);
    }
  }
}

TEST_F(RecordTester, indexSortTest) {
  const StreamId id1(RecordableTypeId::UnitTest1, 1);
  const StreamId id2(RecordableTypeId::UnitTest1, 2);
  const StreamId id3(RecordableTypeId::UnitTest2, 1);
  const StreamId id4(RecordableTypeId::UnitTest2, 2);

  // Create a vector of records you expect to be sorted and verify all the compares combo
  vector<IndexRecord::RecordInfo> records;
  // test types
  records.emplace_back(0, 100, id1, Record::Type::STATE);

  // test StreamId, in all dimensions
  records.emplace_back(1, 100, id1, Record::Type::DATA);
  records.emplace_back(1, 100, id2, Record::Type::DATA);
  records.emplace_back(1, 100, id3, Record::Type::DATA);
  records.emplace_back(1, 100, id4, Record::Type::DATA);

  // test time
  records.emplace_back(2, 100, id4, Record::Type::DATA);

  // test offset
  records.emplace_back(2, 101, id4, Record::Type::DATA);

  // test timestamp matters most
  records.emplace_back(3, 200, id2, Record::Type::TAGS);
  // increase timestamp, decrease rest
  records.emplace_back(4, 100, id1, Record::Type::DATA);
  // keep timestamp, increase StreamId, decrease rest
  records.emplace_back(4, 99, id2, Record::Type::CONFIGURATION);

  checkIndexOrder(records);
}

void checkSortOrder(const vector<RecordFileWriter::SortRecord>& records) {
  for (size_t first = 0; first < records.size(); first++) {
    for (size_t second = first + 1; second < records.size(); second++) {
      EXPECT_TRUE(records[first] < records[second]);
      EXPECT_FALSE(records[second] < records[first]);
    }
  }
}

#define NEW_RECORD(TIMESTAMP, ID, TYPE) \
  recordManager.createRecord(TIMESTAMP, TYPE, 0, DataSource()), ID

TEST_F(RecordTester, sortRecordSortTest) {
  const StreamId id1(RecordableTypeId::UnitTest1, 1);
  const StreamId id2(RecordableTypeId::UnitTest1, 2);
  const StreamId id3(RecordableTypeId::UnitTest2, 1);
  const StreamId id4(RecordableTypeId::UnitTest2, 2);

  RecordManager recordManager;

  // Create a vector of records you expect to be sorted and verify all the compares combo
  vector<RecordFileWriter::SortRecord> records;
  // test types
  records.emplace_back(NEW_RECORD(0, id1, Record::Type::STATE));
  records.emplace_back(NEW_RECORD(0, id1, Record::Type::CONFIGURATION));
  records.emplace_back(NEW_RECORD(0, id1, Record::Type::DATA));
  records.emplace_back(NEW_RECORD(0, id1, Record::Type::TAGS));

  // test StreamId, in all dimensions
  records.emplace_back(NEW_RECORD(1, id1, Record::Type::DATA));
  records.emplace_back(NEW_RECORD(1, id2, Record::Type::DATA));
  records.emplace_back(NEW_RECORD(1, id3, Record::Type::DATA));
  records.emplace_back(NEW_RECORD(1, id4, Record::Type::DATA));

  // test time
  records.emplace_back(NEW_RECORD(2, id4, Record::Type::DATA));

  // test offset
  records.emplace_back(NEW_RECORD(2, id4, Record::Type::DATA));

  // test timestamp matters most
  records.emplace_back(NEW_RECORD(3, id2, Record::Type::TAGS));
  // increase timestamp, decrease rest
  records.emplace_back(NEW_RECORD(4, id1, Record::Type::DATA));
  // keep timestamp, increase StreamId, decrease rest
  records.emplace_back(NEW_RECORD(4, id2, Record::Type::CONFIGURATION));
  // keep timestamp and StreamId, increase record type, decrease rest
  records.emplace_back(NEW_RECORD(4, id2, Record::Type::DATA));
  // keep timestamp, StreamId and record type, increase offset
  records.emplace_back(NEW_RECORD(4, id2, Record::Type::DATA));

  checkSortOrder(records);
}
