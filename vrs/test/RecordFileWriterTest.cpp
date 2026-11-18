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

#include <chrono>
#include <cstdint>
#include <limits>
#include <memory>
#include <vector>

#include <thread_safety_analysis/Attributes.h>
#include <thread_safety_analysis/ConditionVariable.h>
#include <thread_safety_analysis/Mutex.h>
#include <vrs/DataSource.h>
#include <vrs/DiskFile.h>
#include <vrs/RecordFileWriter.h>
#include <vrs/Recordable.h>
#include <vrs/os/Utils.h>

#include <vrs/test/helpers/VRSTestsHelpers.h>

using namespace std;
using namespace vrs;
using namespace vrs::test;

namespace {

constexpr auto kLongTimeout = chrono::seconds(10);

class DataRecordable : public Recordable {
 public:
  DataRecordable() : Recordable(RecordableTypeId::UnitTest1) {
    setCompression(CompressionPreset::None);
  }
  const Record* createConfigurationRecord() override {
    return nullptr;
  }
  const Record* createStateRecord() override {
    return nullptr;
  }
  void createDataRecord(double timestamp, const vector<uint8_t>& payload) {
    createRecord(timestamp, Record::Type::DATA, 1, DataSource(DataSourceChunk(payload)));
  }
};

// Lets a test wait until a number of bytes have been written to the file.
class ObservedDiskFile : public DiskFile {
 public:
  int write(const void* buffer, size_t length) XR_TSA_REQUIRES(!mutex_) override {
    int status = DiskFile::write(buffer, length);
    {
      tsa::lock_guard<tsa::mutex> lock{mutex_};
      writtenByteCount_ += length;
    }
    written_.notify_all();
    return status;
  }
  bool waitForWrittenByteCount(uint64_t byteCount) XR_TSA_REQUIRES(!mutex_) {
    tsa::unique_lock<tsa::mutex> lock{mutex_};
    return written_.wait_for(lock, kLongTimeout, [&] { return writtenByteCount_ >= byteCount; });
  }

 private:
  tsa::mutex mutex_;
  tsa::condition_variable written_;
  uint64_t writtenByteCount_{};
};

// Holds the writer thread back when it starts, until released.
class WriterThreadGate {
 public:
  void hold() XR_TSA_REQUIRES(!mutex_) {
    tsa::unique_lock<tsa::mutex> lock{mutex_};
    released_.wait(lock, [&] { return isReleased_; });
  }
  void release() XR_TSA_REQUIRES(!mutex_) {
    {
      tsa::lock_guard<tsa::mutex> lock{mutex_};
      isReleased_ = true;
    }
    released_.notify_all();
  }

 private:
  tsa::mutex mutex_;
  tsa::condition_variable released_;
  bool isReleased_{false};
};

} // namespace

TEST(RecordFileWriterTest, writesRecordsRequestedBeforeWriterThreadWaits) {
  TemporaryRecordableInstanceIdsResetter instanceIdsResetter;
  const string path = os::getUniquePath(os::getTempFolder() + "writesRecordsRequestedEarly");
  WriterThreadGate gate;
  RecordFileWriter writer;
  writer.setInitCreatedThreadCallback([&gate](thread&, ThreadRole role, int) {
    if (role == ThreadRole::Writer) {
      gate.hold();
    }
  });
  auto file = make_unique<ObservedDiskFile>();
  ObservedDiskFile& observedFile = *file;
  ASSERT_EQ(writer.setWriteFileHandler(std::move(file)), 0);
  DataRecordable recordable;
  writer.addRecordable(&recordable);
  ASSERT_EQ(writer.createFileAsync(path), 0);
  const vector<uint8_t> payload(1024 * 1024, 42);
  recordable.createDataRecord(1, payload);
  ASSERT_EQ(writer.writeRecordsAsync(numeric_limits<double>::max()), 0);
  gate.release();
  EXPECT_TRUE(observedFile.waitForWrittenByteCount(payload.size()));
  EXPECT_EQ(writer.waitForFileClosed(), 0);
  os::remove(path);
}
