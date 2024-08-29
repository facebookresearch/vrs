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

#include <cassert>
#include <cstdio>

#ifdef _WIN32
#include <windows.h>
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

#include <logging/Log.h>

#include <vrs/ErrorCode.h>
#include <vrs/helpers/EnumStringConverter.h>
#include <vrs/helpers/FileMacros.h>
#include <vrs/helpers/Strings.h>
#include <vrs/os/Platform.h>
#include <vrs/os/Utils.h>

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

  bool isOpened() const {
    return h_ != INVALID_HANDLE_VALUE;
  }

  int open(const std::string& path, const char* modes, int flags) {
    // O_DIRECT is roughly equivalent to (FILE_FLAG_NO_BUFFERING | FILE_FLAG_WRITE_THROUGH)
    // We always open with FILE_FLAG_OVERLAPPED

    DWORD dwDesiredAccess = 0;

    bool badmode = false;
    if (modes[0] != 0) {
      for (size_t i = 1; modes[i] != 0; ++i) {
        switch (modes[i]) {
          case 'b':
            // do nothing: binary mode is the only mode available
            break;
          case '+':
            dwDesiredAccess = GENERIC_READ | GENERIC_WRITE;
            break;

          default:
            badmode = true;
        }
      }
    }

    DWORD dwCreationDisposition = 0;
    DWORD dwShareMode = 0;
    int whence = SEEK_SET;
    switch (modes[0]) {
      case 'r':
        dwCreationDisposition = dwDesiredAccess == 0 ? OPEN_EXISTING : OPEN_ALWAYS;
        dwDesiredAccess |= GENERIC_READ;
        dwShareMode = FILE_SHARE_READ;
        break;
      case 'w':
        dwCreationDisposition = CREATE_ALWAYS;
        dwDesiredAccess |= GENERIC_WRITE;
        break;
      case 'a':
        dwCreationDisposition = OPEN_ALWAYS;
        dwDesiredAccess |= GENERIC_WRITE;
        dwShareMode = FILE_SHARE_READ;
        whence = SEEK_END;
        break;
      default:
        badmode = true;
    }

    if (badmode) {
      XR_LOGCE(VRS_DISKFILECHUNK, "Unsupported open mode: '%s'", modes);
      return INVALID_PARAMETER;
    }

    DWORD dwFlagsAndAttributes = FILE_FLAG_OVERLAPPED;
    if (flags & O_DIRECT) {
      dwFlagsAndAttributes |= FILE_FLAG_NO_BUFFERING | FILE_FLAG_WRITE_THROUGH;
    }

    h_ = CreateFileA(
        path.c_str(),
        dwDesiredAccess,
        dwShareMode,
        NULL,
        dwCreationDisposition,
        dwFlagsAndAttributes,
        NULL);

    if (h_ == INVALID_HANDLE_VALUE) {
      return GetLastError();
    }

    int64_t pos;
    return seek(0, whence, pos);
  }

  int close() {
    if (!isOpened()) {
      return SUCCESS;
    }
    HANDLE h = h_;
    h_ = INVALID_HANDLE_VALUE;
    int error = CloseHandle(h) ? SUCCESS : GetLastError();

    return error;
  }

  int pwrite(const void* buf, size_t count, int64_t offset, size_t& outWriteSize) {
    return _readwrite(false, (void*)buf, count, offset, outWriteSize);
  }
  int read(void* buf, size_t count, int64_t offset, size_t& outReadSize) {
    return _readwrite(true, buf, count, offset, outReadSize);
  }

  int _readwrite(bool readNotWrite, void* buf, size_t count, int64_t offset, size_t& outSize) {
    // This assumes that the file is opened with FILE_FLAG_OVERLAPPED

    outSize = 0;

    // TODO: do we ever need to support larger-than 4GB accesses?
    DWORD dwToXfer = count;
    if ((decltype(count))dwToXfer != count) {
      return readNotWrite ? DISKFILE_NOT_ENOUGH_DATA : DISKFILE_PARTIAL_WRITE_ERROR;
    }

    // N.B. this does not create an hEvent for the OVERLAPPED structure, instead using the file
    // handle. This is only a valid thing to do if there are NO other IO operations occuring during
    // this one. The calls to flushWriteBuffer() before calling this ensures this is the case.
    OVERLAPPED ov = {};
    ov.Offset = (DWORD)offset;
    ov.OffsetHigh = (DWORD)(offset >> 32);

    DWORD dwNumberOfBytesTransferred = 0;
    bool success = false;
    if (readNotWrite) {
      success = ReadFile(h_, buf, dwToXfer, &dwNumberOfBytesTransferred, &ov);
    } else {
      success = WriteFile(h_, buf, dwToXfer, &dwNumberOfBytesTransferred, &ov);
    }

    if (!success) {
      int error = GetLastError();
      if (error != ERROR_IO_PENDING) {
        return error;
      }

      if (!GetOverlappedResult(h_, &ov, &dwNumberOfBytesTransferred, TRUE)) {
        return GetLastError();
      }
    }

    outSize = dwNumberOfBytesTransferred;
    if (dwNumberOfBytesTransferred != count) {
      return readNotWrite ? DISKFILE_NOT_ENOUGH_DATA : DISKFILE_PARTIAL_WRITE_ERROR;
    }
    return SUCCESS;
  }

  int truncate(int64_t newSize) {
    LARGE_INTEGER distanceToMove, currentFilePointer;
    // Get current filepointer
    distanceToMove.QuadPart = 0;
    if (!SetFilePointerEx(h_, distanceToMove, &currentFilePointer, FILE_CURRENT)) {
      return GetLastError();
    }

    if (currentFilePointer.QuadPart > newSize) {
      return DISKFILE_INVALID_STATE;
    }

    distanceToMove.QuadPart = newSize;
    if (!SetFilePointerEx(h_, distanceToMove, nullptr, FILE_BEGIN)) {
      return GetLastError();
    }

    if (!SetEndOfFile(h_)) {
      return GetLastError();
    }

    if (!SetFilePointerEx(h_, currentFilePointer, nullptr, FILE_BEGIN)) {
      return GetLastError();
    }

    return SUCCESS;
  }

  int seek(int64_t pos, int origin, int64_t& outFilepos) {
    LARGE_INTEGER liPos, liNewPos;
    liPos.QuadPart = pos;
    outFilepos = 0;
    static_assert(SEEK_SET == FILE_BEGIN);
    static_assert(SEEK_END == FILE_END);
    static_assert(SEEK_CUR == FILE_CURRENT);
    if (!SetFilePointerEx(h_, liPos, &liNewPos, origin)) {
      return GetLastError();
    } else {
      outFilepos = liNewPos.QuadPart;
      return SUCCESS;
    }
  }

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

  int open(const std::string& path, const char* modes, int flags) {
    assert(!isOpened());

    int permissions = 0666;

    bool badmode = false;
    bool rdwr = false;
    if (modes[0] != 0) {
      for (size_t i = 1; modes[i] != 0; ++i) {
        switch (modes[i]) {
          case 'b':
            // Linux has no O_BINARY
            break;
          case '+':
            rdwr = true;
            break;

          default:
            badmode = true;
        }
      }
    }

    int whence = SEEK_SET;
    switch (modes[0]) {
      case 'r':
        flags |= rdwr ? O_RDWR : O_RDONLY;
        break;
      case 'w':
        flags |= O_CREAT | O_TRUNC;
        flags |= rdwr ? O_RDWR : O_WRONLY;
        break;
      case 'a':
        flags |= rdwr ? O_RDWR : O_WRONLY;
        flags |= O_CREAT | O_APPEND;
        whence = rdwr ? SEEK_END : SEEK_SET;
        break;
      default:
        badmode = true;
    }

    if (badmode) {
      XR_LOGCE(VRS_DISKFILECHUNK, "Unsupported open mode: '%s'", modes);
      return INVALID_PARAMETER;
    }
    int newFd = ::open(path.c_str(), flags, permissions);
    if (newFd < 0) {
      return errno;
    }
    if (::lseek64(newFd, 0, whence) < 0) {
      ::close(newFd);
      return errno;
    }
    fd_ = newFd;
    return SUCCESS;
  }

  [[nodiscard]] bool isOpened() const {
    return fd_ >= 0;
  }

  int read(void* ptr, size_t bufferSize, size_t offset, size_t& outReadSize) {
    ssize_t ret = ::pread(fd_, ptr, bufferSize, offset);
    if (ret < 0) {
      outReadSize = 0;
      return errno;
    }

    outReadSize = ret;
    if (outReadSize != bufferSize) {
      return DISKFILE_NOT_ENOUGH_DATA;
    }
    return SUCCESS;
  }

  int truncate(int64_t newSize) {
#if IS_WINDOWS_PLATFORM()
    return ::_chsize_s(fd_, newSize);
#elif (defined(__ANDROID_API__) && __ANDROID_API__ >= 21)
    return ::ftruncate64(fd_, static_cast<off64_t>(newSize));
#else
    return ::ftruncate(fd_, newSize);
#endif
  }

  int seek(int64_t pos, int origin, int64_t& outFilepos) {
    off64_t result = ::lseek64(fd_, pos, origin);
    if (result < 0) {
      outFilepos = 0;
      return errno;
    } else {
      outFilepos = result;
      return 0;
    }
  }

  int pwrite(const void* buf, size_t count, off_t offset, size_t& written) {
    ssize_t result = ::pwrite(fd_, buf, count, offset);
    written = result;
    if (result != count) {
      if (result < 0) {
        written = 0;
        return errno;
      }
      return DISKFILE_PARTIAL_WRITE_ERROR;
    }
    return SUCCESS;
  }

  int close() {
    if (fd_ < 0) {
      return SUCCESS;
    }
    int fd = fd_;
    fd_ = INVALID_FILE_DESCRIPTOR;
    return ::close(fd);
  }

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
  AlignedBuffer(size_t size, size_t memalign, size_t lenalign) : capacity_(size) {
    if (lenalign && 0 != (capacity_ % lenalign)) {
      throw std::runtime_error("Capacity is not a multiple of lenalign");
    }
#ifdef _WIN32
    aligned_buffer_ = _aligned_malloc(capacity_, memalign);
#else
    if (0 != posix_memalign(&aligned_buffer_, memalign, capacity_)) {
      aligned_buffer_ = nullptr;
    }
#endif
    if (aligned_buffer_ == nullptr) {
      throw std::runtime_error("Failed to allocate aligned buffer");
    }
  }

  virtual ~AlignedBuffer() {
    free();
  }
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

  void free() {
    if (aligned_buffer_ == nullptr) {
      return;
    }
#if defined(_WIN32)
    _aligned_free(aligned_buffer_);
#else
    ::free(aligned_buffer_);
#endif
    aligned_buffer_ = nullptr;
    capacity_ = 0;
    size_ = 0;
  }

  void clear() {
    size_ = 0;
  }

  [[nodiscard]] inline void* data() const {
    return aligned_buffer_;
  }
  [[nodiscard]] inline char* bdata() const {
    return reinterpret_cast<char*>(aligned_buffer_);
  }

  /// adds std::min(size, capacity()-size()) bytes from buffer to our buffer.
  /// returns <0 on error, otherwise returns the number of bytes added.
  /// may return zero if the buffer is full
  [[nodiscard]] ssize_t add(const void* buffer, size_t size) {
    assert(size);

    size_t capacity = this->capacity();
    if (capacity == 0) {
      return -1;
    }
    if (size_ >= capacity) {
      throw std::runtime_error("buffer is already at capacity");
    }
    size_t tocopy = std::min<size_t>(size, capacity - size_);
    memcpy(bdata() + size_, buffer, tocopy);
    size_ += tocopy;

    return tocopy;
  }
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

  void complete_write(ssize_t io_return, int io_errno) {
    on_complete_(io_return, io_errno);
  }

  [[nodiscard]] int
  start_write(const AsyncHandle& file, int64_t offset, complete_write_callback on_complete) {
    ssize_t io_return = 0;
    int io_errno = SUCCESS;

#ifdef _WIN32
    ov_.self = this;
    ov_.ov = {};
    ov_.ov.Offset = (DWORD)offset;
    ov_.ov.OffsetHigh = (DWORD)(offset >> 32);
    if (!WriteFileEx(file.h_, AlignedBuffer::data(), size(), &ov_.ov, CompletedWriteRoutine)) {
      io_return = -1;
      io_errno = GetLastError();
      if (io_errno == 0) {
        io_errno = ERROR_GEN_FAILURE;
      }
    }

#else
    aiocb_ = {};
    aiocb_.aio_fildes = file.fd_;
    aiocb_.aio_offset = offset;
    aiocb_.aio_buf = AlignedBuffer::data();
    aiocb_.aio_nbytes = size();
    aiocb_.aio_reqprio = 0;
    aiocb_.aio_sigevent.sigev_notify = SIGEV_THREAD;
    aiocb_.aio_sigevent.sigev_notify_function = SigEvNotifyFunction;
    aiocb_.aio_sigevent.sigev_value.sival_ptr = this;
    aiocb_.aio_sigevent.sigev_notify_attributes = nullptr;
    aiocb_.aio_lio_opcode = 0; // used for lio_lioio only, unused
    on_complete_ = std::move(on_complete);

    if (aio_write(&aiocb_) != 0) {
      io_return = -1;
      io_errno = errno;
      if (io_errno == 0) {
        XR_LOGCD(VRS_DISKFILECHUNK, "aio_write failed, errno is 0");
        io_errno = -1;
      }
    }
    // The submission failed, call the cleanup function.
    // Note that the return value of aio_write is a subset of the aio_return (which is what a normal
    // completion calls). `aio_write` will either return -1 and set perror (same as aio_return), or
    // will return 0
#endif

    if (io_return != 0) {
      // If aio_write failed call the completion callback immediately to free the buffer
      on_complete_(io_return, io_errno);
    }
    return io_return;
  }

 private:
