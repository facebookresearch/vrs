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

#include <cerrno>
#include <cstdint>
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

  static void verifyRejectedSubmission(AsyncDiskFileChunk& chunk) {
    AsyncBuffer* buffer = nullptr;
    enqueue(chunk, buffer);
    {
      std::unique_lock lock{chunk.buffers_mutex_};
      chunk.pump_buffers_locked(lock, [](AsyncDiskFileChunk::QueuedWrite& item) {
        return item.buffer_->startWrite(
            item.file_, item.offset_, std::move(item.callback_), rejectAgain);
      });
      EXPECT_EQ(chunk.buffers_writing_, 0);
      EXPECT_TRUE(chunk.buffers_queued_.empty());
      EXPECT_EQ(chunk.buffers_free_.size(), chunk.buffers_.size());
      EXPECT_EQ(buffer->size(), 0);
      EXPECT_FALSE(buffer->on_complete_);
    }
    EXPECT_EQ(chunk.flush(), EAGAIN);
    EXPECT_EQ(chunk.flush(), SUCCESS);
    EXPECT_EQ(chunk.close(), SUCCESS);
  }

  static void verifyFailedFlushPreservesTailAndPosition(AsyncDiskFileChunk& chunk) {
    AsyncBuffer* rejected = nullptr;
    enqueue(chunk, rejected);
    const int64_t tailOffset = chunk.file_position_;
    {
      std::unique_lock lock{chunk.buffers_mutex_};
      chunk.pump_buffers_locked(lock, [](AsyncDiskFileChunk::QueuedWrite& item) {
        return item.buffer_->startWrite(
            item.file_, item.offset_, std::move(item.callback_), rejectAgain);
      });
    }
    chunk.ioengine_ = AsyncDiskFileChunk::IoEngine::PSync;
    const std::vector<uint8_t> tail(17, 0x43);
    size_t written = 0;
    ASSERT_EQ(chunk.write(tail.data(), tail.size(), written), SUCCESS);
    ASSERT_EQ(written, tail.size());
    int64_t position = 0;
    ASSERT_EQ(chunk.tell(position), SUCCESS);
    const int64_t bufferedEnd = tailOffset + static_cast<int64_t>(tail.size());
    ASSERT_EQ(position, bufferedEnd);
    ASSERT_EQ(chunk.flush(), EAGAIN);
    EXPECT_TRUE(chunk.isOpened());
    ASSERT_EQ(chunk.tell(position), SUCCESS);
    EXPECT_EQ(position, bufferedEnd);
    const std::vector<uint8_t> appended(11, 0x95);
    ASSERT_EQ(chunk.write(appended.data(), appended.size(), written), SUCCESS);
    ASSERT_EQ(written, appended.size());
    ASSERT_EQ(chunk.tell(position), SUCCESS);
    const int64_t finalEnd = bufferedEnd + static_cast<int64_t>(appended.size());
    EXPECT_EQ(position, finalEnd);
    ASSERT_EQ(chunk.flush(), SUCCESS);
    ASSERT_EQ(chunk.tell(position), SUCCESS);
    EXPECT_EQ(position, finalEnd);
    ASSERT_EQ(chunk.seek(tailOffset, SEEK_SET), SUCCESS);
    std::vector<uint8_t> expected = tail;
    expected.insert(expected.end(), appended.begin(), appended.end());
    std::vector<uint8_t> actual(expected.size());
    size_t read = 0;
    ASSERT_EQ(chunk.read(actual.data(), actual.size(), read), SUCCESS);
    ASSERT_EQ(read, actual.size());
    EXPECT_EQ(actual, expected);
    EXPECT_EQ(chunk.close(), SUCCESS);
  }

  static void verifyMixedSubmissionsAndTail(AsyncDiskFileChunk& chunk) {
    AsyncBuffer* first = nullptr;
    AsyncBuffer* accepted = nullptr;
    AsyncBuffer* last = nullptr;
    enqueue(chunk, first);
    enqueue(chunk, accepted);
    enqueue(chunk, last);
    chunk.current_buffer_ = chunk.get_free_buffer();
    const std::vector<uint8_t> tail(17, 0x43);
    size_t copied = 0;
    ASSERT_TRUE(chunk.current_buffer_->add(tail.data(), tail.size(), copied));
    ASSERT_EQ(copied, tail.size());
    size_t submissions = 0;
    {
      std::unique_lock lock{chunk.buffers_mutex_};
      chunk.pump_buffers_locked(lock, [&submissions](AsyncDiskFileChunk::QueuedWrite& item) {
        ++submissions;
        const AsyncBuffer::AioSubmitter submit = submissions == 2
            ? acceptWithoutCompletion
            : (submissions == 1 ? rejectAgain : rejectNoMemory);
        return item.buffer_->startWrite(
            item.file_, item.offset_, std::move(item.callback_), submit);
      });
      EXPECT_EQ(submissions, 3);
      EXPECT_TRUE(chunk.buffers_queued_.empty());
      EXPECT_EQ(chunk.buffers_writing_, 1);
      EXPECT_EQ(chunk.buffers_free_.size(), 2);
      EXPECT_EQ(chunk.async_error_.load(), EAGAIN);
    }
    accepted->complete_write(accepted->size(), SUCCESS);
    EXPECT_EQ(chunk.buffers_writing_, 0);
    EXPECT_EQ(chunk.close(), EAGAIN);
    EXPECT_FALSE(chunk.isOpened());
    EXPECT_EQ(chunk.current_buffer_, nullptr);
    EXPECT_TRUE(chunk.buffers_.empty());
    EXPECT_TRUE(chunk.buffers_free_.empty());
    EXPECT_EQ(chunk.close(), SUCCESS);
  }

 private:
  using ControlBlock = decltype(AsyncBuffer::aiocb_);

  static int rejectAgain(ControlBlock*) {
    errno = EAGAIN;
    return -1;
  }

  static int rejectNoMemory(ControlBlock*) {
    errno = ENOMEM;
    return -1;
  }

  static int acceptWithoutCompletion(ControlBlock*) {
    return SUCCESS;
  }

  static void enqueue(AsyncDiskFileChunk& chunk, AsyncBuffer*& outBuffer) {
    outBuffer = chunk.get_free_buffer();
    ASSERT_NE(outBuffer, nullptr);
    const std::vector<uint8_t> payload(outBuffer->capacity(), 0x72);
    size_t copied = 0;
    ASSERT_TRUE(outBuffer->add(payload.data(), payload.size(), copied));
    ASSERT_EQ(copied, payload.size());
    chunk.buffers_queued_.emplace_back(
        outBuffer,
        chunk.file_,
        chunk.file_position_,
        [&chunk, buffer = outBuffer](ssize_t written, int error) {
          chunk.complete_write(buffer, written, error);
        });
    chunk.file_position_ += static_cast<int64_t>(payload.size());
  }
};

} // namespace vrs

