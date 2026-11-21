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

#pragma once

#include <vrs/DiskFile.h>

#if VRS_ASYNC_DISKFILE_SUPPORTED()

#include <cassert>
#include <cstdio>

// POSIX AIO (aio_write/aio_error/aio_return) is used on Linux. Android (its NDK has no <aio.h>)
// and Windows default to PSync (pwrite, with O_DIRECT unless disabled) instead.
#define POSIX_AIO_SUPPORTED() (IS_MAC_PLATFORM() || IS_LINUX_PLATFORM())

#if IS_WINDOWS_PLATFORM()
#define VC_EXTRALEAN
#include <Windows.h>
#elif POSIX_AIO_SUPPORTED()
#include <aio.h>
#include <fcntl.h>
#include <unistd.h>
#else
#include <fcntl.h>
#include <unistd.h>
#endif

#include <deque>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include <vrs/ErrorCode.h>
#include <vrs/VrsExport.h>
#include <vrs/os/Platform.h>

#define VRS_DISKFILECHUNK "AsyncDiskFileChunk"

namespace vrs {

#if POSIX_AIO_SUPPORTED()
#if IS_LINUX_PLATFORM()
// On 32-bit Linux, struct aiocb holds its file offset as a 32-bit off_t unless the whole build
// sets _FILE_OFFSET_BITS=64. Use the explicit 64-bit control block, the same way
// AsyncFileDescriptor uses lseek64 and pread64, so the layout of AsyncBuffer does not depend on
// the feature macros of the code that includes this header.
using AioControlBlock = aiocb64;
#else
using AioControlBlock = aiocb;
#endif
#endif

#if IS_WINDOWS_PLATFORM()
// Windows doesn't normally define these.
using ssize_t = int64_t;
#define O_DIRECT 0x80000000U

struct VRS_API AsyncWindowsHandle {
  AsyncWindowsHandle() : h_(INVALID_HANDLE_VALUE) {}
  AsyncWindowsHandle(HANDLE h) : h_(h) {}
  AsyncWindowsHandle(AsyncWindowsHandle&& rhs) : h_(rhs.h_) {
    rhs.h_ = INVALID_HANDLE_VALUE;
  }
  AsyncWindowsHandle(AsyncWindowsHandle& rhs) : h_(rhs.h_) {}
  AsyncWindowsHandle& operator=(AsyncWindowsHandle&& rhs) {
    h_ = rhs.h_;
    rhs.h_ = INVALID_HANDLE_VALUE;
    return *this;
  }

  bool isOpened() const;
  int open(const std::string& path, const char* modes, int flags);
  int close();
  int pwrite(const void* buf, size_t count, int64_t offset, size_t& outWriteSize);
  int read(void* buf, size_t count, int64_t offset, size_t& outReadSize);
  int truncate(int64_t newSize);
  int seek(int64_t pos, int origin, int64_t& outFilepos);

 private:
  int _readwrite(bool readNotWrite, void* buf, size_t count, int64_t offset, size_t& outSize);

 public:
  HANDLE h_ = INVALID_HANDLE_VALUE;
};
using AsyncHandle = AsyncWindowsHandle;
#else
struct VRS_API AsyncFileDescriptor {
  static constexpr int INVALID_FILE_DESCRIPTOR = -1;

  AsyncFileDescriptor() = default;
  explicit AsyncFileDescriptor(int fd) : fd_(fd) {}
  AsyncFileDescriptor(AsyncFileDescriptor&& rhs) noexcept : fd_(rhs.fd_) {
    rhs.fd_ = INVALID_FILE_DESCRIPTOR;
  }
  AsyncFileDescriptor(const AsyncFileDescriptor& rhs) noexcept = delete;
  AsyncFileDescriptor& operator=(AsyncFileDescriptor&& rhs) noexcept {
    fd_ = rhs.fd_;
    rhs.fd_ = INVALID_FILE_DESCRIPTOR;
    return *this;
  }
  AsyncFileDescriptor& operator=(const AsyncFileDescriptor& rhs) = delete;

  bool operator==(int fd) const {
    return fd_ == fd;
  }

  int open(const std::string& path, const char* modes, int flags);
  [[nodiscard]] bool isOpened() const;
  int read(void* ptr, size_t bufferSize, int64_t offset, size_t& outReadSize);
  int truncate(int64_t newSize);
  int seek(int64_t pos, int origin, int64_t& outFilepos);
  int pwrite(const void* buf, size_t count, int64_t offset, size_t& written);
  int close();

  int fd_ = INVALID_FILE_DESCRIPTOR;
};
using AsyncHandle = AsyncFileDescriptor;
#endif

class VRS_API AlignedBuffer {
 private:
  void* aligned_buffer_ = nullptr;
  size_t capacity_ = 0;
  size_t size_ = 0;