#ifdef _WIN32
  AsyncOVERLAPPED ov_;

  static void CompletedWriteRoutine(DWORD dwErr, DWORD cbBytesWritten, LPOVERLAPPED lpOverlapped) {
    auto self = reinterpret_cast<AsyncOVERLAPPED*>(
                    reinterpret_cast<char*>(lpOverlapped) - offsetof(AsyncOVERLAPPED, ov))
                    ->self;
    ssize_t io_return;
    int io_errno;

    if (dwErr == 0) {
      io_errno = 0;
      io_return = cbBytesWritten;
    } else {
      io_return = -1;
      io_errno = 0;
    }

    self->complete_write(io_return, io_errno);
  }
#else
  struct aiocb aiocb_ {};

  static void SigEvNotifyFunction(union sigval val) {
    auto* self = reinterpret_cast<AsyncBuffer*>(val.sival_ptr);

    ssize_t io_return = 0;
    int io_errno = 0;

    io_errno = aio_error(&self->aiocb_);
    if (io_errno == 0) {
      io_return = aio_return(&self->aiocb_);
      if (io_return < 0) {
        throw std::runtime_error(
            "aio_return returned a negative number despire aio_error indicating success");
      }
    } else if (io_errno == EINPROGRESS) {
      throw std::runtime_error("aio_error()==EINPROGRESS on a completed aio_write");
    } else if (io_errno == ECANCELED) {
      // If canceled, aio_return will give -1
      io_return = aio_return(&self->aiocb_);
      if (io_return >= 0) {
        throw std::runtime_error(
            "aio_error() signaled cancellation, but aio_return indicated success");
      }
    } else if (io_errno > 0) {
      io_return = aio_return(&self->aiocb_);
      if (io_return >= 0) {
        throw std::runtime_error("aio_error() signaled an error, but aio_return indicated success");
      }
    } else {
      throw std::runtime_error("aio_error() returned an unexpected negative number");
    }

    self->complete_write(io_return, io_errno);
  }

