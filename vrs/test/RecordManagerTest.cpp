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

#include <gtest/gtest.h>

#include <vrs/DataSource.h>
#include <vrs/RecordManager.h>
#include <vrs/os/Time.h>

using namespace std;
using namespace vrs;

TEST(RecordManager, CollectOldRecords) {
  RecordManager manager;
  constexpr uint32_t kDataVersion = 1337;
  constexpr int kFrameDt = 1;
  vector<uint8_t> data(10);

  // Create a bunch of records!
  int frameIndex = 0;
  double timestamp = 0.0;
  for (int i = 0; i < 1000; ++i) {
    manager.createRecord(timestamp, Record::Type::DATA, kDataVersion, DataSource(frameIndex, data));
    frameIndex += kFrameDt;
    timestamp = static_cast<double>(frameIndex) / 100.0;
  }

  // Verify that we can pull a subsection of the records.
  list<Record*> recordData;
  manager.collectOldRecords(1.33, recordData);
  EXPECT_EQ(recordData.size(), 134);
  EXPECT_DOUBLE_EQ(recordData.front()->getTimestamp(), 0.0);
  EXPECT_DOUBLE_EQ(recordData.back()->getTimestamp(), 1.33);
  for (auto* r : recordData) {
    r->recycle();
  }

  // Call backwards in time. There shouldn't be anything.
  list<Record*> noRecords;
  manager.collectOldRecords(0.74, noRecords);
  EXPECT_TRUE(noRecords.empty());

  // Purge the rest of the records.
  EXPECT_EQ(1000 - 134, manager.purgeOldRecords(10.0));

  // Call it again. There shouldn't be anymore.
  manager.collectOldRecords(1000.0, noRecords);
  EXPECT_TRUE(noRecords.empty());
}

TEST(RecordManager, Recycle) {
  RecordManager manager;
  constexpr uint32_t kDataVersion = 1337;
  vector<uint8_t> data;
  manager.setMaxCacheSize(5);
  EXPECT_EQ(manager.getCurrentCacheSize(), 0);

  data.resize(100);
  manager.createRecord(os::getTimestampSec(), Record::Type::DATA, kDataVersion, DataSource(data));
  EXPECT_EQ(manager.getCurrentCacheSize(), 0);

  list<Record*> records;
  manager.collectOldRecords(os::getTimestampSec(), records);
  EXPECT_EQ(records.size(), 1);

  records.front()->recycle();
  EXPECT_EQ(manager.getCurrentCacheSize(), 1);
  manager.purgeCache();
  EXPECT_EQ(manager.getCurrentCacheSize(), 0);
}

namespace {
constexpr uint32_t kDataVersion = 1337;
void collectAndRecycle(RecordManager& manager, double maxAge = numeric_limits<double>::max()) {
  list<Record*> records;
  manager.collectOldRecords(maxAge, records);
  for (auto record : records) {
    record->recycle();
  }
}
void resetManager(RecordManager& manager) {
  collectAndRecycle(manager);
  manager.purgeCache();
  EXPECT_EQ(manager.getCurrentCacheSize(), 0);
}
void testAllocationLimit(RecordManager& manager, size_t firstSize, size_t maxSize) {
  resetManager(manager);
  double now = os::getTimestampSec();
  vector<uint8_t> data(firstSize);
  Record* record = manager.createRecord(now, Record::Type::DATA, kDataVersion, DataSource(data));
  collectAndRecycle(manager);
  ASSERT_EQ(manager.getCurrentCacheSize(), 1);
  data.resize(maxSize);
  // we should get this record from the cache
  Record* record2 = manager.createRecord(now, Record::Type::DATA, kDataVersion, DataSource(data));
  EXPECT_EQ(record, record2);
  EXPECT_EQ(manager.getCurrentCacheSize(), 0);
  collectAndRecycle(manager);
  data.resize(maxSize + 1);
  // we should NOT get this record from the cache
  record2 = manager.createRecord(now, Record::Type::DATA, kDataVersion, DataSource(data));
  EXPECT_NE(record, record2);
  EXPECT_EQ(manager.getCurrentCacheSize(), 1);
}
} // namespace

TEST(RecordManager, RecordSize) {
  RecordManager manager;

  // test default allocation strategy
  testAllocationLimit(manager, 500, 500);
}
