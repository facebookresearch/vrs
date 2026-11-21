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

#include <vrs/AsyncDiskFileChunk.h>

#if VRS_ASYNC_DISKFILE_SUPPORTED()
#if POSIX_AIO_SUPPORTED()

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <algorithm>
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include <vrs/DiskFile.h>
#include <vrs/os/Utils.h>
#include <vrs/test/helpers/VRSTestsHelpers.h>

namespace vrs {

class AsyncDiskFileChunkTestPeer {
 public:
  static void recordAsyncError(AsyncDiskFileChunk& chunk, int error) {
    chunk.recordAsyncError(error);
  }

  static bool buffersWrites(const AsyncDiskFileChunk& chunk) {
    return chunk.ioengine_ != AsyncDiskFileChunk::IoEngine::Sync;
  }

  static bool usesDirectIo(const AsyncDiskFileChunk& chunk) {
    return (chunk.current_flags_ & O_DIRECT) != 0;
  }

  static bool hasWriteBuffers(const AsyncDiskFileChunk& chunk) {
    return !chunk.buffers_.empty();
  }

  static std::vector<size_t> bufferCapacities(const AsyncDiskFileChunk& chunk) {
    std::vector<size_t> capacities;
    capacities.reserve(chunk.buffers_.size());
    for (const auto& buffer : chunk.buffers_) {
      capacities.push_back(buffer->capacity());
    }
    return capacities;
  }

  // Wait until an in-flight write finishes, without completing it.
  static void waitForAWriteToFinish(AsyncDiskFileChunk& chunk) {
    ASSERT_FALSE(chunk.buffers_writing_.empty());
    chunk.waitForAWriteToFinish();
  }

  // The file offset where the buffer being filled starts.
  static int64_t bufferStart(const AsyncDiskFileChunk& chunk) {
    return chunk.file_position_;
  }

  static void verifyFailedSynchronousFallbackIsSticky(AsyncDiskFileChunk& chunk) {
    AsyncBuffer* buffer = nullptr;
    enqueue(chunk, buffer, 0x72);
    test::limitFileSize(2048); // cuts the 4096-byte write short, leaves room for the log lines
    chunk.pump_buffers([](AsyncDiskFileChunk::QueuedWrite& item) {
      return item.buffer_->startWrite(item.file_, item.offset_, rejectAgain);
    });
    test::removeFileSizeLimit();
    const int error = chunk.flush();
    EXPECT_NE(error, SUCCESS);
    size_t written = 0;
    EXPECT_EQ(chunk.write("x", 1, written), error);
    EXPECT_EQ(chunk.close(), error);
  }

  static std::string readFile(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
  }

  static void verifyRejectedSubmissionIsWrittenSynchronously(AsyncDiskFileChunk& chunk) {
    AsyncBuffer* buffer = nullptr;
    enqueue(chunk, buffer, 0x72);
    chunk.pump_buffers([](AsyncDiskFileChunk::QueuedWrite& item) {
      return item.buffer_->startWrite(item.file_, item.offset_, captureNotificationAndReject);
    });
    EXPECT_EQ(sNotification, SIGEV_NONE);
    EXPECT_TRUE(chunk.buffers_writing_.empty());
    EXPECT_TRUE(chunk.buffers_queued_.empty());
    EXPECT_EQ(chunk.buffers_free_.size(), chunk.buffers_.size());
    EXPECT_EQ(chunk.flush(), SUCCESS);
    expectContent(chunk, std::string(chunk.buffer_size_, 0x72));
    EXPECT_EQ(chunk.close(), SUCCESS);
  }

  static void verifyMixedSubmissionsAreAllWritten(AsyncDiskFileChunk& chunk) {
    AsyncBuffer* buffer = nullptr;
    enqueue(chunk, buffer, 0x11);
    enqueue(chunk, buffer, 0x22);
    enqueue(chunk, buffer, 0x33);
    const std::string tail(17, 0x43);
    bufferTail(chunk, tail);
    size_t submissions = 0;
    chunk.pump_buffers([&submissions](AsyncDiskFileChunk::QueuedWrite& item) {
      ++submissions;
      if (submissions == 2) {
        return item.buffer_->start_write(item.file_, item.offset_);
      }
      return item.buffer_->startWrite(
          item.file_, item.offset_, submissions == 1 ? rejectAgain : rejectNoMemory);
    });
    EXPECT_EQ(submissions, 3);
    EXPECT_TRUE(chunk.buffers_queued_.empty());
    EXPECT_EQ(chunk.flush(), SUCCESS);
    const size_t size = chunk.buffer_size_;
    expectContent(
        chunk, std::string(size, 0x11) + std::string(size, 0x22) + std::string(size, 0x33) + tail);
    EXPECT_EQ(chunk.close(), SUCCESS);
  }