#endif
  complete_write_callback on_complete_ = nullptr;
};

class AsyncDiskFileChunk {
 public:
  AsyncDiskFileChunk() = default;
  AsyncDiskFileChunk(std::string path, int64_t offset, int64_t size)
      : path_{std::move(path)}, offset_{offset}, size_{size} {}
  AsyncDiskFileChunk(AsyncDiskFileChunk&& other) noexcept {
    file_ = std::move(other.file_);
    path_ = std::move(other.path_);
    offset_ = other.offset_;
    size_ = other.size_;

    // Keeps track of the current read/write position in the file of the current buffer.
    file_position_ = other.file_position_;

    file_mode_ = other.file_mode_;
    current_flags_ = other.current_flags_;
    supported_flags_ = other.supported_flags_;

    // Note that these members are not movable
    // buffers_mutex_
    // buffer_freed_cv_

    buffers_free_ = std::move(other.buffers_free_);
    buffers_queued_ = std::move(other.buffers_queued_);
    buffers_writing_ = other.buffers_writing_;
    other.buffers_writing_ = 0;
    buffers_ = std::move(other.buffers_);
    current_buffer_ = other.current_buffer_;
    other.current_buffer_ = nullptr;
    async_error_ = other.async_error_.load();

    ioengine_ = other.ioengine_;
    use_directio_ = other.use_directio_;
    num_buffers_ = other.num_buffers_;
    buffer_size_ = other.buffer_size_;
    iodepth_ = other.iodepth_;
    offset_align_ = other.offset_align_;
    mem_align_ = other.mem_align_;
  }

