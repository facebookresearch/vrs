// Copyright (c) Facebook, Inc. and its affiliates. Confidential and proprietary.

#include <memory>

#include <gtest/gtest.h>

#define DEFAULT_LOG_CHANNEL "DeviceSimulator"
#include <logging/Log.h>
#include <portability/Filesystem.h>
#include <vrs/RecordFileReader.h>
#include <vrs/os/Utils.h>

#include <vrs/test/helpers/VRSTestsHelpers.h>

using namespace vrs;
using namespace vrs::test;

namespace {

struct DeviceSimulator : testing::Test {};

} // namespace

TEST_F(DeviceSimulator, classicIndex) {
  const std::string testPath = os::getTempFolder() + "ClassicIndex.vrs";

  CreateParams t(testPath);
  EXPECT_EQ(threadedCreateRecords(t), 0);

  RecordFileReader reader;
  ASSERT_EQ(reader.openFile(testPath), 0);
  size_t recordCount = reader.getIndex().size();
  EXPECT_EQ(recordCount, kClassicFileConfig.totalRecordCount);
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
  const std::string testPath = os::getTempFolder() + "SingleThread.vrs";

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

TEST_F(DeviceSimulator, preallocateIndex) {
  const std::string testPath = os::getTempFolder() + "PreallocateTest.vrs";

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
  const std::string testPath = os::getTempFolder() + "PreallocateTooFewTest.vrs";

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
  const std::string testPath = os::getTempFolder() + "PreallocateTooManyTest.vrs";

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
  ChunkCollector(map<size_t, string>& _chunks) : chunks{_chunks} {
    chunks.clear();
  }
  void newChunk(const string& path, size_t index, bool isLastChunk) override {
    EXPECT_EQ(chunks.find(index), chunks.end());
    chunks[index] = path;
  }
  map<size_t, string>& chunks;
};

void checkChunks(const map<size_t, std::string>& chunks, const std::string& path, size_t count) {
  EXPECT_EQ(chunks.size(), count);
  if (chunks.size() > 0) {
    auto iter = chunks.begin();
    EXPECT_STREQ(iter->second.c_str(), path.c_str());
    EXPECT_EQ(iter->first, 0);
    size_t index = 0;
    while (++iter != chunks.end()) {
      ++index;
      EXPECT_EQ(iter->first, index);
      EXPECT_STREQ(iter->second.c_str(), (path + '_' + std::to_string(index)).c_str());
    }
  }
}

TEST_F(DeviceSimulator, splitIndex) {
  const std::string testPath = os::getTempFolder() + "SplitIndex.vrs";

  map<size_t, string> chunks;
  EXPECT_EQ(
      threadedCreateRecords(CreateParams(testPath, kLongFileConfig)
                                .setTestOptions(TestOptions::SPLIT_HEADER)
                                .setMaxChunkSizeMB(1)
                                .setChunkHandler(std::make_unique<ChunkCollector>(chunks))),
      0);
  checkRecordCountAndIndex(CheckParams(testPath, kLongFileConfig)); // baseline: all ok
  checkChunks(chunks, testPath, 3);

  // truncate the file to corrupt the index
  int64_t fileSize = os::getFileSize(testPath);
  ::filesystem::resize_file(testPath, static_cast<uintmax_t>(fileSize - 1));
  // We fix the index, but we won't have to jump back a single time with the fixed file
  checkRecordCountAndIndex(
      CheckParams(testPath, kLongFileConfig).setHasIndex(false).setJumpbackCount(2));
  deleteChunkedFile(testPath);

  const int64_t indexRecordHeaderEnd = 1752; // exact known location

  // cut out all the index
  EXPECT_EQ(
      threadedCreateRecords(CreateParams(testPath, kLongFileConfig)
                                .setTestOptions(TestOptions::SPLIT_HEADER)
                                .setMaxChunkSizeMB(100)
                                .setChunkHandler(std::make_unique<ChunkCollector>(chunks))),
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
