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

#include <memory>

#include <gtest/gtest.h>

#define DEFAULT_LOG_CHANNEL "DeviceSimulator"
#include <logging/Log.h>

#include <portability/Filesystem.h>

#include <vrs/DiskFile.h>
#include <vrs/RecordFileReader.h>
#include <vrs/os/Platform.h>
#include <vrs/os/Utils.h>

#include <vrs/test/helpers/VRSTestsHelpers.h>

using namespace std;
using namespace vrs;
using namespace vrs::test;

namespace {

struct DeviceSimulator : testing::Test {};

} // namespace

TEST_F(DeviceSimulator, classicIndex) {
  const string testPath = os::getTempFolder() + "ClassicIndex.vrs";

  CreateParams t(testPath);
  EXPECT_EQ(threadedCreateRecords(t), 0);

  RecordFileReader reader;
  ASSERT_EQ(reader.openFile(testPath), 0);
  size_t recordCount = reader.getIndex().size();
  EXPECT_EQ(recordCount, kClassicFileConfig.totalRecordCount);

  // validate serial number methods
  set<string> serialNumbers;
  for (uint32_t k = 0; k < kCameraCount; ++k) {
    string tagName = CreateParams::getCameraStreamTag(k);
    string serialNumber = reader.getTag(tagName);
    EXPECT_FALSE(serialNumber.empty());
    serialNumbers.insert(serialNumber);
    EXPECT_TRUE(reader.getStreamForSerialNumber(serialNumber).isValid());
  }
  EXPECT_EQ(serialNumbers.size(), kCameraCount);

  reader.closeFile();

  // truncate the file to corrupt the index
  int64_t fileSize = os::getFileSize(testPath);
  ::filesystem::resize_file(testPath, static_cast<uintmax_t>(fileSize - 1));
  checkRecordCountAndIndex(
      CheckParams(testPath).setHasIndex(false).setJumpbackCount(2).setJumpbackAfterFixingIndex(
          1)); // index needed to be rebuilt

  deleteChunkedFile(testPath);
}

TEST_F(DeviceSimulator, singleThread) {
  const string testPath = os::getTempFolder() + "SingleThread.vrs";

  CreateParams t(testPath);
  EXPECT_EQ(singleThreadCreateRecords(t), 0);

  checkRecordCountAndIndex(CheckParams(testPath));

  // truncate the file to corrupt the last record, which isn't the index
  int64_t fileSize = os::getFileSize(testPath);
  ::filesystem::resize_file(testPath, static_cast<uintmax_t>(fileSize - 1));
  // the index was fine, but we lost 1 record
  checkRecordCountAndIndex(CheckParams(testPath).setTruncatedUserRecords(1));

  deleteChunkedFile(testPath);
}

#if VRS_ASYNC_DISKFILE_SUPPORTED()
TEST_F(DeviceSimulator, singleThreadAsync) {
  const string testPath = os::getTempFolder() + "SingleThreadAsync.vrs";

  CreateParams t(testPath, kLongFileConfig);
  t.useAsyncDiskFile();
  EXPECT_EQ(singleThreadCreateRecords(t), 0);

  checkRecordCountAndIndex(CheckParams(testPath, kLongFileConfig).setJumpbackCount(1));

  deleteChunkedFile(testPath);
}

TEST_F(DeviceSimulator, multiThreadAsyncAioDirect) {
  const string testPath = os::getTempFolder() + "MultiThreadAsyncAioDirect.vrs";

  CreateParams t(testPath, kVeryLongFileConfig);
  t.useAsyncDiskFile("ioengine=aio").setTestOptions(0);
  EXPECT_EQ(threadedCreateRecords(t), 0);

  checkRecordCountAndIndex(CheckParams(testPath, kVeryLongFileConfig).setJumpbackCount(1));

  deleteChunkedFile(testPath);
}

TEST_F(DeviceSimulator, multiThreadAsyncAioNotDirect) {
  const string testPath = os::getTempFolder() + "MultiThreadAsyncAioNotDirect.vrs";

  CreateParams t(testPath, kVeryLongFileConfig);
  t.useAsyncDiskFile("ioengine=aio&direct=false").setTestOptions(0);
  EXPECT_EQ(threadedCreateRecords(t), 0);

  checkRecordCountAndIndex(CheckParams(testPath, kVeryLongFileConfig).setJumpbackCount(1));

  deleteChunkedFile(testPath);
}