  // Prevent copying
  AsyncDiskFileChunk(const AsyncDiskFileChunk& other) noexcept = delete;
  AsyncDiskFileChunk& operator=(const AsyncDiskFileChunk& other) noexcept = delete;
  AsyncDiskFileChunk& operator=(AsyncDiskFileChunk&& rhs) noexcept = delete;

  ~AsyncDiskFileChunk() {
    try {
      close();
    } catch (std::exception& e) {
      XR_LOGCE(VRS_DISKFILECHUNK, "Exception on close() during destruction: {}", e.what());
    }
  }

  int create(const std::string& newpath, const std::map<std::string, std::string>& options) {
    close();

    path_ = newpath;
    offset_ = 0;
    size_ = 0;

    file_position_ = 0;
    async_error_ = SUCCESS;
    file_mode_ = "wb+";

    IF_ERROR_RETURN(init_parameters(options));
    int error = ensureOpenDirect();
    if (error != 0 && 0 != (O_DIRECT & supported_flags_)) {
      error = ensureOpenNonDirect();
      if (error == 0) {
        XR_LOGCW(
            VRS_DISKFILECHUNK,
            "O_DIRECT appears not to be supported for {}, falling back to non-direct IO",
            newpath);
        supported_flags_ &= ~O_DIRECT;
      }
    }

    if (error == 0) {
#if IS_ANDROID_PLATFORM()
      const size_t kBufferingSize = 128 * 1024;
      error = setvbuf(file_.fd_, nullptr, _IOFBF, kBufferingSize);
#endif
    }

    return error;
  }

  int open(bool readOnly, const std::map<std::string, std::string>& options) {
    close();

    file_position_ = 0;
    async_error_ = SUCCESS;
    file_mode_ = readOnly ? "rb" : "rb+";

    IF_ERROR_RETURN(init_parameters(options));
    return ensureOpenNonDirect();
  }

  int close() {
    if (!isOpened()) {
      return SUCCESS;
    }

    int error = flushWriteBuffer();

    // Release the write buffers, if any. File chunking is a rare enough event that it's not worth
    // trying to move these to the next currentChunk.
    free_write_buffers();

    int error2 = file_.close();
    return error != 0 ? error : error2;
  }