 protected:
  /// `size` must be a multiple of `lenalign`, unless `lenalign` is 0.
  AlignedBuffer(size_t size, size_t memalign, size_t lenalign);

  [[nodiscard]] inline bool isValid() const {
    return aligned_buffer_ != nullptr;
  }

 public:
  /// @return An empty buffer of capacity `size`, aligned to `memalign`, or nullptr if it couldn't
  /// be allocated.
  static std::unique_ptr<AlignedBuffer> make(size_t size, size_t memalign, size_t lenalign);

  virtual ~AlignedBuffer();

  [[nodiscard]] inline size_t size() const {
    return size_;
  }
  [[nodiscard]] inline size_t capacity() const {
    return capacity_;
  }
  [[nodiscard]] inline bool empty() const {
    return !size();
  }
  [[nodiscard]] inline bool full() const {
    return size() == capacity();
  }

  void free();
  void clear();
  /// Remove the first `size` bytes, moving the bytes after them to the start of the buffer.
  /// @param size: Number of bytes to remove, at most size().
  void consume(size_t size);
  [[nodiscard]] inline void* data() const {
    return aligned_buffer_;
  }
  [[nodiscard]] inline char* bdata() const {
    return reinterpret_cast<char*>(aligned_buffer_);
  }
  /// Append up to `size` bytes, stopping at the buffer's capacity.
  /// @param outCopiedSize: Set to the number of bytes copied, which is 0 if the buffer is
  /// already full, or `size` was 0.
  /// @return False if the buffer has no storage.
  [[nodiscard]] bool add(const void* buffer, size_t size, size_t& outCopiedSize);
};

class VRS_API AsyncBuffer : public AlignedBuffer {
 public:
  /// @return An empty buffer of capacity `size`, aligned to `memalign`, or nullptr if it couldn't
  /// be allocated.
  static std::unique_ptr<AsyncBuffer> make(size_t size, size_t memalign, size_t lenalign);

  ~AsyncBuffer() override = default;

  /// Submit an asynchronous write of the buffer's content. Nothing signals its completion:
  /// poll it with poll_write().
  /// @param file File receiving the write.
  /// @param offset File offset of the write.
  /// @return SUCCESS if submitted, otherwise the submission error (ENOSYS without POSIX AIO).
  [[nodiscard]] int start_write(const AsyncHandle& file, int64_t offset);
#if POSIX_AIO_SUPPORTED()
  /// Check whether the write submitted by start_write() has finished.
  /// @param outWritten: Set to the number of bytes written, or -1, once finished.
  /// @param outError: Set to the write's errno value, once finished.
  /// @return False while the write is still in progress.
  [[nodiscard]] bool poll_write(ssize_t& outWritten, int& outError);
  [[nodiscard]] const AioControlBlock* control_block() const {
    return &aiocb_;
  }
#endif

 protected:
  AsyncBuffer(size_t size, size_t memalign, size_t lenalign)
      : AlignedBuffer(size, memalign, lenalign) {}

 private:
#if POSIX_AIO_SUPPORTED()
  AioControlBlock aiocb_{};
  using AioSubmitter = int (*)(decltype(aiocb_)*);
  int startWrite(const AsyncHandle& file, int64_t offset, AioSubmitter submit);
  friend class AsyncDiskFileChunkTestPeer;
#endif
};

/// A file chunk that writes through aligned buffers, submitting full ones as asynchronous writes.
/// Finished writes are only processed by later calls, on the caller's thread: nothing calls back
/// into this object, so it needs no locking, but calls must not be concurrent. With `iodepth`
/// below `buffer_count`, full buffers beyond `iodepth` stay in memory until the next write(),
/// flush() or close().
class VRS_API AsyncDiskFileChunk {
 public:
  AsyncDiskFileChunk() = default;
  explicit AsyncDiskFileChunk(size_t index) : index_{index} {}
  AsyncDiskFileChunk(size_t index, std::string path, int64_t offset, int64_t size)
      : index_{index}, path_{std::move(path)}, offset_{offset}, size_{size} {}
  // Queued writes refer to this object's file handle, so it can neither be copied nor moved.
  AsyncDiskFileChunk(AsyncDiskFileChunk&& other) = delete;
  AsyncDiskFileChunk(const AsyncDiskFileChunk& other) noexcept = delete;
  AsyncDiskFileChunk& operator=(const AsyncDiskFileChunk& other) noexcept = delete;
  AsyncDiskFileChunk& operator=(AsyncDiskFileChunk&& rhs) noexcept = delete;

  ~AsyncDiskFileChunk();