#if IS_VRS_FB_INTERNAL() || !IS_WINDOWS_PLATFORM() // avoid OSS/Windows
TEST_F(DeviceSimulator, multiThreadAsyncSync) {
  const string testPath = os::getTempFolder() + "MultiThreadAsyncSync.vrs";

  CreateParams t(testPath, kVeryLongFileConfig);
  t.useAsyncDiskFile("ioengine=sync").setTestOptions(0);
  EXPECT_EQ(threadedCreateRecords(t), 0);

  checkRecordCountAndIndex(CheckParams(testPath, kVeryLongFileConfig).setJumpbackCount(1));

  deleteChunkedFile(testPath);
}
#endif

TEST_F(DeviceSimulator, multiThreadAsyncPsync) {
  const string testPath = os::getTempFolder() + "MultiThreadAsyncPsync.vrs";

  CreateParams t(testPath, kVeryLongFileConfig);
  t.useAsyncDiskFile("ioengine=psync").setTestOptions(0);
  EXPECT_EQ(threadedCreateRecords(t), 0);

  checkRecordCountAndIndex(CheckParams(testPath, kVeryLongFileConfig).setJumpbackCount(1));

  deleteChunkedFile(testPath);
}
#endif

TEST_F(DeviceSimulator, preallocateIndex) {
  const string testPath = os::getTempFolder() + "PreallocateTest.vrs";

  // preallocate for the exact number of records
  EXPECT_EQ(
      threadedCreateRecords(
          CreateParams(testPath).setPreallocateIndexSize(kClassicFileConfig.totalRecordCount)),
      0);
  checkRecordCountAndIndex(CheckParams(testPath));

  // truncate the file to corrupt the last record, which isn't the index
  int64_t fileSize = os::getFileSize(testPath);
  ::filesystem::resize_file(testPath, static_cast<uintmax_t>(fileSize - 1));
  // the index was fine, but we lost 1 record
  checkRecordCountAndIndex(CheckParams(testPath).setTruncatedUserRecords(1));

  deleteChunkedFile(testPath);
}

TEST_F(DeviceSimulator, preallocateTooFewIndex) {
  const string testPath = os::getTempFolder() + "PreallocateTooFewTest.vrs";

  // preallocate for too few records
  EXPECT_EQ(threadedCreateRecords(CreateParams(testPath).setPreallocateIndexSize(5)), 0);
  checkRecordCountAndIndex(CheckParams(testPath).setJumpbackCount(1));

  // truncate the file to corrupt the last record, which is the index record,
  // since the preallocation was way too small
  int64_t fileSize = os::getFileSize(testPath);
  ::filesystem::resize_file(testPath, static_cast<uintmax_t>(fileSize - 1));
  // index needed to be rebuilt
  checkRecordCountAndIndex(
      CheckParams(testPath).setHasIndex(false).setJumpbackCount(2).setJumpbackAfterFixingIndex(1));

  deleteChunkedFile(testPath);
}

TEST_F(DeviceSimulator, preallocateTooManyIndex) {
  const string testPath = os::getTempFolder() + "PreallocateTooManyTest.vrs";

  // preallocate for too many records
  EXPECT_EQ(
      threadedCreateRecords(
          CreateParams(testPath).setPreallocateIndexSize(kClassicFileConfig.totalRecordCount + 1)),
      0);
  checkRecordCountAndIndex(CheckParams(testPath));

  // truncate the file to corrupt the last record, which isn't the index now
  int64_t fileSize = os::getFileSize(testPath);
  ::filesystem::resize_file(testPath, static_cast<uintmax_t>(fileSize - 1));
  // the index was fine, but we lost 1 record
  checkRecordCountAndIndex(CheckParams(testPath).setTruncatedUserRecords(1));

  deleteChunkedFile(testPath);
}

struct ChunkCollector : public NewChunkHandler {
  explicit ChunkCollector(map<size_t, string>& _chunks) : chunks{_chunks} {
    chunks.clear();
  }
  void newChunk(const string& path, size_t index, bool /*isLastChunk*/) override {
    EXPECT_EQ(chunks.find(index), chunks.end());
    chunks[index] = path;
  }
  map<size_t, string>& chunks;
};

void checkChunks(const map<size_t, string>& chunks, const string& path, size_t count) {
  EXPECT_EQ(chunks.size(), count);
  if (!chunks.empty()) {
    auto iter = chunks.begin();
    EXPECT_STREQ(iter->second.c_str(), path.c_str());
    EXPECT_EQ(iter->first, 0);
    size_t index = 0;
    while (++iter != chunks.end()) {
      ++index;
      EXPECT_EQ(iter->first, index);
      EXPECT_STREQ(iter->second.c_str(), (path + '_' + to_string(index)).c_str());
    }
  }
}

