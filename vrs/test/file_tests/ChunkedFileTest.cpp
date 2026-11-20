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

#include <vrs/os/Platform.h>

#if !IS_WINDOWS_PLATFORM()
#include <sys/stat.h>
#include <unistd.h>
#endif

#include <utility>

#include <gtest/gtest.h>

#include <TestDataDir/TestDataDir.h>
#include <vrs/test/helpers/VRSTestsHelpers.h>

#include <vrs/ProgressLogger.h>
#include <vrs/StreamPlayer.h>
#include <vrs/os/Utils.h>

using namespace std;
using namespace vrs;

using coretech::getTestDataDir;

namespace {
int addPies(DiskFile& file, const string& path) {
  int status = file.create(path);
  EXPECT_EQ(status, 0);
  if (status != 0) {
    return status;
  }
  double pi = 3.14;
  EXPECT_EQ(file.write(pi), 0);
  EXPECT_EQ(file.addChunk(), 0);
  EXPECT_EQ(file.write(pi), 0);
  EXPECT_EQ(file.write(pi), 0);
  EXPECT_EQ(file.addChunk(), 0);
  EXPECT_EQ(file.write(pi), 0);
  EXPECT_EQ(file.write(pi), 0);
  EXPECT_EQ(file.write(pi), 0);
  EXPECT_EQ(file.close(), 0);
  return 0;
}

// Leaves the first chunk open with pending writes while a third chunk is added.
template <class File>
void addChunkWhileEarlierChunkHasPendingWrites(const string& path) {
  constexpr size_t kBlock = 4096;
  const string a(3 * kBlock, 'A');
  const string b(2 * kBlock, 'B');
  const string c(kBlock, 'C');
  const string d(kBlock, 'D');
  const string e(kBlock, 'E');
  File file;
  ASSERT_EQ(file.create(path, {{"buffer_size", "4096"}, {"direct", "false"}}), 0);
  ASSERT_EQ(file.write(a.data(), a.size()), 0);
  ASSERT_EQ(file.addChunk(), 0);
  ASSERT_EQ(file.write(c.data(), c.size()), 0);
  ASSERT_EQ(file.setPos(0), 0);
  ASSERT_EQ(file.overwrite(b.data(), b.size()), 0);
  ASSERT_EQ(file.setPos(a.size() + c.size()), 0);
  ASSERT_EQ(file.write(d.data(), d.size()), 0);
  ASSERT_EQ(file.addChunk(), 0);
  ASSERT_EQ(file.write(e.data(), e.size()), 0);
  ASSERT_EQ(file.close(), 0);
  DiskFile reader;
  FileSpec fileSpec;
  ASSERT_EQ(RecordFileReader::vrsFilePathToFileSpec(path, fileSpec), 0);
  ASSERT_EQ(reader.openSpec(fileSpec), 0);
  const string expected = b + a.substr(b.size()) + c + d + e;
  EXPECT_EQ(reader.getTotalSize(), static_cast<int64_t>(expected.size()));
  string actual(expected.size(), '\0');
  EXPECT_EQ(reader.read(actual.data(), actual.size()), 0);
  EXPECT_EQ(actual, expected);
  EXPECT_EQ(reader.close(), 0);
  vrs::test::deleteChunkedFile(path);
}

#if !IS_WINDOWS_PLATFORM()
// @return False if the file is still writable after chmod, as it is for root.
bool makeReadOnly(const string& path) {
  EXPECT_EQ(::chmod(path.c_str(), 0444), 0);
  return ::access(path.c_str(), W_OK) != 0;
}

class RecordConsumer : public StreamPlayer {
  bool processRecordHeader(const CurrentRecord& record, DataReference& outDataReference) override {
    buffer_.resize(record.recordSize);
    outDataReference.useVector(buffer_);
    return true;
  }
  void processRecord(const CurrentRecord& /* record */, uint32_t /* readSize */) override {}

  vector<uint8_t> buffer_;
};
#endif

} // namespace

struct ChunkedFileTester : testing::Test {
  string kChunkedFile = os::pathJoin(getTestDataDir(), "VRS_Files/chunks.vrs");
  string kChunkedFile2 = os::pathJoin(getTestDataDir(), "VRS_Files/chunks.vrs_1");
  string kChunkedFile3 = os::pathJoin(getTestDataDir(), "VRS_Files/chunks.vrs_2");
  string kMissingFile = os::pathJoin(getTestDataDir(), "VRS_Files/does_not_exist.vrs");
};

