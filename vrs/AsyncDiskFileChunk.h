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

#include "DiskFile.h"

#if VRS_ASYNC_DISKFILE_SUPPORTED()

#include <cassert>
#include <cstdio>

#ifdef _WIN32
#define VC_EXTRALEAN
#include <Windows.h>
#else
#include <aio.h>
#include <fcntl.h>
#include <unistd.h>
#endif

#include <atomic>
#include <condition_variable>
#include <deque>
#include <functional>
#include <map>
#include <memory>
#include <string>

#include <vrs/ErrorCode.h>
#include <vrs/os/Platform.h>

#define VRS_DISKFILECHUNK "AsyncDiskFileChunk"

namespace vrs {

#ifdef _WIN32
// Windows doesn't normally define these.
using ssize_t = int64_t;
#define O_DIRECT 0x80000000U

struct AsyncWindowsHandle {
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
  std::mutex mtx_;
};
using AsyncHandle = AsyncWindowsHandle;
#else
struct AsyncFileDescriptor {
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
  int read(void* ptr, size_t bufferSize, size_t offset, size_t& outReadSize);
  int truncate(int64_t newSize);
  int seek(int64_t pos, int origin, int64_t& outFilepos);
  int pwrite(const void* buf, size_t count, off_t offset, size_t& written);
  int close();

  int fd_ = INVALID_FILE_DESCRIPTOR;
};
using AsyncHandle = AsyncFileDescriptor;
#endif

class AlignedBuffer {
 private:
  void* aligned_buffer_ = nullptr;
  size_t capacity_ = 0;
  size_t size_ = 0;

 public:
  AlignedBuffer(size_t size, size_t memalign, size_t lenalign);
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
  [[nodiscard]] inline void* data() const {
    return aligned_buffer_;
  }
  [[nodiscard]] inline char* bdata() const {
    return reinterpret_cast<char*>(aligned_buffer_);
  }
  [[nodiscard]] ssize_t add(const void* buffer, size_t size);
};

class AsyncBuffer;
#ifdef _WIN32
struct AsyncOVERLAPPED {
  OVERLAPPED ov;
  // Allows the completion routine to recover a pointer to the containing AsyncBuffer
  AsyncBuffer* self;
};
#endif

class AsyncBuffer : public AlignedBuffer {
 public:
  using complete_write_callback = std::function<void(ssize_t io_return, int io_errno)>;

  AsyncBuffer(size_t size, size_t memalign, size_t lenalign)
      : AlignedBuffer(size, memalign, lenalign) {}
  ~AsyncBuffer() override = default;

  void complete_write(ssize_t io_return, int io_errno);
  [[nodiscard]] int
  start_write(const AsyncHandle& file, int64_t offset, complete_write_callback on_complete);

 private:
#ifdef _WIN32
  AsyncOVERLAPPED ov_;
  static void CompletedWriteRoutine(DWORD dwErr, DWORD cbBytesWritten, LPOVERLAPPED lpOverlapped);
#else
  struct aiocb aiocb_ {};
  static void SigEvNotifyFunction(union sigval val);
#endif
  complete_write_callback on_complete_ = nullptr;
};

class AsyncDiskFileChunk {
 public:
  AsyncDiskFileChunk() = default;
  AsyncDiskFileChunk(std::string path, int64_t offset, int64_t size)
      : path_{std::move(path)}, offset_{offset}, size_{size} {}
  AsyncDiskFileChunk(AsyncDiskFileChunk&& other) noexcept;

  // Prevent copying
  AsyncDiskFileChunk(const AsyncDiskFileChunk& other) noexcept = delete;
  AsyncDiskFileChunk& operator=(const AsyncDiskFileChunk& other) noexcept = delete;
  AsyncDiskFileChunk& operator=(AsyncDiskFileChunk&& rhs) noexcept = delete;

  ~AsyncDiskFileChunk();

  int create(const std::string& newpath, const std::map<std::string, std::string>& options);
  int open(bool readOnly, const std::map<std::string, std::string>& options);
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

