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

#include <cmath>
#include <random>
#include "vrs/StreamId.h"

#include <gtest/gtest.h>

#include <TestDataDir/TestDataDir.h>
#include <vrs/IndexRecord.h>
#include <vrs/RecordFileReader.h>

using namespace std;
using namespace vrs;

namespace {
struct GetRecordTester : testing::Test {
  string kTestFile = string(coretech::getTestDataDir()) + "/VRS_Files/sample_file.vrs";
  string kTestFile2 = string(coretech::getTestDataDir()) + "/VRS_Files/simulated.vrs";
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

static bool isCloserThan(
    const IndexRecord::RecordInfo& closer,
    double timestamp,
    const IndexRecord::RecordInfo& farther) {
  return abs(closer.timestamp - timestamp) < abs(farther.timestamp - timestamp);
}

const IndexRecord::RecordInfo* getNearestRecordByTime(
    const vector<IndexRecord::RecordInfo>& index,
    double timestamp,
    double epsilon,
    StreamId streamId = {},
    Record::Type recordType = Record::Type::UNDEFINED) {
  const IndexRecord::RecordInfo* closest = nullptr;
  for (const auto& record : index) {
    if ((!streamId.isValid() || streamId == record.streamId) &&
        (recordType == Record::Type::UNDEFINED || recordType == record.recordType) &&
        (closest == nullptr || isCloserThan(record, timestamp, *closest) ||
         (record.timestamp == closest->timestamp && timestamp > record.timestamp))) {
      closest = &record;
    }
  }
  if (closest != nullptr && (abs(closest->timestamp - timestamp) > epsilon)) {
    closest = nullptr;
  }
  EXPECT_TRUE(!streamId.isValid() || closest == nullptr || streamId == closest->streamId);
  EXPECT_TRUE(
      recordType == Record::Type::UNDEFINED || closest == nullptr ||
      recordType == closest->recordType);
  return closest;
}

void checkNearestRecord(vrs::RecordFileReader& file, const IndexRecord::RecordInfo& record) {
  const auto& index = file.getIndex();
  auto streamIds = file.getStreams();
  // Add invalid stream for test
  streamIds.insert({});
  for (double timestampDiff : {1., -1., 1e-7, -1e-7, 1e-6, -1e-6, 0.}) {
    double target = record.timestamp + timestampDiff;
    // Test epsilon to be different kinds
    for (double epsilon : {1.1, 1., 9e-1, 2e-7, 1e-7, 9e-8, 0.}) {
      for (StreamId streamId : streamIds) {
        for (Record::Type type :
             {Record::Type::CONFIGURATION, Record::Type::STATE, Record::Type::DATA}) {
          auto r = file.getNearestRecordByTime(target, epsilon, streamId, type);
          auto ref = getNearestRecordByTime(index, target, epsilon, streamId, type);
          EXPECT_EQ(r, ref);
        }
      }
    }
  }
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
  while (i != recordIndex && i + 1 < index.size() && index[i + 1].streamId == record.streamId &&
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
  while (i != recordIndex && i + 1 < index.size() && index[i + 1].streamId == record.streamId &&
         index[i + 1].recordType == record.recordType &&
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

  // search nearest record
  r = file.getNearestRecordByTime(record.timestamp, 1e-6, record.streamId);
  EXPECT_NE(r, nullptr);
  i = file.getRecordIndex(r);
  EXPECT_FALSE(
      i > 0 && index[i - 1].streamId == record.streamId &&
      index[i - 1].timestamp == record.timestamp);

  // search nearest record with slightly different timestamps
  checkNearestRecord(file, record);

  r = file.getNearestRecordByTime(record.timestamp, 1e-6);
  EXPECT_NE(r, nullptr);
}

TEST_F(GetRecordTester, GetRecordTest) {
  vrs::RecordFileReader file;
  EXPECT_EQ(file.openFile(kTestFile), 0);
  EXPECT_EQ(file.getRecordCount(), 307);
  EXPECT_EQ(file.getStreams().size(), 3);

  mt19937 gen(123456); // constant seed, to always do the same tests

  // random type generator
  vector<Record::Type> types = {
      Record::Type::CONFIGURATION, Record::Type::STATE, Record::Type::DATA};
  uniform_int_distribution<size_t> typeIndex(0, types.size() - 1);
  // random stream id generator
  vector<StreamId> ids;
  copy(file.getStreams().begin(), file.getStreams().end(), back_inserter(ids));
  uniform_int_distribution<size_t> idIndex(0, ids.size() - 1);
  for (int k = 0; k < 5000; k++) {
    StreamId id = ids[idIndex(gen)];
    // random index generator
    uniform_int_distribution<uint32_t> indexNumber(0, file.getIndex(id).size());
    check(file, id, types[typeIndex(gen)], indexNumber(gen));
  }

  // test seekRecordIndex methods
  const auto& index = file.getIndex();
  EXPECT_EQ(index.size(), 307);

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

  double startTime = index[0].timestamp;

  rec = file.getRecordByTime(nextafter(startTime, startTime - 1));
  EXPECT_EQ(file.getRecordIndex(rec), 0);

  rec = file.getRecordByTime(startTime);
  EXPECT_EQ(file.getRecordIndex(rec), 0);

  rec = file.getRecordByTime(Record::Type::CONFIGURATION, nextafter(startTime, startTime + 1));
  EXPECT_EQ(file.getRecordIndex(rec), 2);

  rec = file.getRecordByTime(Record::Type::STATE, startTime);
  EXPECT_EQ(file.getRecordIndex(rec), 1);

  // records 33 & 34 have identical timestamps
  rec = file.getRecordByTime(index[33].timestamp);
  EXPECT_EQ(file.getRecordIndex(rec), 33);
  rec = file.getRecordByTime(index[34].timestamp);
  EXPECT_EQ(file.getRecordIndex(rec), 33);

  // test timestamps slightly greater or lesser
  rec = file.getRecordByTime(nextafter(index[33].timestamp, index[33].timestamp - 1.));
  EXPECT_EQ(file.getRecordIndex(rec), 33);
  rec = file.getRecordByTime(nextafter(index[33].timestamp, index[33].timestamp + 1.));
  EXPECT_EQ(file.getRecordIndex(rec), 35);

  rec = file.getRecordByTime(index[34].recordType, index[34].timestamp);
  EXPECT_EQ(index[33].recordType, index[34].recordType);
  EXPECT_EQ(file.getRecordIndex(rec), 33);
  rec = file.getRecordByTime(index[34].streamId, index[34].timestamp);
  EXPECT_EQ(file.getRecordIndex(rec), 33);

  for (uint32_t i = 0; i < index.size(); i++) {
    checkIndex(file, i);
  }
}

TEST_F(GetRecordTester, GetRecordForwardBackwardTest) {
  vrs::RecordFileReader file;
  EXPECT_EQ(file.openFile(kTestFile2), 0);
  EXPECT_EQ(file.getRecordCount(), 15377);
  EXPECT_EQ(file.getStreams().size(), 3);
  auto iter = file.getStreams().begin();
  StreamId id1 = *iter++;
  EXPECT_EQ(file.getRecordCount(id1), 76);
  EXPECT_EQ(file.getRecordCount(id1, Record::Type::CONFIGURATION), 1);
  EXPECT_EQ(file.getRecordCount(id1, Record::Type::STATE), 1);
  EXPECT_EQ(file.getRecordCount(id1, Record::Type::DATA), 74);
  StreamId id2 = *iter++;
  EXPECT_EQ(file.getRecordCount(id2), 228);
  EXPECT_EQ(file.getRecordCount(id2, Record::Type::CONFIGURATION), 1);
  EXPECT_EQ(file.getRecordCount(id2, Record::Type::STATE), 1);
  EXPECT_EQ(file.getRecordCount(id2, Record::Type::DATA), 226);
  StreamId id3 = *iter++;
  EXPECT_EQ(file.getRecordCount(id3), 15073);
  EXPECT_EQ(file.getRecordCount(id3, Record::Type::CONFIGURATION), 1);
  EXPECT_EQ(file.getRecordCount(id3, Record::Type::STATE), 1);
  EXPECT_EQ(file.getRecordCount(id3, Record::Type::DATA), 15071);
  const auto& index = file.getIndex();
  // validate forward iteration
  RecordFileReader::RecordTypeCounter counters;
  for (const auto& record : index) {
    if (record.streamId == id2) {
      uint32_t streamTypeIndex = counters[record.recordType];
      if (streamTypeIndex % 7 != 3) {
        EXPECT_EQ(&record, file.getRecord(id2, record.recordType, streamTypeIndex));
      }
      uint32_t totalCount = counters.totalCount();
      if (totalCount % 7 != 3) {
        EXPECT_EQ(&record, file.getRecord(id2, totalCount));
      }
      counters[record.recordType]++;
    }
  }
  uint32_t id2Counter = file.getRecordCount(id2);
  // validate backward iteration
  RecordFileReader::RecordTypeCounter reverseCounters;
  for (auto riter = index.rbegin(); riter != index.rend(); ++riter) {
    const IndexRecord::RecordInfo& record = *riter;
    if (record.streamId == id2) {
      uint32_t streamTypeIndex =
          counters[record.recordType] - reverseCounters[record.recordType] - 1;
      if (streamTypeIndex % 7 != 3) {
        EXPECT_EQ(&record, file.getRecord(id2, record.recordType, streamTypeIndex));
      }
      reverseCounters[record.recordType]++;
      uint32_t streamIndex = id2Counter - reverseCounters.totalCount();
      if (streamIndex % 7 != 3) {
        EXPECT_EQ(&record, file.getRecord(id2, streamIndex));
      }
    }
  }
}