static const char* kChunkedFileStreamSignature =
    "101-462fa095330f6ac4-1-1-100,200-51b98ee8be872906-1-1-100,372-1b24bc705850ccad-1-1-100";

TEST_F(ChunkedFileTester, ChunkedFileTest) {
  vrs::RecordFileReader file;
  EXPECT_EQ(file.openFile(kChunkedFile), 0);
  EXPECT_EQ(file.getRecordCount(), 306); // number of records if all chunks are found
  EXPECT_EQ(file.getFileChunks().size(), 3);
  EXPECT_TRUE(file.mightContainImages(StreamId(RecordableTypeId::ForwardCameraRecordableClass, 1)));
  EXPECT_FALSE(file.mightContainImages(StreamId(RecordableTypeId::AudioStream, 1)));
  EXPECT_FALSE(file.mightContainImages(StreamId(RecordableTypeId::MotionRecordableClass, 1)));
  EXPECT_FALSE(file.mightContainAudio(StreamId(RecordableTypeId::ForwardCameraRecordableClass, 1)));
  EXPECT_TRUE(file.mightContainAudio(StreamId(RecordableTypeId::AudioStream, 1)));
  EXPECT_FALSE(file.mightContainAudio(StreamId(RecordableTypeId::MotionRecordableClass, 1)));
  EXPECT_EQ(file.getStreamsSignature(), kChunkedFileStreamSignature);
}

TEST_F(ChunkedFileTester, OpenChunkedFileTest) {
  vrs::RecordFileReader file;
  string jsonPath = FileSpec({kChunkedFile, kChunkedFile2, kChunkedFile3}).toJson();
  EXPECT_EQ(file.openFile(jsonPath), 0);
  EXPECT_EQ(file.getRecordCount(), 306); // number of records if all chunks are found
  EXPECT_EQ(file.getFileChunks().size(), 3);
  EXPECT_EQ(file.getStreamsSignature(), kChunkedFileStreamSignature);
}

TEST_F(ChunkedFileTester, MissingChunkChunkedFileTest) {
  vrs::RecordFileReader file;
  string jsonPath = FileSpec({kChunkedFile, kMissingFile, kChunkedFile3}).toJson();
  EXPECT_EQ(file.openFile(jsonPath), DISKFILE_FILE_NOT_FOUND);
}

#if !IS_WINDOWS_PLATFORM()

TEST_F(ChunkedFileTester, LinkedFileTest) {
  vrs::RecordFileReader file;
  // Test that if we link to the first chunk, even from a remote folder,
  // all the chunks are found & indexed
  string linkedFile = os::getTempFolder() + "chunks_link.vrs";
  EXPECT_EQ(::symlink(kChunkedFile.c_str(), linkedFile.c_str()), 0); // create a link
  ASSERT_EQ(file.openFile(linkedFile), 0);
  EXPECT_EQ(file.getRecordCount(), 306);
  EXPECT_EQ(file.getFileChunks().size(), 3);
  EXPECT_EQ(file.getStreamsSignature(), kChunkedFileStreamSignature);

  for (bool checkSignature : {false, true}) {
    FileSpec foundSpec;
    ASSERT_EQ(RecordFileReader::vrsFilePathToFileSpec(linkedFile, foundSpec, checkSignature), 0);
    ASSERT_EQ(foundSpec.chunks.size(), 3);
    EXPECT_EQ(foundSpec.fileHandlerName, DiskFile::staticName());

    EXPECT_EQ(os::getFilename(foundSpec.chunks[0]), os::getFilename(kChunkedFile));
    EXPECT_EQ(os::getFilename(foundSpec.chunks[1]), os::getFilename(kChunkedFile2));
    EXPECT_EQ(os::getFilename(foundSpec.chunks[2]), os::getFilename(kChunkedFile3));
    EXPECT_EQ(os::getFileSize(foundSpec.chunks[0]), os::getFileSize(kChunkedFile));
    EXPECT_EQ(os::getFileSize(foundSpec.chunks[1]), os::getFileSize(kChunkedFile2));
    EXPECT_EQ(os::getFileSize(foundSpec.chunks[2]), os::getFileSize(kChunkedFile3));
  }

  EXPECT_EQ(::unlink(linkedFile.c_str()), 0); // delete that link

  FileSpec spec;
  EXPECT_EQ(RecordFileReader::vrsFilePathToFileSpec(linkedFile, spec), DISKFILE_FILE_NOT_FOUND);
}