  int rewind() {
    // Normally rewind can't return an error, but this may be the only spot we have to return a
    // deferred error If we can't return errors here, we'll need to remember that we must flush on
    // the next operation, whatever it is, and THEN reset the file_position_. Pain.
    //
    // TODO: DiskFile doesn't currently check the return value of this, why would it? In a world in
    // which writes may be failing asynchronously, what should DiskFile or RecordFileWriter do to
    // recover, if anything?
    IF_ERROR_RETURN(flushWriteBuffer());

    file_position_ = 0;
    async_error_ = SUCCESS;

    return SUCCESS;
  }

  [[nodiscard]] bool eof() const {
    int64_t pos = 0;
    if (tell(pos) != 0) {
      return false;
    }

    return pos == getSize();
  }

  bool isOpened() {
    return file_.isOpened();
  }

  int write(const void* buffer, size_t count, size_t& outWrittenSize) {
    if (count == 0) {
      return SUCCESS;
    } else if (!isOpened()) {
      XR_LOGCE(VRS_DISKFILECHUNK, "DiskFile not opened");
      return DISKFILE_NOT_OPEN;
    }
    const auto* bbuffer = static_cast<const char*>(buffer);
    outWrittenSize = 0;

    if (!isOpened()) {
      return DISKFILE_NOT_OPEN;
    }

    if (count == 0) {
      return SUCCESS;
    }

    // compute the number of bytes to write synchronously, if any.
    size_t towrite = 0;
    if (ioengine_ == IoEngine::Sync) {
      // Write the entire buffer synchronously
      towrite = count;
    } else if (
        use_directio_ && (current_buffer_ == nullptr || current_buffer_->empty()) &&
        (file_position_ % offset_align_) != 0) {
      // Write as much as we need to in order to hit offset_align_, then fill the buffers
      towrite = std::min<size_t>(count, offset_align_ - (file_position_ % offset_align_));
    } // Otherwise writes can be aligned to anything, write nothing synchronously here

    if (towrite != 0) {
      // Rather than read-modify-write lenalign chunks of the file, and deal with all of the
      // corner cases of "do we overlap the end of the file or not, previously written data or
      // not, etc", we just close/reopen the file here to do the handful of partial writes
      // required by the library.

      IF_ERROR_RETURN(flushWriteBuffer());

      IF_ERROR_RETURN(ensureOpenNonDirect());

      size_t thiswritten = 0;
      IF_ERROR_RETURN(file_.pwrite(bbuffer, towrite, file_position_, thiswritten));
      bbuffer += thiswritten;
      count -= thiswritten;
      outWrittenSize += thiswritten;
      file_position_ += thiswritten;
    }

    if (count != 0 && current_buffer_ == nullptr) {
      current_buffer_ = get_free_buffer();
      if (current_buffer_ == nullptr) {
        return ENOMEM;
      }
    }

    while (count != 0) {
      // This data is aligned to lenalign, so cache it in the current_buffer_
      ssize_t additionalBuffered = current_buffer_->add(bbuffer, count);
      if (additionalBuffered <= 0) {
        return DISKFILE_PARTIAL_WRITE_ERROR;
      }
      bbuffer += additionalBuffered;
      count -= additionalBuffered;
      outWrittenSize += additionalBuffered;

      if (current_buffer_->full()) {
        IF_ERROR_RETURN(ensureOpenDirect());

        towrite = current_buffer_->size();
        switch (ioengine_) {
          case IoEngine::AIO: {
            // Other async IO engines like uring or libaio would go here in the fugure, and the
            // `start_write` call would dispatch it
            std::unique_lock lock{buffers_mutex_};
            buffers_queued_.emplace_back(
                current_buffer_,
                file_,
                file_position_,
                [this, buffer = current_buffer_](ssize_t io_return, int io_errno) {
                  this->complete_write(buffer, io_return, io_errno);
                });
            current_buffer_ = nullptr;
            file_position_ += towrite;
            pump_buffers_locked();

            if (count != 0) {
              current_buffer_ = get_free_buffer_locked(lock);
              if (!current_buffer_) {
                return ENOMEM;
              }
            }
            break;
          }
          case IoEngine::PSync: {
            size_t thiswritten = 0;
            int err = file_.pwrite(
                current_buffer_->data(), current_buffer_->size(), file_position_, thiswritten);
            // There's no need to release this buffer, as we've already written it. Save a fetch
            // later
            current_buffer_->clear();
            file_position_ += thiswritten;
            if (err) {
              return err;
            }
            break;
          }
          default:
            XR_LOGCE(VRS_DISKFILECHUNK, "Unhandled ioengine");
            return VRSERROR_INTERNAL_ERROR;
        }
      }
    }
    return SUCCESS;
  }

  void setSize(int64_t newSize) {
    size_ = newSize;
  }

  int flush() {
    return flushWriteBuffer();
  }

  int truncate(int64_t newSize) {
    IF_ERROR_RETURN(flushWriteBuffer());

    IF_ERROR_RETURN(file_.truncate(newSize));
    size_ = newSize;
    return SUCCESS;
  }