  static void verifyFailedAsyncWriteErrorPersistsUntilClose(
      AsyncDiskFileChunk& chunk,
      const std::string& path) {
    AsyncBuffer* failed = nullptr;
    enqueue(chunk, failed, 0x72);
    const std::string tail(17, 0x43);
    bufferTail(chunk, tail);
    chunk.pump_buffers([](AsyncDiskFileChunk::QueuedWrite& item) {
      return item.buffer_->startWrite(item.file_, item.offset_, acceptWithoutWriting);
    });
    // Complete the write that was never submitted before anything polls it.
    ASSERT_EQ(chunk.buffers_writing_.size(), 1);
    chunk.buffers_writing_.clear();
    chunk.complete_write(failed, -1, EIO);
    size_t written = 0;
    chunk.recordAsyncError(ENOSPC); // the first error is the one kept
    EXPECT_EQ(chunk.write(tail.data(), tail.size(), written), EIO);
    EXPECT_EQ(written, 0u);
    EXPECT_EQ(chunk.flush(), EIO);
    EXPECT_EQ(chunk.flush(), EIO);
    EXPECT_EQ(chunk.rewind(), EIO);
    EXPECT_EQ(chunk.close(), EIO);
    EXPECT_FALSE(chunk.isOpened());
    // The tail accepted before the failure is still written, at its own offset.
    EXPECT_EQ(readFile(path), std::string(chunk.buffer_size_, '\0') + tail);
    // Reopening clears the error.
    ASSERT_EQ(chunk.open(false, {}), SUCCESS);
    EXPECT_EQ(chunk.write(tail.data(), tail.size(), written), SUCCESS);
    EXPECT_EQ(chunk.close(), SUCCESS);
  }

 private:
  using ControlBlock = decltype(AsyncBuffer::aiocb_);

  static inline int sNotification = -1;

  static int captureNotificationAndReject(ControlBlock* controlBlock) {
    sNotification = controlBlock->aio_sigevent.sigev_notify;
    errno = EAGAIN;
    return -1;
  }

  static int rejectAgain(ControlBlock*) {
    errno = EAGAIN;
    return -1;
  }

  static int rejectNoMemory(ControlBlock*) {
    errno = ENOMEM;
    return -1;
  }

  static int acceptWithoutWriting(ControlBlock*) {
    return SUCCESS;
  }

  static void enqueue(AsyncDiskFileChunk& chunk, AsyncBuffer*& outBuffer, char value) {
    outBuffer = chunk.get_free_buffer();
    ASSERT_NE(outBuffer, nullptr);
    const std::string payload(outBuffer->capacity(), value);
    size_t copied = 0;
    ASSERT_TRUE(outBuffer->add(payload.data(), payload.size(), copied));
    ASSERT_EQ(copied, payload.size());
    chunk.buffers_queued_.emplace_back(outBuffer, chunk.file_, chunk.file_position_);
    chunk.file_position_ += static_cast<int64_t>(payload.size());
  }

  // Put `tail` in the buffer being filled, as write() would, without polling or submitting.
  static void bufferTail(AsyncDiskFileChunk& chunk, const std::string& tail) {
    chunk.current_buffer_ = chunk.get_free_buffer();
    ASSERT_NE(chunk.current_buffer_, nullptr);
    size_t copied = 0;
    ASSERT_TRUE(chunk.current_buffer_->add(tail.data(), tail.size(), copied));
    ASSERT_EQ(copied, tail.size());
  }

  static void expectContent(AsyncDiskFileChunk& chunk, const std::string& expected) {
    ASSERT_EQ(chunk.seek(0, SEEK_SET), SUCCESS);
    std::string actual(expected.size(), '\0');
    size_t read = 0;
    ASSERT_EQ(chunk.read(actual.data(), actual.size(), read), SUCCESS);
    EXPECT_EQ(actual, expected);
  }
};

} // namespace vrs