// Once the file can't be reopened for writing, it should still be readable.
TEST_F(ChunkedFileTester, reopenForUpdatesFailureKeepsFileReadOnly) {
  const string testPath = os::getTempFolder() + "reopen_read_only.vrs";
  const string data = "0123456789";
  DiskFile file;
  ASSERT_EQ(file.create(testPath), 0);
  ASSERT_EQ(file.write(data.data(), data.size()), 0);
  ASSERT_EQ(file.close(), 0);
  if (!makeReadOnly(testPath)) {
    EXPECT_EQ(os::remove(testPath), 0);
    GTEST_SKIP() << "this user can write to read-only files";
  }
  ASSERT_EQ(file.open(testPath), 0);
  EXPECT_EQ(file.reopenForUpdates(), EACCES);
  EXPECT_TRUE(file.isOpened());
  EXPECT_TRUE(file.isReadOnly());
  ASSERT_EQ(file.setPos(3), 0);
  string actual(4, '?');
  EXPECT_EQ(file.read(actual.data(), actual.size()), 0);
  EXPECT_EQ(actual, "3456");
  EXPECT_EQ(file.close(), 0);
  EXPECT_EQ(::chmod(testPath.c_str(), 0644), 0);
  EXPECT_EQ(os::remove(testPath), 0);
}

TEST_F(ChunkedFileTester, reopenForUpdatesOfAWritableFileChangesNothing) {
  const string testPath = os::getTempFolder() + "reopen_writable.vrs";
  DiskFile file;
  ASSERT_EQ(file.create(testPath), 0);
  ASSERT_EQ(file.write("0123", 4), 0);
  EXPECT_EQ(file.reopenForUpdates(), 0);
  EXPECT_FALSE(file.isReadOnly());
  ASSERT_EQ(file.write("4567", 4), 0); // continues where it was
  ASSERT_EQ(file.close(), 0);
  ASSERT_EQ(file.open(testPath), 0);
  string actual(8, '?');
  EXPECT_EQ(file.read(actual.data(), actual.size()), 0);
  EXPECT_EQ(actual, "01234567");
  EXPECT_EQ(file.close(), 0);
  EXPECT_EQ(os::remove(testPath), 0);
}

// If the file can't even be reopened read-only, it should be closed.
TEST_F(ChunkedFileTester, reopenForUpdatesFailureClosesMissingFile) {
  const string testPath = os::getTempFolder() + "reopen_missing.vrs";
  const string data = "0123456789";
  DiskFile file;
  ASSERT_EQ(file.create(testPath), 0);
  ASSERT_EQ(file.write(data.data(), data.size()), 0);
  ASSERT_EQ(file.close(), 0);
  ASSERT_EQ(file.open(testPath), 0);
  ASSERT_EQ(os::remove(testPath), 0);
  EXPECT_EQ(file.reopenForUpdates(), ENOENT);
  EXPECT_FALSE(file.isOpened());
}

// Patching the index of a read-only file fails, but the file can still be read.
TEST_F(ChunkedFileTester, openFileFixingIndexOfReadOnlyFile) {
  const string testPath = os::getTempFolder() + "fix_index_read_only.vrs";
  ASSERT_EQ(
      vrs::test::threadedCreateRecords(
          vrs::test::CreateParams(testPath).setTestOptions(
              vrs::test::TestOptions::SKIP_FINALIZE_INDEX)),
      0);
  if (!makeReadOnly(testPath)) {
    vrs::test::deleteChunkedFile(testPath);
    GTEST_SKIP() << "this user can write to read-only files";
  }
  RecordFileReader reader;
  EXPECT_EQ(reader.openFile(testPath, true), 0);
  EXPECT_TRUE(reader.isOpened());
  EXPECT_FALSE(reader.hasIndex());
  EXPECT_EQ(reader.getIndex().size(), kClassicFileConfig.totalRecordCount);
  RecordConsumer consumer;
  for (StreamId id : reader.getStreams()) {
    reader.setStreamPlayer(id, &consumer);
  }
  EXPECT_EQ(reader.readAllRecords(), 0);
  EXPECT_EQ(reader.closeFile(), 0);
  EXPECT_EQ(::chmod(testPath.c_str(), 0644), 0);
  vrs::test::deleteChunkedFile(testPath);
}

namespace {
class RemoveFileWhileReindexing : public SilentLogger {
 public:
  explicit RemoveFileWhileReindexing(string path) : path_{std::move(path)} {}
  bool logProgress(const string& stepName, size_t, size_t, bool) override {
    if (stepName == "Reindexing" && !removed_) {
      removed_ = os::remove(path_) == 0;
    }
    return true;
  }
  bool removed() const {
    return removed_;
  }

 private:
  string path_;
  bool removed_{};
};
} // namespace