  int read(void* buffer, size_t count, size_t& outReadSize) {
    outReadSize = 0;
    if (!isOpened()) {
      return DISKFILE_NOT_OPEN;
    }

    // Finish writes in case we'll be reading data from pending writes
    IF_ERROR_RETURN(flushWriteBuffer());
    IF_ERROR_RETURN(ensureOpenNonDirect());

    int error = file_.read(buffer, count, file_position_, outReadSize);
    file_position_ += outReadSize;
    return error;
  }

  [[nodiscard]] int64_t getSize() const {
    return size_;
  }

  [[nodiscard]] bool contains(int64_t fileOffset) const {
    return fileOffset >= offset_ && fileOffset < offset_ + size_;
  }

  int tell(int64_t& outFilepos) const {
    outFilepos = file_position_ + (current_buffer_ ? current_buffer_->size() : 0);

    return SUCCESS;
  }

  int seek(int64_t pos, int origin) {
    // We don't know if we'll be reading or overwriting existing data, flush the buffers, and return
    // any errors that may surface from the completing operations
    IF_ERROR_RETURN(flushWriteBuffer());

    // we track the file offset ourselves, but let os::fileSeek do the actual work to compute the
    // final position, as our own `size_` member may not reflect the current size of the chunk.
    IF_ERROR_RETURN(file_.seek(file_position_, SEEK_SET, file_position_));
    IF_ERROR_RETURN(file_.seek(pos, origin, file_position_));

    return SUCCESS;
  }

  [[nodiscard]] const std::string& getPath() const {
    return path_;
  }

  void setOffset(int64_t newOffset) {
    offset_ = newOffset;
  }

  [[nodiscard]] int64_t getOffset() const {
    return offset_;
  }

 private:
  enum class IoEngine {
    Sync,
    AIO,
    PSync,
  };

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

  int flushWriteBuffer() {
    // Allow any other aio writes to complete. Note that `buffers_` may be empty because of
    // default construction and swapping.
    if (!buffers_.empty()) {
      std::unique_lock lock{buffers_mutex_};
      size_t expected_free = buffers_.size() - (current_buffer_ ? 1 : 0);
      while (buffers_free_.size() != expected_free) {
        // N.B. as buffers are freed they pump the queue to completion.
        buffer_freed_cv_.wait(
            lock, [this, expected_free] { return buffers_free_.size() == expected_free; });
      }

      int async_error = async_error_.exchange(0);
      if (async_error != 0) {
        XR_LOGCE(VRS_DISKFILECHUNK, "Returning async error on flush {}", async_error);
        return async_error;
      }
    }

    if (current_buffer_ && !current_buffer_->empty()) {
      IF_ERROR_RETURN(ensureOpenNonDirect());

      while (current_buffer_) {
        size_t towrite = current_buffer_->size();

        // if we've gotten here we're flushing, so just pwrite() the contents, don't bother being
        // fast about it
        size_t thiswritten = 0;
        int error = file_.pwrite(current_buffer_->data(), towrite, file_position_, thiswritten);
        free_buffer(current_buffer_);
        if (error != 0) {
          return error;
        }
        file_position_ += thiswritten;
      }
    }

    if (current_buffer_) {
      free_buffer(current_buffer_);
    }

    return SUCCESS;
  }

  int ensureOpenNonDirect() {
    return ensureOpen_(supported_flags_ & ~O_DIRECT);
  }

  int ensureOpenDirect() {
    return ensureOpen_(supported_flags_);
  }

  int ensureOpen_(int requested_flags) {
    bool no_truncate = false;
    if (file_.isOpened()) {
      if (requested_flags == current_flags_) {
        return SUCCESS;
      }
      no_truncate = true;
      file_.close();
    }

    const char* mode = file_mode_;
    if (mode == nullptr) {
      return DISKFILE_NOT_OPEN;
    }

    bool readOnly = (mode[0] == 'w') && (strchr(mode, '+') == nullptr);

    // When re-opening a file here, we must convert 'w' modes to 'r+' modes to ensure that we do not
    // truncate the file. This could fail if we don't have read permissions on the drive. If so,
    // we'd need to refactor so that we can provide the `O_TRUNC` or not flag to `open()`
    //
    // We assume that all VRS modes are binary here to avoid more string manipulation.
    if (mode[0] == 'w' && no_truncate) {
      mode = "rb+";
    }

    if (!readOnly) {
      IF_ERROR_RETURN(alloc_write_buffers());
    }

    int error = file_.open(path_, mode, requested_flags);
    if (error != 0) {
      close();
      return error;
    }
    current_flags_ = requested_flags;

#if IS_ANDROID_PLATFORM()
    const size_t kBufferingSize = 128 * 1024;
    IF_ERROR_LOG(setvbuf(newFd, nullptr, _IOFBF, kBufferingSize));
#endif

    return SUCCESS;
  }