namespace {

using vrs::AsyncDiskFileChunkTestPeer;

using vrs::test::limitFileSize;
using vrs::test::removeFileSizeLimit;

// Data that doesn't repeat, so a misplaced block can't go unnoticed.
std::string makeData(size_t size) {
  std::string data(size, '\0');
  uint32_t state = 1;
  for (char& c : data) {
    state = state * 1103515245 + 12345;
    c = static_cast<char>(state >> 16);
  }
  return data;
}

class AsyncDiskFileWithChunkAccess : public vrs::AsyncDiskFile {
 public:
  vrs::AsyncDiskFileChunk& getChunk(size_t index) {
    return *chunks_[index];
  }
};

TEST(AsyncDiskFileSubmissionTest, SwitchingBackToAChunkReturnsItsDeferredWriteError) {
  const std::string path = vrs::os::getTempFolder() + "deferred_rewind_error.vrs";
  const std::string data(4096, 'A');
  AsyncDiskFileWithChunkAccess file;
  ASSERT_EQ(file.create(path, {{"buffer_size", "4096"}, {"buffer_count", "4"}}), vrs::SUCCESS);
  ASSERT_EQ(file.write(data.data(), data.size()), vrs::SUCCESS);
  ASSERT_EQ(file.addChunk(), vrs::SUCCESS);
  ASSERT_EQ(file.write(data.data(), data.size()), vrs::SUCCESS);
  ASSERT_EQ(file.setPos(0), vrs::SUCCESS); // reopens the first chunk for writing
  ASSERT_EQ(file.setPos(static_cast<int64_t>(data.size())), vrs::SUCCESS);
  AsyncDiskFileChunkTestPeer::recordAsyncError(file.getChunk(0), EIO);
  EXPECT_EQ(file.setPos(0), EIO);
  EXPECT_EQ(file.close(), EIO);
  vrs::test::deleteChunkedFile(path);
}

TEST(AsyncDiskFileSubmissionTest, ClosingAnEvictedChunkReturnsItsDeferredWriteError) {
  const std::string path = vrs::os::getTempFolder() + "deferred_eviction_error.vrs";
  const std::string data(4096, 'A');
  AsyncDiskFileWithChunkAccess file;
  ASSERT_EQ(file.create(path, {{"buffer_size", "4096"}, {"buffer_count", "4"}}), vrs::SUCCESS);
  ASSERT_EQ(file.write(data.data(), data.size()), vrs::SUCCESS);
  for (int chunk = 1; chunk < 4; ++chunk) {
    ASSERT_EQ(file.addChunk(), vrs::SUCCESS);
    ASSERT_EQ(file.write(data.data(), data.size()), vrs::SUCCESS);
  }
  // Writing closes each chunk as the next one is added. Reopen the first two...
  const auto chunkOffset = [&data](int chunk) { return static_cast<int64_t>(chunk * data.size()); };
  ASSERT_EQ(file.setPos(chunkOffset(0)), vrs::SUCCESS);
  ASSERT_EQ(file.setPos(chunkOffset(1)), vrs::SUCCESS);
  // ... so reopening a third one evicts the current chunk, the second.
  AsyncDiskFileChunkTestPeer::recordAsyncError(file.getChunk(1), EIO);
  EXPECT_EQ(file.setPos(chunkOffset(2)), EIO);
  EXPECT_EQ(file.close(), vrs::SUCCESS);
  vrs::test::deleteChunkedFile(path);
}

TEST(AsyncDiskFileSubmissionTest, ReadingPastAChunkBoundaryReturnsItsDeferredWriteError) {
  const std::string path = vrs::os::getTempFolder() + "deferred_read_error.vrs";
  const std::string data(4096, 'A');
  AsyncDiskFileWithChunkAccess file;
  ASSERT_EQ(file.create(path, {{"buffer_size", "4096"}, {"buffer_count", "4"}}), vrs::SUCCESS);
  ASSERT_EQ(file.write(data.data(), data.size()), vrs::SUCCESS);
  ASSERT_EQ(file.addChunk(), vrs::SUCCESS);
  ASSERT_EQ(file.write(data.data(), data.size()), vrs::SUCCESS);
  ASSERT_EQ(file.setPos(0), vrs::SUCCESS);
  std::string actual(data.size(), '\0');
  ASSERT_EQ(file.read(actual.data(), actual.size()), vrs::SUCCESS); // to the end of chunk 0
  AsyncDiskFileChunkTestPeer::recordAsyncError(file.getChunk(0), EIO);
  char c = 0;
  EXPECT_EQ(file.read(c), EIO);
  EXPECT_EQ(file.close(), EIO);
  vrs::test::deleteChunkedFile(path);
}

TEST(AsyncDiskFileSubmissionTest, OverwritingPastAChunkBoundaryReturnsItsDeferredWriteError) {
  const std::string path = vrs::os::getTempFolder() + "deferred_overwrite_error.vrs";
  const std::string data(4096, 'A');
  AsyncDiskFileWithChunkAccess file;
  ASSERT_EQ(file.create(path, {{"buffer_size", "4096"}, {"buffer_count", "4"}}), vrs::SUCCESS);
  ASSERT_EQ(file.write(data.data(), data.size()), vrs::SUCCESS);
  ASSERT_EQ(file.addChunk(), vrs::SUCCESS);
  ASSERT_EQ(file.write(data.data(), data.size()), vrs::SUCCESS);
  ASSERT_EQ(file.setPos(0), vrs::SUCCESS);
  ASSERT_EQ(file.overwrite(data.data(), data.size()), vrs::SUCCESS); // to the end of chunk 0
  AsyncDiskFileChunkTestPeer::recordAsyncError(file.getChunk(0), EIO);
  EXPECT_EQ(file.overwrite("B", 1), EIO);
  EXPECT_EQ(file.close(), EIO);
  vrs::test::deleteChunkedFile(path);
}

TEST(AsyncDiskFileSubmissionTest, FailedReopenIsReportedByAddChunkAndClose) {
  const std::string path = vrs::os::getTempFolder() + "failed_reopen.vrs";
  const std::string moved = path + "_moved";
  const std::string data(4096 + 17, 'A');
  AsyncDiskFileWithChunkAccess file;
  ASSERT_EQ(file.create(path, {{"buffer_size", "4096"}, {"buffer_count", "4"}}), vrs::SUCCESS);
  ASSERT_EQ(file.write(data.data(), data.size()), vrs::SUCCESS);
  if (!AsyncDiskFileChunkTestPeer::usesDirectIo(file.getChunk(0))) {
    EXPECT_EQ(file.close(), vrs::SUCCESS);
    vrs::test::deleteChunkedFile(path);
    GTEST_SKIP() << "writing the tail reopens the file only when it is open for direct I/O";
  }
  ASSERT_EQ(::rename(path.c_str(), moved.c_str()), 0); // the tail can't be written
  EXPECT_EQ(file.addChunk(), ENOENT);
  EXPECT_EQ(file.close(), ENOENT);
  EXPECT_EQ(vrs::os::remove(moved), 0);
}

class AsyncDiskFileChunkWriteTest : public testing::Test {
 protected:
  void SetUp() override {
    path_ = vrs::os::getTempFolder() +
        testing::UnitTest::GetInstance()->current_test_info()->name() + ".vrs";
    ASSERT_EQ(chunk_.create(path_, {{"buffer_size", "4096"}, {"buffer_count", "4"}}), vrs::SUCCESS);
  }