// If the file vanishes while its index is patched, opening it fails cleanly.
TEST_F(ChunkedFileTester, openFileFixingIndexOfRemovedFile) {
  const string testPath = os::getTempFolder() + "fix_index_removed.vrs";
  ASSERT_EQ(
      vrs::test::threadedCreateRecords(
          vrs::test::CreateParams(testPath).setTestOptions(
              vrs::test::TestOptions::SKIP_FINALIZE_INDEX)),
      0);
  RemoveFileWhileReindexing logger(testPath);
  RecordFileReader reader;
  reader.setOpenProgressLogger(&logger);
  EXPECT_EQ(reader.openFile(testPath, true), ENOENT);
  EXPECT_TRUE(logger.removed());
  EXPECT_FALSE(reader.isOpened());
  EXPECT_TRUE(reader.getStreams().empty());
  EXPECT_TRUE(reader.getIndex().empty());
  EXPECT_TRUE(reader.getTags().empty());
}
#endif // !IS_WINDOWS_PLATFORM

TEST_F(ChunkedFileTester, newChunks) {
  const string testPath = os::getTempFolder() + "chunking.vrs";

  // test regular chunking: path not ending with "_1", path + "_1", path + "_2", etc
  DiskFile file;
  ASSERT_EQ(addPies(file, testPath), 0);
  FileSpec fileSpec;
  ASSERT_EQ(RecordFileReader::vrsFilePathToFileSpec(testPath, fileSpec), 0);
  file.openSpec(fileSpec);
  vector<pair<string, int64_t>> chunks = file.getFileChunks();
  ASSERT_EQ(chunks.size(), 3);
  EXPECT_EQ(chunks[0], make_pair(testPath, static_cast<int64_t>(sizeof(double))));
  EXPECT_EQ(chunks[1], make_pair(testPath + "_1", static_cast<int64_t>(2 * sizeof(double))));
  EXPECT_EQ(chunks[2], make_pair(testPath + "_2", static_cast<int64_t>(3 * sizeof(double))));

  // test regular split-head chunking: path ending with "_1", path + "_2", path + "_3", etc
  ASSERT_EQ(addPies(file, testPath + "_1"), 0);
  // we open testPath, and find it + 3 chunks
  ASSERT_EQ(RecordFileReader::vrsFilePathToFileSpec(testPath, fileSpec), 0);
  file.openSpec(fileSpec);
  chunks = file.getFileChunks();
  ASSERT_EQ(chunks.size(), 4);
  EXPECT_EQ(chunks[0], make_pair(testPath, static_cast<int64_t>(sizeof(double))));
  EXPECT_EQ(chunks[1], make_pair(testPath + "_1", static_cast<int64_t>(sizeof(double))));
  EXPECT_EQ(chunks[2], make_pair(testPath + "_2", static_cast<int64_t>(2 * sizeof(double))));
  EXPECT_EQ(chunks[3], make_pair(testPath + "_3", static_cast<int64_t>(3 * sizeof(double))));
  vrs::test::deleteChunkedFile(file);
}

TEST_F(ChunkedFileTester, openSpecWithoutChunks) {
  DiskFile file;
  EXPECT_EQ(file.openSpec(FileSpec{}), INVALID_FILE_SPEC);
  EXPECT_FALSE(file.isOpened());
}

TEST_F(ChunkedFileTester, setPosAcrossChunks) {
  const string testPath = os::getTempFolder() + "set_pos_chunks.vrs";
  const string a(10, 'A');
  const string c(10, 'C');
  const string d(10, 'D');
  DiskFile file;
  ASSERT_EQ(file.create(testPath), 0);
  ASSERT_EQ(file.write(a.data(), a.size()), 0);
  ASSERT_EQ(file.addChunk(), 0); // empty chunk
  ASSERT_EQ(file.addChunk(), 0);
  ASSERT_EQ(file.write(c.data(), c.size()), 0);
  ASSERT_EQ(file.addChunk(), 0);
  ASSERT_EQ(file.write(d.data(), d.size()), 0);
  ASSERT_EQ(file.close(), 0);
  FileSpec fileSpec;
  ASSERT_EQ(RecordFileReader::vrsFilePathToFileSpec(testPath, fileSpec), 0);
  ASSERT_EQ(file.openSpec(fileSpec), 0);
  ASSERT_EQ(file.getFileChunks().size(), 4);
  const vector<pair<int64_t, char>> expected = {
      {25, 'D'}, {0, 'A'}, {10, 'C'}, {9, 'A'}, {29, 'D'}, {19, 'C'}, {20, 'D'}, {5, 'A'}};
  for (const auto& [offset, value] : expected) {
    ASSERT_EQ(file.setPos(offset), 0);
    char actual = 0;
    ASSERT_EQ(file.read(actual), 0);
    EXPECT_EQ(actual, value) << "at offset " << offset;
  }
  EXPECT_EQ(file.setPos(30), 0);
  EXPECT_EQ(file.setPos(31), DISKFILE_INVALID_OFFSET);
  EXPECT_EQ(file.setPos(-1), DISKFILE_INVALID_OFFSET);
  vrs::test::deleteChunkedFile(file);
}