  void complete_write(AsyncBuffer* buffer, ssize_t io_return, int io_errno) {
    // N.B. this is called asynchronously by the write completion "thread" from the kernel, it
    // must be thread-safe

    if (io_return == buffer->size()) {
      if (io_errno != SUCCESS) {
        XR_LOGCD(
            VRS_DISKFILECHUNK,
            "io_return was the size of the buffer, but io_errno is {}",
            io_errno);
        io_errno = SUCCESS;
      }
    } else {
      if (io_return < 0) {
        if (io_errno == SUCCESS) {
          XR_LOGCD(VRS_DISKFILECHUNK, "io_errno is 0 but io_return < 0");
          io_errno = DISKFILE_INVALID_STATE;
        }
      } else {
        // this was a partial write. Ignore io_errno, and signal it ourselves
        io_errno = DISKFILE_PARTIAL_WRITE_ERROR;
      }
    }

    if (io_errno != SUCCESS) {
      int current_error = async_error_.load();
      while (current_error == SUCCESS &&
             !async_error_.compare_exchange_weak(current_error, io_errno)) {
      }
    }

    {
      std::unique_lock lock{buffers_mutex_};
      free_buffer_locked(lock, buffer);
      buffers_writing_ -= 1;
      pump_buffers_locked();
    }
  }

  AsyncBuffer* get_free_buffer_locked(std::unique_lock<std::mutex>& lock) {
    if (buffers_free_.empty()) {
      buffer_freed_cv_.wait(lock, [this] { return !buffers_free_.empty(); });
    }
    assert(!buffers_free_.empty());
    auto* buffer = buffers_free_.back();
    buffers_free_.pop_back();
    assert(buffer);
    assert(buffer->empty());
    return buffer;
  }

  AsyncBuffer* get_free_buffer() {
    std::unique_lock lock{buffers_mutex_};
    return get_free_buffer_locked(lock);
  }

  void free_buffer(AsyncBuffer*& buffer) {
    std::unique_lock lock{buffers_mutex_};
    free_buffer_locked(lock, buffer);
  }

  void free_buffer_locked(std::unique_lock<std::mutex>& /* lock */, AsyncBuffer*& buffer) {
    buffer->clear();
    buffers_free_.push_back(buffer);
    buffer = nullptr;
    buffer_freed_cv_.notify_one();
  }

  void pump_buffers() {
    std::unique_lock lock{buffers_mutex_};
    pump_buffers_locked();
  }

  void pump_buffers_locked() {
    // You must own a lock on buffers_mutex_ to call this

    // Move as many queued buffers as we can to the writing state
    while (buffers_writing_ < iodepth_ && !buffers_queued_.empty()) {
      int result = SUCCESS;
      {
        // N.B. item's storage in in buffers_queued_, don't `pop_front()` until we're done using
        // it
        auto& item = buffers_queued_.front();
        result = item.buffer_->start_write(item.file_, item.offset_, std::move(item.callback_));
      }
      buffers_queued_.pop_front();
      // Note that `async_error_` is set by the completion routine on a start_write failure, no need
      // to modify it here
      if (result == SUCCESS) {
        buffers_writing_ += 1;
      }
    }
  }

  int alloc_write_buffers() {
    assert(buffers_writing_ == 0);
    buffers_free_.reserve(num_buffers_);
    buffers_.reserve(num_buffers_);
    while (buffers_.size() < num_buffers_) {
      auto buffer = std::make_unique<AsyncBuffer>(buffer_size_, mem_align_, offset_align_);
      if (!buffer) {
        return ENOMEM;
      }
      buffers_free_.push_back(buffer.get());
      buffers_.push_back(std::move(buffer));
    }
    return SUCCESS;
  }

  int free_write_buffers() {
    assert(buffers_free_.size() == buffers_.size());
    assert(buffers_writing_ == 0);
    assert(buffers_queued_.empty());
    current_buffer_ = nullptr;
    buffers_free_.clear();
    buffers_.clear();
    return SUCCESS;
  }