  int create(const std::string& newpath, const FileSpec::Extras& options);
  int open(bool readOnly, const FileSpec::Extras& options);
  int close();
  int rewind();
  [[nodiscard]] bool eof() const;
  bool isOpened();
  int write(const void* buffer, size_t count, size_t& outWrittenSize);
  void setSize(int64_t newSize);
  int flush();
  int truncate(int64_t newSize);
  int read(void* buffer, size_t count, size_t& outReadSize);
  [[nodiscard]] int64_t getSize() const;
  [[nodiscard]] bool contains(int64_t fileOffset) const;
  int tell(int64_t& outFilepos) const;
  int seek(int64_t pos, int origin);
  [[nodiscard]] const std::string& getPath() const;
  void setOffset(int64_t newOffset);
  [[nodiscard]] int64_t getOffset() const;
  [[nodiscard]] size_t getIndex() const {
    return index_;
  }

  enum class IoEngine {
    Sync,
    AIO,
    PSync,
  };

 private:
  struct QueuedWrite {
    AsyncBuffer* buffer_;
    // Queued writes are always drained before the file handle is closed or replaced, so they
    // can refer to it.
    const AsyncHandle& file_;
    int64_t offset_;
    QueuedWrite(AsyncBuffer* buffer, AsyncHandle& file, int64_t offset)
        : buffer_(buffer), file_(file), offset_(offset) {}
  };

  int flushWriteBuffer();
  int waitForAsyncWrites();
  int writeTail();
  void writeRejectedBuffer(AsyncBuffer& buffer, int64_t offset, int submitError);
  int ensureOpenNonDirect();
  int ensureOpenDirect();
  int ensureOpen_(int requested_flags);
  void complete_write(AsyncBuffer* buffer, ssize_t io_return, int io_errno);
  // Complete the writes that have finished and submit queued ones. With `waitForOne`, wait until
  // at least one buffer is freed, or nothing is in flight.
  void reapWrites(bool waitForOne);
  void waitForAWriteToFinish();
  // @return nullptr if no buffer can ever become free.
  AsyncBuffer* get_free_buffer();
  void free_buffer(AsyncBuffer*& buffer);
  using WriteSubmitter = std::function<int(QueuedWrite&)>;
  void pump_buffers(const WriteSubmitter& submit = nullptr);
  void recordAsyncError(int error);
  friend class AsyncDiskFileChunkTestPeer;
  int alloc_write_buffers();
  int free_write_buffers();
  int init_parameters(const FileSpec::Extras& options);

  const size_t index_{}; // position of this chunk in the file's chunk list
  AsyncHandle file_{};
  // Open from create() or open() to close(), even while `file_` is closed by a failed reopen, so
  // that close() still writes, or reports, what is buffered.
  bool opened_ = false;
  std::string path_; // path of this chunk
  int64_t offset_{}; // offset of this chunk in the file
  int64_t size_{}; // size of the chunk

  // File position of the start of the current buffer. tell() adds the bytes buffered.
  int64_t file_position_ = 0;

  const char* file_mode_ = nullptr;
  // The flags currently in force for the open `file_`. Typically a subset of supported_flags_.
  int current_flags_ = 0;
  // The flags to use for full buffers: O_DIRECT unless disabled, or rejected by the file system.
  int supported_flags_ = 0;

  // The list of free buffers
  std::vector<AsyncBuffer*> buffers_free_;
  // The list of buffers to be written. Drained by pump_buffers(), up to `iodepth_` writes in
  // flight. Queued buffers wait for the next call that writes or flushes.
  std::deque<QueuedWrite> buffers_queued_;
  // The buffers with a write in flight
  std::vector<AsyncBuffer*> buffers_writing_;
#if POSIX_AIO_SUPPORTED()
  // The control blocks of `buffers_writing_`, for aio_suspend
  std::vector<const AioControlBlock*> writes_to_wait_for_;
#endif
  // A list of all the buffers to keep them alive when they are being written (no longer in any
  // other queue)
  std::vector<std::unique_ptr<AsyncBuffer>> buffers_;
  // The current buffer (if any) being filled by calls to `write()`. Once full, `write()` queues
  // or writes it. Otherwise, `flushWriteBuffer()` writes it out.
  AsyncBuffer* current_buffer_ = nullptr;
  // The first error that lost data write() had already accepted. While the chunk is open, every
  // later write, close, and operation that flushes returns it, until create() or open().
  int async_error_ = SUCCESS;
  bool rejected_submission_logged_ = false;

  // Operational parameters initialized from the FileSpec extra params/options at create/open
  // time. These can be tuned by the user via uri parameters.
  IoEngine ioengine_ = IoEngine::AIO;
  bool use_directio_ = true;
  // How many asyncio buffers to allocate and fill
  size_t num_buffers_ = 0;
  // The size of each individual buffer
  size_t buffer_size_ = 0;
  // The maximum number of AIO writes in flight
  size_t iodepth_ = 4;
  // The requested alignment of buffer lengths and file offsets
  size_t offset_align_ = 0;
  // The requested length of memory alignment
  size_t mem_align_ = 0;
};

} // namespace vrs

#endif