  enum class IoEngine {
    Sync,
    AIO,
    PSync,
  };

 private:
  struct QueuedWrite {
    AsyncBuffer* buffer_;
    // N.B. QueuedWrite's are guaranteed to be flushed before the associated file descriptor is
    // close, so storing this via reference is safe.
    const AsyncHandle& file_;
    off_t offset_;
    AsyncBuffer::complete_write_callback callback_;
    QueuedWrite(
        AsyncBuffer* buffer,
        AsyncHandle& file,
        off_t offset,
        AsyncBuffer::complete_write_callback callback)
        : buffer_(buffer), file_(file), offset_(offset), callback_(std::move(callback)) {}
  };

  int flushWriteBuffer();
  int ensureOpenNonDirect();
  int ensureOpenDirect();
  int ensureOpen_(int requested_flags);
  void complete_write(AsyncBuffer* buffer, ssize_t io_return, int io_errno);
  AsyncBuffer* get_free_buffer_locked(std::unique_lock<std::mutex>& lock);
  AsyncBuffer* get_free_buffer();
  void free_buffer(AsyncBuffer*& buffer);
  void free_buffer_locked(std::unique_lock<std::mutex>& lock, AsyncBuffer*& buffer);
  void pump_buffers();
  void pump_buffers_locked();
  int alloc_write_buffers();
  int free_write_buffers();
  int init_parameters(const std::map<std::string, std::string>& options);

  AsyncHandle file_{};
  std::string path_; // path of this chunk
  int64_t offset_{}; // offset of this chunk in the file
  int64_t size_{}; // size of the chunk

  // Keeps track of the current read/write position in the file of the current buffer.
  int64_t file_position_ = 0;

  const char* file_mode_ = nullptr;
  // Keeps track of the flags currently in force for the opened fd_. Typically a subset of the
  // supported_flags_
  int current_flags_ = 0;
  // The flags supported by the underlying path_ file
  int supported_flags_ = 0;

  // Protects the following members from the writing thread as well as the asyncio callback
  // thread(s). Note that this lock is not really required on Windows, as the callbacks are
  // delivered on the dispatching thread when it's in an alertable state.
  std::mutex buffers_mutex_;
  // Used to notify a waiting writing thread that a buffer was freed.
  std::condition_variable buffer_freed_cv_;
  // The list of free buffers
  std::vector<AsyncBuffer*> buffers_free_;
  // The list of buffers to be written. Drained by pump_buffers()
  std::deque<QueuedWrite> buffers_queued_;
  // A count of the number of buffers waiting on async completions
  //
  // This could be a std::atomic<size_t>, but the current implementation has to take the lock
  // anyway to manage the list of buffers_free_, so don't bother.
  size_t buffers_writing_ = 0;
  // A list of all the buffers to keep them alive when they are being written (no longer in any
  // other queue)
  std::vector<std::unique_ptr<AsyncBuffer>> buffers_;
  // The current buffer (if any) being filled by calls to `write()`. It will either be queued
  // for async write by `write()`, or written out by `flushWriteBuffer()`
  AsyncBuffer* current_buffer_ = nullptr;
  // If != SUCCESS, represents errors that were signaled by async writes completing. Typically
  // returned to the caller as the result of another, later operation (e.g. another write after
  // the failure, or a call to flushWriteBuffer(), etc)
  std::atomic<int> async_error_ = SUCCESS;

  // Operational parameters initialized from the FileSpec extra params/options at create/open
  // time. These can be tuned by the user via uri parameeters.
  IoEngine ioengine_ = IoEngine::AIO;
  bool use_directio_ = true;
  // How many asyncio buffers to allocate and fill
  size_t num_buffers_ = 0;
  // The size of each individual buffer
  size_t buffer_size_ = 0;
  // The maximum number of simultaneous async_write operations allowed
  size_t iodepth_ = 4;
  // The requested alignment of buffer lengths and file offsets
  size_t offset_align_ = 0;
  // The requested length of memory alignment
  size_t mem_align_ = 0;
};

} // namespace vrs

#endif