  int init_parameters(const std::map<std::string, std::string>& options) {
    static const char* sIoEngineTypes[] = {"sync", "aio", "psync"};
    struct IoEngineTypeConverter : public EnumStringConverter<
                                       IoEngine,
                                       sIoEngineTypes,
                                       COUNT_OF(sIoEngineTypes),
                                       IoEngine::AIO,
                                       IoEngine::AIO,
                                       true> {};

    // The VRS_DISKFILECHUNKASYNC_* options are primarily used for running the test suite with
    // different default IO configurations
    if (!helpers::getBool(options, "direct", use_directio_) &&
        !helpers::getBool(options, "directio", use_directio_)) {
      use_directio_ = true;
    }

#ifdef VRS_BUILDTYPE_TSAN
    // N.B. The aio_notify completions come in on a thread spawned from glibc that is not
    // tsan-instrumented. As a result, the `malloc()` call in the `aio_notify()` (which does go
    // through the tsan version) crashes when it tries to access the tsan thread state for tracking
    // the allocation. Force the use of the non-aio APIs in this case.
    ioengine_ = IoEngine::Sync;
#else
    {
      ioengine_ = IoEngine::AIO; // default, unless overridden
      auto it = options.find("ioengine");
      if (it != options.end()) {
        // ioengine names here have been chosen to correspond to the `fio` program's `ioengine` as
        // closely as possible, except `sync`, which acts like the basic DiskFileChunk.hpp, which
        // synchronously writes the buffer to disk write away, no buffering in this class.
        ioengine_ = IoEngineTypeConverter::toEnum(it->second);
      }
    }
#endif

    bool needBuffers = use_directio_ || (ioengine_ != IoEngine::Sync);
    if (!needBuffers) {
      supported_flags_ = 0;
      mem_align_ = 0;
      offset_align_ = 0;
      buffer_size_ = 0;
      num_buffers_ = 0;
      iodepth_ = 0;
      XR_LOGCI(
          VRS_DISKFILECHUNK,
          "asyncdiskfile configuration: IO Engine={} DirectIO={} (no internal buffers)",
          IoEngineTypeConverter::toString(ioengine_),
          use_directio_);
      return SUCCESS;
    }

    if (use_directio_) {
      supported_flags_ |= O_DIRECT;
    }

    mem_align_ = 4 * 1024;
    offset_align_ = 4 * 1024;

#ifdef STATX_DIOALIGN
    // Current kernel versions deployed around Meta don't have statx. Rely on the defaults/users
    // to set this up correctly for now.
    {
      struct statx statxbuf;
      int statErr = ::statx(AT_FDCWD, chunkFilePath.c_str(), 0, STATX_DIOALIGN, &statxbuf);
      if (statErr != 0) {
        XR_LOGCE(VRS_DISKFILECHUNK, "statx failed: %s", strerror(errno));
        return errno;
      } else {
        mem_align_ = statxbuf.stx_dio_mem_align;
        offset_align_ = statxbuf.stx_dio_mem_align;

        XR_LOGCD(
            VRS_DISKFILECHUNK,
            "statx reports blocksize:{:#x} mem_align:{:#x} offset_align:{:#x}",
            statxbuf.stx_blksize,
            mem_align_,
            offset_align_);
      }

      if (0 == mem_align_ || 0 == offset_align_) {
        XR_LOGCE(VRS_DISKFILECHUNK, "failed to get alignment info");
        return DISKFILE_NOT_OPEN;
      }
#endif

      // Allow overrides, but don't bother checking that they are powers of two or anything, on
      // the assumption that the underlying write() calls will fail if they're bad values.

      uint64_t temp_u64 = 0;
      mem_align_ = helpers::getByteSize(options, "mem_align", temp_u64) ? temp_u64 : mem_align_;
      mem_align_ = std::clamp<size_t>(mem_align_, 1, 16 * 1024);
      offset_align_ =
          helpers::getByteSize(options, "offset_align", temp_u64) ? temp_u64 : offset_align_;
      offset_align_ = std::clamp<size_t>(offset_align_, 1, 16 * 1024);

      // The defaults below might not be optimal for your rig.
      // They can still be overwritten with the parameter names below from the input URI.
      // fio testing showed each worker using 32MB buffers for non-pre-allocated disk was pretty
      // good. Avoids using more than 128 outstanding IO requests at a time, beyond which IO calls
      // were blocking.

      buffer_size_ =
          helpers::getByteSize(options, "buffer_size", temp_u64) ? temp_u64 : 32 * 1024 * 1024;
      buffer_size_ = std::clamp<size_t>(buffer_size_, 512, 512 * 1024 * 1024);
      num_buffers_ = helpers::getUInt64(options, "buffer_count", temp_u64) ? temp_u64 : 4;
      num_buffers_ = std::clamp<size_t>(num_buffers_, 1, 512);

      if (ioengine_ == IoEngine::PSync && num_buffers_ > 1) {
        XR_LOGCW(
            VRS_DISKFILECHUNK,
            "The psync ioengine can only make use of a single buffer, not {}.",
            num_buffers_);
        num_buffers_ = 1;
      }

      // fio testing showed that we really only need to keep a couple of these at a time
      iodepth_ = helpers::getUInt64(options, "iodepth", temp_u64) ? temp_u64 : num_buffers_;
      iodepth_ = std::clamp<size_t>(iodepth_, 1, 512);

      if ((buffer_size_ % offset_align_ != 0) || (buffer_size_ % mem_align_ != 0)) {
        XR_LOGCE(
            VRS_DISKFILECHUNK,
            "buffer_size={} doesn't conform to offset_align={} or mem_align={}",
            helpers::humanReadableFileSize(buffer_size_),
            helpers::humanReadableFileSize(offset_align_),
            helpers::humanReadableFileSize(mem_align_));
        return DISKFILE_INVALID_STATE;
      }
      XR_LOGCI(
          VRS_DISKFILECHUNK,
          "asyncdiskfile configuration: IOEngine={} DirectIO={} iodepth={} buffer_count={} "
          "buffer_size={} offset_align={} mem_align={}",
          IoEngineTypeConverter::toString(ioengine_),
          use_directio_,
          iodepth_,
          num_buffers_,
          helpers::humanReadableFileSize(buffer_size_),
          helpers::humanReadableFileSize(offset_align_),
          helpers::humanReadableFileSize(mem_align_));
      return SUCCESS;
    }

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