TEST_F(DeviceSimulator, splitIndex) {
  const string testPath = os::getTempFolder() + "SplitIndex.vrs";

  map<size_t, string> chunks;
  CreateParams creationParams(testPath, kLongFileConfig);
  creationParams.setTestOptions(TestOptions::SPLIT_HEADER)
      .setMaxChunkSizeMB(1)
      .setChunkHandler(make_unique<ChunkCollector>(chunks));
  EXPECT_EQ(threadedCreateRecords(creationParams), 0);
  checkRecordCountAndIndex(CheckParams(testPath, kLongFileConfig)); // baseline: all ok
  checkChunks(chunks, testPath, 3);

  // truncate the file to corrupt the index
  int64_t fileSize = os::getFileSize(testPath);
  ::filesystem::resize_file(testPath, static_cast<uintmax_t>(fileSize - 1));
  // We fix the index, but we won't have to jump back a single time with the fixed file
  checkRecordCountAndIndex(
      CheckParams(testPath, kLongFileConfig).setHasIndex(false).setJumpbackCount(2));
  deleteChunkedFile(testPath);

  const int64_t indexRecordHeaderEnd = creationParams.outMinFileSize;

  // cut out all the index
  EXPECT_EQ(
      threadedCreateRecords(CreateParams(testPath, kLongFileConfig)
                                .setTestOptions(TestOptions::SPLIT_HEADER)
                                .setMaxChunkSizeMB(100)
                                .setChunkHandler(make_unique<ChunkCollector>(chunks))),
      0);
  checkChunks(chunks, testPath, 2);
  ::filesystem::resize_file(testPath, static_cast<uintmax_t>(indexRecordHeaderEnd));
  checkRecordCountAndIndex(
      CheckParams(testPath, kLongFileConfig).setHasIndex(false).setJumpbackCount(2));
  deleteChunkedFile(testPath);

  // cut part of the index
  EXPECT_EQ(
      threadedCreateRecords(CreateParams(testPath, kLongFileConfig)
                                .setTestOptions(TestOptions::SPLIT_HEADER)
                                .setMaxChunkSizeMB(100)),
      0);
  const int64_t headFileSize = os::getFileSize(testPath);
  ::filesystem::resize_file(
      testPath, static_cast<uintmax_t>((indexRecordHeaderEnd + headFileSize) / 2 - 3));
  checkRecordCountAndIndex(
      CheckParams(testPath, kLongFileConfig).setHasIndex(false).setJumpbackCount(2));
  deleteChunkedFile(testPath);

  // Simulate an interrupted recording:
  // The index record was not finalized, which means that the file's header and the index record's
  // record header are initialized, but not up-to-date. In particular:
  // - the location (offset) of the first user record is missing.
  //   That record comes after the index record, which size can't be known upfront.
  // - the index record's size is also missing for the same reason.
  //   The index record might contain *some* data, but we don't know how much upfront.
  // The only way to tell is by looking at the size of the first chunk: it should tell us where
  // the index record ends (everything before the end of that first chunk), and where the first
  // user record starts: right at the start of the second chunk.
  // When trying to fix the index of a file with a split index, we will rewrite the index at the end
  // of the first chunk, which means the file is immediately suitable for streaming.

  // This file is short: we end up writing nothing in the index & rebuilding it all
  EXPECT_EQ(
      threadedCreateRecords(
          CreateParams(testPath)
              .setTestOptions(TestOptions::SPLIT_HEADER + TestOptions::SKIP_FINALIZE_INDEX)
              .setMaxChunkSizeMB(100)),
      0);
  checkRecordCountAndIndex(CheckParams(testPath).setHasIndex(false).setJumpbackCount(2));
  deleteChunkedFile(testPath);

  // This file is much longer, so we write a large part of the index, and rebuild the rest
  EXPECT_EQ(
      threadedCreateRecords(
          CreateParams(testPath, kLongFileConfig)
              .setTestOptions(TestOptions::SPLIT_HEADER + TestOptions::SKIP_FINALIZE_INDEX)
              .setMaxChunkSizeMB(100)),
      0);
  checkRecordCountAndIndex(
      CheckParams(testPath, kLongFileConfig).setHasIndex(false).setJumpbackCount(2));
  deleteChunkedFile(testPath);
}