namespace {

using vrs::AsyncDiskFileChunkTestPeer;

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
  EXPECT_EQ(file.close(), vrs::SUCCESS);
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

class AsyncDiskFileSubmissionDeathTest : public testing::Test {
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

  std::string path_;
  vrs::AsyncDiskFileChunk chunk_;
};

// A synchronous callback would deadlock on the queue lock; bound that regression in a child.
TEST_F(AsyncDiskFileSubmissionDeathTest, RejectionReturnsErrorAndReclaimsBuffer) {
  ASSERT_EXIT(
      {
        alarm(5);
        vrs::AsyncDiskFileChunkTestPeer::verifyRejectedSubmission(chunk_);
        _exit(testing::Test::HasFailure() ? 1 : 0);
      },
      testing::ExitedWithCode(0),
      "");
}

TEST_F(AsyncDiskFileSubmissionDeathTest, FailedFlushPreservesTailAndPositionForContinuedWrites) {
  ASSERT_EXIT(
      {
        alarm(5);
        vrs::AsyncDiskFileChunkTestPeer::verifyFailedFlushPreservesTailAndPosition(chunk_);
        _exit(testing::Test::HasFailure() ? 1 : 0);
      },
      testing::ExitedWithCode(0),
      "");
}

TEST_F(
    AsyncDiskFileSubmissionDeathTest,
    RejectionPreservesFirstErrorAndDrainsAcceptedWritesAndTail) {
  ASSERT_EXIT(
      {
        alarm(5);
        vrs::AsyncDiskFileChunkTestPeer::verifyMixedSubmissionsAndTail(chunk_);
        _exit(testing::Test::HasFailure() ? 1 : 0);
      },
      testing::ExitedWithCode(0),
      "");
}

} // namespace

#endif // POSIX_AIO_SUPPORTED()
#endif // VRS_ASYNC_DISKFILE_SUPPORTED()