TEST_F(ChunkedFileTester, forgetFurtherChunksKeepsTheCurrentChunk) {
  const string testPath = os::getTempFolder() + "forget_chunks.vrs";
  DiskFile file;
  ASSERT_EQ(addPies(file, testPath), 0); // chunks at offsets 0, 8 and 24
  file.forgetFurtherChunks(0); // a closed file has nothing to forget
  FileSpec fileSpec;
  ASSERT_EQ(RecordFileReader::vrsFilePathToFileSpec(testPath, fileSpec), 0);
  ASSERT_EQ(file.openSpec(fileSpec), 0);
  const auto secondChunk = static_cast<int64_t>(sizeof(double));
  ASSERT_EQ(file.setPos(secondChunk), 0);
  file.forgetFurtherChunks(secondChunk);
  EXPECT_EQ(file.getFileChunks().size(), 2);
  EXPECT_EQ(file.getPos(), secondChunk);
  EXPECT_EQ(file.close(), 0);
  vrs::test::deleteChunkedFile(testPath);
}

TEST_F(ChunkedFileTester, truncateMovesTheFollowingChunks) {
  const string testPath = os::getTempFolder() + "truncate_chunks.vrs";
  const string data(100, 'A');
  DiskFile file;
  ASSERT_EQ(file.create(testPath), 0);
  ASSERT_EQ(file.write(data.data(), data.size()), 0);
  for (int chunk = 1; chunk < 4; ++chunk) {
    ASSERT_EQ(file.addChunk(), 0);
    ASSERT_EQ(file.write(data.data(), data.size()), 0);
  }
  ASSERT_EQ(file.setPos(150), 0);
  ASSERT_EQ(file.truncate(), 0); // the second chunk is now 50 bytes
  ASSERT_EQ(file.setPos(160), 0);
  int64_t chunkOffset = 0;
  int64_t chunkSize = 0;
  ASSERT_EQ(file.getChunkRange(chunkOffset, chunkSize), 0);
  EXPECT_EQ(chunkOffset, 150);
  EXPECT_EQ(chunkSize, 100);
  vrs::test::deleteChunkedFile(file);
}

#if !IS_WINDOWS_PLATFORM()
TEST_F(ChunkedFileTester, switchingBackToAChunkReturnsItsFlushError) {
  // Everything happens in the child process: gtest flushes all stdio buffers before forking.
  ASSERT_EXIT(
      {
        const string testPath = os::getTempFolder() + "flush_error_chunks.vrs";
        const string data(100, 'A');
        DiskFile file;
        ASSERT_EQ(file.create(testPath), 0);
        ASSERT_EQ(file.write(data.data(), data.size()), 0);
        ASSERT_EQ(file.addChunk(), 0);
        ASSERT_EQ(file.write(data.data(), data.size()), 0);
        ASSERT_EQ(file.setPos(0), 0); // reopens the first chunk for writing
        ASSERT_EQ(file.overwrite(data.data(), 50), 0); // buffered by stdio
        ASSERT_EQ(file.setPos(100), 0);
        vrs::test::limitFileSize(10); // so the buffered write can't be flushed
        const int error = file.setPos(0);
        vrs::test::removeFileSizeLimit();
        EXPECT_EQ(error, EFBIG);
        vrs::test::deleteChunkedFile(file);
        _exit(testing::Test::HasFailure() ? 1 : 0);
      },
      testing::ExitedWithCode(0),
      "");
}
#endif

TEST_F(ChunkedFileTester, addChunkWhileEarlierChunkHasPendingWrites) {
  addChunkWhileEarlierChunkHasPendingWrites<DiskFile>(os::getTempFolder() + "pending.vrs");
}

#if VRS_ASYNC_DISKFILE_SUPPORTED()
TEST_F(ChunkedFileTester, asyncAddChunkWhileEarlierChunkHasPendingWrites) {
  addChunkWhileEarlierChunkHasPendingWrites<AsyncDiskFile>(
      os::getTempFolder() + "async_pending.vrs");
}
#endif