  void TearDown() override {
    EXPECT_EQ(chunk_.close(), vrs::SUCCESS);
    EXPECT_EQ(vrs::os::remove(path_), 0);
  }

  // TSAN builds force the synchronous engine, which writes everything right away.
  bool writesAreBuffered() const {
    return AsyncDiskFileChunkTestPeer::buffersWrites(chunk_);
  }

  std::string path_;
  vrs::AsyncDiskFileChunk chunk_;
};

using AsyncDiskFileChunkWriteDeathTest = AsyncDiskFileChunkWriteTest;

TEST_F(AsyncDiskFileChunkWriteTest, RejectedSubmissionIsWrittenSynchronously) {
  AsyncDiskFileChunkTestPeer::verifyRejectedSubmissionIsWrittenSynchronously(chunk_);
}

TEST_F(AsyncDiskFileChunkWriteTest, RejectedAndAcceptedSubmissionsAreAllWritten) {
  if (!writesAreBuffered()) {
    GTEST_SKIP() << "needs an engine that buffers writes";
  }
  AsyncDiskFileChunkTestPeer::verifyMixedSubmissionsAreAllWritten(chunk_);
}

TEST_F(AsyncDiskFileChunkWriteTest, FailedAsyncWriteErrorPersistsAndTailIsStillWritten) {
  if (!writesAreBuffered()) {
    GTEST_SKIP() << "needs an engine that buffers writes";
  }
  AsyncDiskFileChunkTestPeer::verifyFailedAsyncWriteErrorPersistsUntilClose(chunk_, path_);
}

// Death tests run in a child process, so the file size limits they set don't leak into other tests.
TEST_F(AsyncDiskFileChunkWriteDeathTest, ShortTailWriteKeepsTheRestAndThePosition) {
  if (!writesAreBuffered()) {
    GTEST_SKIP() << "needs an engine that buffers writes";
  }
  ASSERT_EXIT(
      {
        const std::string tail(17, 0x43);
        size_t written = 0;
        ASSERT_EQ(chunk_.write(tail.data(), tail.size(), written), vrs::SUCCESS);
        limitFileSize(5);
        EXPECT_EQ(chunk_.flush(), vrs::DISKFILE_PARTIAL_WRITE_ERROR);
        int64_t position = 0;
        EXPECT_EQ(chunk_.tell(position), vrs::SUCCESS);
        EXPECT_EQ(position, static_cast<int64_t>(tail.size()));
        removeFileSizeLimit();
        const std::string more = makeData(5000);
        EXPECT_EQ(chunk_.write(more.data(), more.size(), written), vrs::SUCCESS);
        // The bytes kept from the short write are written out, so buffers start aligned again.
        EXPECT_EQ(AsyncDiskFileChunkTestPeer::bufferStart(chunk_) % 4096, 0);
        EXPECT_EQ(chunk_.flush(), vrs::SUCCESS);
        EXPECT_EQ(chunk_.tell(position), vrs::SUCCESS);
        const std::string expected = tail + more;
        EXPECT_EQ(position, static_cast<int64_t>(expected.size()));
        EXPECT_EQ(chunk_.close(), vrs::SUCCESS);
        EXPECT_EQ(AsyncDiskFileChunkTestPeer::readFile(path_), expected);
        _exit(testing::Test::HasFailure() ? 1 : 0);
      },
      testing::ExitedWithCode(0),
      "");
}

TEST_F(AsyncDiskFileChunkWriteDeathTest, CloseReportsATailItCannotWrite) {
  if (!writesAreBuffered()) {
    GTEST_SKIP() << "needs an engine that buffers writes";
  }
  ASSERT_EXIT(
      {
        alarm(5);
        const std::string tail(17, 0x43);
        size_t written = 0;
        ASSERT_EQ(chunk_.write(tail.data(), tail.size(), written), vrs::SUCCESS);
        limitFileSize(5);
        EXPECT_EQ(chunk_.close(), vrs::DISKFILE_PARTIAL_WRITE_ERROR);
        EXPECT_FALSE(chunk_.isOpened());
        removeFileSizeLimit();
        EXPECT_EQ(AsyncDiskFileChunkTestPeer::readFile(path_), tail.substr(0, 5));
        _exit(testing::Test::HasFailure() ? 1 : 0);
      },
      testing::ExitedWithCode(0),
      "");
}

TEST_F(AsyncDiskFileChunkWriteDeathTest, ShortPsyncWriteKeepsTheRestAndThePosition) {
  if (!writesAreBuffered()) {
    GTEST_SKIP() << "needs an engine that buffers writes";
  }
  ASSERT_EXIT(
      {
        vrs::AsyncDiskFileChunk chunk;
        const std::string path = path_ + "_psync";
        ASSERT_EQ(
            chunk.create(
                path, {{"ioengine", "psync"}, {"direct", "false"}, {"buffer_size", "4096"}}),
            vrs::SUCCESS);
        const std::string data = makeData(2 * 4096);
        limitFileSize(4096 + 100);
        size_t written = 0;
        EXPECT_EQ(
            chunk.write(data.data(), data.size(), written), vrs::DISKFILE_PARTIAL_WRITE_ERROR);
        int64_t position = 0;
        EXPECT_EQ(chunk.tell(position), vrs::SUCCESS);
        EXPECT_EQ(position, static_cast<int64_t>(written));
        removeFileSizeLimit();
        EXPECT_EQ(chunk.close(), vrs::SUCCESS);
        EXPECT_EQ(AsyncDiskFileChunkTestPeer::readFile(path), data.substr(0, written));
        EXPECT_EQ(vrs::os::remove(path), 0);
        _exit(testing::Test::HasFailure() ? 1 : 0);
      },
      testing::ExitedWithCode(0),
      "");
}

TEST_F(AsyncDiskFileChunkWriteDeathTest, FailedAsyncWriteIsReturnedByTheNextWrite) {
  if (!writesAreBuffered()) {
    GTEST_SKIP() << "needs an engine that buffers writes";
  }
  // A forked child can't use AIO once its parent has: glibc's AIO threads don't survive fork().
  GTEST_FLAG_SET(death_test_style, "threadsafe");
  ASSERT_EXIT(
      {
        vrs::AsyncDiskFileChunk chunk;
        const std::string path = path_ + "_one_buffer";
        ASSERT_EQ(
            chunk.create(path, {{"buffer_size", "4096"}, {"buffer_count", "1"}, {"iodepth", "1"}}),
            vrs::SUCCESS);
        const std::string data = makeData(2 * 4096);
        limitFileSize(4096); // the second buffer's write fails, after write() returned
        size_t written = 0;
        EXPECT_EQ(chunk.write(data.data(), data.size(), written), vrs::SUCCESS);
        EXPECT_EQ(chunk.write(data.data(), 17, written), EFBIG);
        EXPECT_EQ(written, 0u);
        removeFileSizeLimit();
        EXPECT_EQ(chunk.close(), EFBIG);
        EXPECT_EQ(vrs::os::remove(path), 0);
        _exit(testing::Test::HasFailure() ? 1 : 0);
      },
      testing::ExitedWithCode(0),
      "");
}

TEST_F(AsyncDiskFileChunkWriteDeathTest, ShortAsyncWriteIsAnError) {
  if (!writesAreBuffered()) {
    GTEST_SKIP() << "needs an engine that buffers writes";
  }
  // A forked child can't use AIO once its parent has: glibc's AIO threads don't survive fork().
  GTEST_FLAG_SET(death_test_style, "threadsafe");
  ASSERT_EXIT(
      {
        vrs::AsyncDiskFileChunk chunk;
        const std::string path = path_ + "_short";
        ASSERT_EQ(
            chunk.create(
                path, {{"buffer_size", "4096"}, {"buffer_count", "1"}, {"direct", "false"}}),
            vrs::SUCCESS);
        const std::string data = makeData(2 * 4096);
        limitFileSize(4096 + 100); // the second buffer's write comes up short
        size_t written = 0;
        EXPECT_EQ(chunk.write(data.data(), data.size(), written), vrs::SUCCESS);
        EXPECT_EQ(chunk.write(data.data(), 17, written), vrs::DISKFILE_PARTIAL_WRITE_ERROR);
        removeFileSizeLimit();
        EXPECT_EQ(chunk.close(), vrs::DISKFILE_PARTIAL_WRITE_ERROR);
        EXPECT_EQ(vrs::os::remove(path), 0);
        _exit(testing::Test::HasFailure() ? 1 : 0);
      },
      testing::ExitedWithCode(0),
      "");
}

TEST_F(AsyncDiskFileChunkWriteDeathTest, FailedSynchronousFallbackIsSticky) {
  ASSERT_EXIT(
      {
        AsyncDiskFileChunkTestPeer::verifyFailedSynchronousFallbackIsSticky(chunk_);
        _exit(testing::Test::HasFailure() ? 1 : 0);
      },
      testing::ExitedWithCode(0),
      "");
}

TEST_F(AsyncDiskFileChunkWriteTest, RetriedReopenAfterAFailedOneDoesNotTruncate) {
  const std::string data(4096, 'A');
  const std::string tail(17, 'B');
  size_t written = 0;
  ASSERT_EQ(chunk_.write(data.data(), data.size(), written), vrs::SUCCESS);
  ASSERT_EQ(chunk_.write(tail.data(), tail.size(), written), vrs::SUCCESS);
  if (!AsyncDiskFileChunkTestPeer::usesDirectIo(chunk_)) {
    GTEST_SKIP() << "writing the tail reopens the file only when it is open for direct I/O";
  }
  const std::string moved = path_ + "_moved";
  ASSERT_EQ(::rename(path_.c_str(), moved.c_str()), 0);
  EXPECT_EQ(chunk_.flush(), ENOENT);
  EXPECT_TRUE(chunk_.isOpened()); // still holding the tail
  ASSERT_EQ(::rename(moved.c_str(), path_.c_str()), 0);
  EXPECT_EQ(chunk_.flush(), vrs::SUCCESS);
  EXPECT_EQ(chunk_.close(), vrs::SUCCESS);
  EXPECT_EQ(AsyncDiskFileChunkTestPeer::readFile(path_), data + tail);
}

TEST_F(AsyncDiskFileChunkWriteTest, ReopenSameFileSwitchesToANewDescriptor) {
  const std::string path = path_ + "_fd";
  vrs::AsyncFileDescriptor file;
  ASSERT_EQ(file.open(path, "wb+", 0), vrs::SUCCESS);
  size_t written = 0;
  ASSERT_EQ(file.pwrite("abc", 3, 0, written), vrs::SUCCESS);
  const int first = file.fd_;
  int closeError = EBADF;
  ASSERT_EQ(file.reopenSameFile(path, "rb+", 0, closeError), vrs::SUCCESS);
  EXPECT_EQ(closeError, vrs::SUCCESS);
  // Opened while the old descriptor was still open, so it can't have the same number.
  EXPECT_NE(file.fd_, first);
  ASSERT_EQ(file.pwrite("def", 3, 3, written), vrs::SUCCESS);
  EXPECT_EQ(file.close(), vrs::SUCCESS);
  EXPECT_EQ(AsyncDiskFileChunkTestPeer::readFile(path), "abcdef");
  EXPECT_EQ(vrs::os::remove(path), 0);
}

TEST_F(AsyncDiskFileChunkWriteTest, ReopenSameFileKeepsTheDescriptorOfAReplacedFile) {
  const std::string path = path_ + "_fd";
  const std::string moved = path + "_moved";
  vrs::AsyncFileDescriptor file;
  ASSERT_EQ(file.open(path, "wb+", 0), vrs::SUCCESS);
  ASSERT_EQ(::rename(path.c_str(), moved.c_str()), 0);
  {
    std::ofstream(path) << "replacement";
  }
  const int original = file.fd_;
  int closeError = EBADF;
  EXPECT_EQ(file.reopenSameFile(path, "rb+", 0, closeError), ESTALE);
  EXPECT_EQ(closeError, vrs::SUCCESS);
  EXPECT_EQ(file.fd_, original);
  size_t written = 0;
  ASSERT_EQ(file.pwrite("abc", 3, 0, written), vrs::SUCCESS);
  EXPECT_EQ(file.close(), vrs::SUCCESS);
  EXPECT_EQ(AsyncDiskFileChunkTestPeer::readFile(moved), "abc");
  EXPECT_EQ(AsyncDiskFileChunkTestPeer::readFile(path), "replacement");
  EXPECT_EQ(vrs::os::remove(moved), 0);
  EXPECT_EQ(vrs::os::remove(path), 0);
}

TEST_F(AsyncDiskFileChunkWriteTest, SetDirectChangesTheOpenFile) {
  const std::string path = path_ + "_fd";
  vrs::AsyncFileDescriptor file;
  ASSERT_EQ(file.open(path, "wb+", 0), vrs::SUCCESS);
  const int error = file.setDirect(true);
  if (error == EINVAL) {
    EXPECT_EQ(file.close(), vrs::SUCCESS);
    EXPECT_EQ(vrs::os::remove(path), 0);
    GTEST_SKIP() << "the temporary folder's file system doesn't support O_DIRECT";
  }
  ASSERT_EQ(error, vrs::SUCCESS);
  EXPECT_NE(::fcntl(file.fd_, F_GETFL) & O_DIRECT, 0);
  ASSERT_EQ(file.setDirect(false), vrs::SUCCESS);
  EXPECT_EQ(::fcntl(file.fd_, F_GETFL) & O_DIRECT, 0);
  EXPECT_EQ(file.close(), vrs::SUCCESS);
  EXPECT_EQ(vrs::os::remove(path), 0);
}

TEST_F(AsyncDiskFileChunkWriteTest, SetDirectFailsWhereODirectIsNotSupported) {
  struct stat device{};
  if (::stat("/dev/null", &device) != 0 || !S_ISCHR(device.st_mode)) {
    GTEST_SKIP() << "needs /dev/null";
  }
  vrs::AsyncFileDescriptor file;
  ASSERT_EQ(file.open("/dev/null", "rb+", 0), vrs::SUCCESS);
  EXPECT_EQ(file.setDirect(true), EINVAL);
  EXPECT_EQ(::fcntl(file.fd_, F_GETFL) & O_DIRECT, 0);
  EXPECT_EQ(file.close(), vrs::SUCCESS);
}

TEST_F(AsyncDiskFileChunkWriteTest, SeekAndTruncateOfAClosedChunkFail) {
  ASSERT_EQ(chunk_.close(), vrs::SUCCESS);
  EXPECT_EQ(chunk_.seek(0, SEEK_SET), vrs::DISKFILE_NOT_OPEN);
  EXPECT_EQ(chunk_.truncate(0), vrs::DISKFILE_NOT_OPEN);
  EXPECT_FALSE(chunk_.isOpened());
}

TEST_F(AsyncDiskFileChunkWriteTest, CreateAfterAFailedCreateUsesTheNewOptions) {
  if (!writesAreBuffered()) {
    GTEST_SKIP() << "needs an engine that buffers writes";
  }
  vrs::AsyncDiskFileChunk chunk;
  const std::string missingFolder = path_ + "_missing/chunk.vrs";
  ASSERT_NE(
      chunk.create(missingFolder, {{"buffer_size", "4096"}, {"buffer_count", "2"}}), vrs::SUCCESS);
  const std::string path = path_ + "_recreated";
  ASSERT_EQ(
      chunk.create(path, {{"buffer_size", "8192"}, {"buffer_count", "1"}, {"direct", "false"}}),
      vrs::SUCCESS);
  const std::vector<size_t> expected = {8192};
  EXPECT_EQ(AsyncDiskFileChunkTestPeer::bufferCapacities(chunk), expected);
  EXPECT_FALSE(AsyncDiskFileChunkTestPeer::usesDirectIo(chunk));
  EXPECT_EQ(chunk.close(), vrs::SUCCESS);
  EXPECT_EQ(vrs::os::remove(path), 0);
}

TEST_F(AsyncDiskFileChunkWriteTest, MoreBuffersThanIodepthAreAllWritten) {
  vrs::AsyncDiskFileChunk chunk;
  const std::string path = path_ + "_iodepth";
  ASSERT_EQ(
      chunk.create(path, {{"buffer_size", "4096"}, {"buffer_count", "4"}, {"iodepth", "1"}}),
      vrs::SUCCESS);
  const std::string data = makeData(10 * 4096 + 333);
  for (size_t offset = 0; offset < data.size(); offset += 1000) {
    const size_t size = std::min<size_t>(1000, data.size() - offset);
    size_t written = 0;
    ASSERT_EQ(chunk.write(data.data() + offset, size, written), vrs::SUCCESS);
    ASSERT_EQ(written, size);
  }
  EXPECT_EQ(chunk.close(), vrs::SUCCESS);
  EXPECT_EQ(AsyncDiskFileChunkTestPeer::readFile(path), data);
  EXPECT_EQ(vrs::os::remove(path), 0);
}

TEST_F(AsyncDiskFileChunkWriteTest, NextWriteReturnsAFailureThatAlreadyHappened) {
  struct stat device{};
  if (!writesAreBuffered() || ::stat("/dev/full", &device) != 0 || !S_ISCHR(device.st_mode)) {
    GTEST_SKIP() << "needs an engine that buffers writes, and /dev/full";
  }
  vrs::AsyncDiskFileChunk chunk;
  ASSERT_EQ(
      chunk.create("/dev/full", {{"buffer_size", "4096"}, {"buffer_count", "4"}}), vrs::SUCCESS);
  const std::string data = makeData(4096);
  size_t written = 0;
  ASSERT_EQ(chunk.write(data.data(), data.size(), written), vrs::SUCCESS);
  AsyncDiskFileChunkTestPeer::waitForAWriteToFinish(chunk); // /dev/full fails it with ENOSPC
  EXPECT_EQ(chunk.write(data.data(), 10, written), ENOSPC); // a free buffer was available
  EXPECT_EQ(chunk.close(), ENOSPC);
}

TEST_F(AsyncDiskFileChunkWriteTest, ReadOnlyOpenHasNoWriteBuffers) {
  const std::string data(100, 'A');
  size_t written = 0;
  ASSERT_EQ(chunk_.write(data.data(), data.size(), written), vrs::SUCCESS);
  ASSERT_EQ(chunk_.close(), vrs::SUCCESS);
  vrs::AsyncDiskFileChunk reader(0, path_, 0, static_cast<int64_t>(data.size()));
  ASSERT_EQ(reader.open(true, {}), vrs::SUCCESS);
  EXPECT_FALSE(AsyncDiskFileChunkTestPeer::hasWriteBuffers(reader));
  EXPECT_EQ(reader.write(data.data(), data.size(), written), vrs::DISKFILE_READ_ONLY);
  std::string actual(data.size(), '\0');
  size_t read = 0;
  EXPECT_EQ(reader.read(actual.data(), actual.size(), read), vrs::SUCCESS);
  EXPECT_EQ(actual, data);
  EXPECT_EQ(reader.close(), vrs::SUCCESS);
}

TEST_F(AsyncDiskFileChunkWriteDeathTest, ShortSynchronousWriteCountsWhatWasWritten) {
  ASSERT_EXIT(
      {
        vrs::AsyncDiskFileChunk chunk;
        const std::string path = path_ + "_sync";
        ASSERT_EQ(chunk.create(path, {{"ioengine", "sync"}, {"direct", "false"}}), vrs::SUCCESS);
        const std::string data = makeData(100);
        limitFileSize(5);
        size_t written = 0;
        EXPECT_EQ(
            chunk.write(data.data(), data.size(), written), vrs::DISKFILE_PARTIAL_WRITE_ERROR);
        EXPECT_EQ(written, 5u);
        int64_t position = 0;
        EXPECT_EQ(chunk.tell(position), vrs::SUCCESS);
        EXPECT_EQ(position, 5);
        removeFileSizeLimit();
        EXPECT_EQ(chunk.close(), vrs::SUCCESS);
        EXPECT_EQ(vrs::os::remove(path), 0);
        _exit(testing::Test::HasFailure() ? 1 : 0);
      },
      testing::ExitedWithCode(0),
      "");
}

} // namespace

#endif // POSIX_AIO_SUPPORTED()
#endif // VRS_ASYNC_DISKFILE_SUPPORTED()
