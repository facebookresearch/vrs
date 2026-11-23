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

// VRS_BUILDTYPE_TSAN is not set by every TSAN build, so also ask the compiler.
#if defined(VRS_BUILDTYPE_TSAN) || defined(__SANITIZE_THREAD__)
#define VRS_ASYNC_DISKFILE_TSAN
#elif defined(__has_feature)
#if __has_feature(thread_sanitizer)
#define VRS_ASYNC_DISKFILE_TSAN
#endif
#endif

#if !IS_WINDOWS_PLATFORM()
#include <sys/stat.h>
#endif

#include <logging/Checks.h>
#include <logging/Log.h>

#include <algorithm>
#include <iterator>
#include <utility>

#include <vrs/helpers/EnumStringConverter.h>
#include <vrs/helpers/FileMacros.h>
#include <vrs/helpers/Strings.h>
#include <vrs/os/Utils.h>

namespace {
std::string_view sIoEngineTypes[] = {"sync", "aio", "psync"};
struct IoEngineTypeConverter : public vrs::EnumStringConverter<
                                   vrs::AsyncDiskFileChunk::IoEngine,
                                   sIoEngineTypes,
                                   vrs::array_size(sIoEngineTypes),
                                   vrs::AsyncDiskFileChunk::IoEngine::AIO,
                                   vrs::AsyncDiskFileChunk::IoEngine::AIO,
                                   true> {};

bool isReadOnlyMode(const char* mode) {
  return mode != nullptr && mode[0] == 'r' && strchr(mode, '+') == nullptr;
}

#if POSIX_AIO_SUPPORTED()
#if IS_LINUX_PLATFORM()
// The 64-bit variants of the AIO calls, matching vrs::AioControlBlock.
int aioWrite(vrs::AioControlBlock* cb) {
  return ::aio_write64(cb);
}
int aioError(const vrs::AioControlBlock* cb) {
  return ::aio_error64(cb);
}
ssize_t aioReturn(vrs::AioControlBlock* cb) {
  return ::aio_return64(cb);
}
int aioSuspend(const vrs::AioControlBlock* const list[], int count) {
  return ::aio_suspend64(list, count, nullptr);
}
#else
int aioWrite(vrs::AioControlBlock* cb) {
  return ::aio_write(cb);
}
int aioError(const vrs::AioControlBlock* cb) {
  return ::aio_error(cb);
}
ssize_t aioReturn(vrs::AioControlBlock* cb) {
  return ::aio_return(cb);
}
int aioSuspend(const vrs::AioControlBlock* const list[], int count) {
  return ::aio_suspend(list, count, nullptr);
}
#endif
#endif
} // namespace

namespace vrs {

#if IS_WINDOWS_PLATFORM()

bool AsyncWindowsHandle::isOpened() const {
  return h_ != INVALID_HANDLE_VALUE;
}

int AsyncWindowsHandle::open(const string& path, const char* modes, int flags) {
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

int AsyncWindowsHandle::close() {
  if (!isOpened()) {
    return SUCCESS;
  }
  HANDLE h = h_;
  h_ = INVALID_HANDLE_VALUE;
  int error = CloseHandle(h) ? SUCCESS : GetLastError();

  return error;
}

int AsyncWindowsHandle::pwrite(
    const void* buf,
    size_t count,
    int64_t offset,
    size_t& outWriteSize) {
  return _readwrite(false, (void*)buf, count, offset, outWriteSize);
}

int AsyncWindowsHandle::read(void* buf, size_t count, int64_t offset, size_t& outReadSize) {
  return _readwrite(true, buf, count, offset, outReadSize);
}

int AsyncWindowsHandle::_readwrite(
    bool readNotWrite,
    void* buf,
    size_t count,
    int64_t offset,
    size_t& outSize) {
  // This assumes that the file is opened with FILE_FLAG_OVERLAPPED

  outSize = 0;

  // TODO: do we ever need to support larger-than 4GB accesses?
  DWORD dwToXfer = count;
  if ((decltype(count))dwToXfer != count) {
    return readNotWrite ? DISKFILE_NOT_ENOUGH_DATA : DISKFILE_PARTIAL_WRITE_ERROR;
  }

  // N.B. this does not create an hEvent for the OVERLAPPED structure, instead using the file
  // handle. This is only valid because there are NO other IO operations on this handle during
  // this one: Windows has no asynchronous writes, so every operation completes before the next.
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
    if (error == ERROR_IO_PENDING) {
      error = GetOverlappedResult(h_, &ov, &dwNumberOfBytesTransferred, TRUE) ? SUCCESS
                                                                              : GetLastError();
    }
    // A read starting at the end of the file reports ERROR_HANDLE_EOF: that's a short read.
    if (error == ERROR_HANDLE_EOF && readNotWrite) {
      error = SUCCESS;
    }
    if (error != SUCCESS) {
      return error;
    }
  }

  outSize = dwNumberOfBytesTransferred;
  if (dwNumberOfBytesTransferred != count) {
    return readNotWrite ? DISKFILE_NOT_ENOUGH_DATA : DISKFILE_PARTIAL_WRITE_ERROR;
  }
  return SUCCESS;
}

int AsyncWindowsHandle::truncate(int64_t newSize) {
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

int AsyncWindowsHandle::seek(int64_t pos, int origin, int64_t& outFilepos) {
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

#else

int AsyncFileDescriptor::open(const string& path, const char* modes, int flags) {
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
#if (defined(__ANDROID_API__) && __ANDROID_API__ < 21)
  int newFd = ::open(path.c_str(), flags, permissions);
#else
  // open64 sets O_LARGEFILE, without which a 32-bit process gets EFBIG past 2 GiB
  int newFd = ::open64(path.c_str(), flags, permissions);
#endif
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

bool AsyncFileDescriptor::isOpened() const {
  return fd_ >= 0;
}

int AsyncFileDescriptor::read(void* ptr, size_t bufferSize, int64_t offset, size_t& outReadSize) {
  ssize_t ret = ::pread64(fd_, ptr, bufferSize, static_cast<off64_t>(offset));
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

int AsyncFileDescriptor::setDirect(bool direct) {
  const int flags = ::fcntl(fd_, F_GETFL);
  if (flags < 0) {
    return errno;
  }
  const int newFlags = direct ? (flags | O_DIRECT) : (flags & ~O_DIRECT);
  return ::fcntl(fd_, F_SETFL, newFlags) == 0 ? SUCCESS : errno;
}

int AsyncFileDescriptor::reopenSameFile(
    const string& path,
    const char* modes,
    int flags,
    int& outCloseError) {
  outCloseError = SUCCESS;
  AsyncFileDescriptor reopened;
  IF_ERROR_RETURN(reopened.open(path, modes, flags));
  struct stat current{};
  struct stat other{};
  if (::fstat(fd_, &current) != 0 || ::fstat(reopened.fd_, &other) != 0) {
    const int error = errno;
    reopened.close();
    return error;
  }
  if (current.st_dev != other.st_dev || current.st_ino != other.st_ino) {
    reopened.close();
    return ESTALE;
  }
  outCloseError = close();
  std::swap(fd_, reopened.fd_);
  return SUCCESS;
}

int AsyncFileDescriptor::truncate(int64_t newSize) {
#if (defined(__ANDROID_API__) && __ANDROID_API__ < 21)
  return ::ftruncate(fd_, newSize) == 0 ? SUCCESS : errno;
#else
  return ::ftruncate64(fd_, static_cast<off64_t>(newSize)) == 0 ? SUCCESS : errno;
#endif
}

int AsyncFileDescriptor::seek(int64_t pos, int origin, int64_t& outFilepos) {
  off64_t result = ::lseek64(fd_, pos, origin);
  if (result < 0) {
    outFilepos = 0;
    return errno;
  } else {
    outFilepos = result;
    return 0;
  }
}

int AsyncFileDescriptor::pwrite(const void* buf, size_t count, int64_t offset, size_t& written) {
  ssize_t result = ::pwrite64(fd_, buf, count, static_cast<off64_t>(offset));
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

int AsyncFileDescriptor::close() {
  if (fd_ < 0) {
    return SUCCESS;
  }
  int fd = fd_;
  fd_ = INVALID_FILE_DESCRIPTOR;
  return ::close(fd) == 0 ? SUCCESS : errno;
}

#endif

AlignedBuffer::AlignedBuffer(size_t size, size_t memalign, size_t lenalign) : capacity_(size) {
  XR_CHECK(
      lenalign == 0 || 0 == (capacity_ % lenalign),
      "Capacity {} is not a multiple of lenalign {}",
      capacity_,
      lenalign);
#if IS_WINDOWS_PLATFORM()
  aligned_buffer_ = _aligned_malloc(capacity_, memalign);
#else
  if (0 != posix_memalign(&aligned_buffer_, memalign, capacity_)) {
    aligned_buffer_ = nullptr;
  }
#endif
  if (aligned_buffer_ == nullptr) {
    XR_LOGCE(VRS_DISKFILECHUNK, "Failed to allocate a {} byte aligned buffer", capacity_);
    capacity_ = 0;
  }
}

std::unique_ptr<AlignedBuffer> AlignedBuffer::make(size_t size, size_t memalign, size_t lenalign) {
  std::unique_ptr<AlignedBuffer> buffer(new AlignedBuffer(size, memalign, lenalign));
  return buffer->isValid() ? std::move(buffer) : nullptr;
}

AlignedBuffer::~AlignedBuffer() {
  free();
}

void AlignedBuffer::free() {
  if (aligned_buffer_ == nullptr) {
    return;
  }
#if IS_WINDOWS_PLATFORM()
  _aligned_free(aligned_buffer_);
#else
  ::free(aligned_buffer_);
#endif
  aligned_buffer_ = nullptr;
  capacity_ = 0;
  size_ = 0;
}

void AlignedBuffer::clear() {
  size_ = 0;
}

void AlignedBuffer::consume(size_t size) {
  XR_CHECK_LE(size, size_);
  memmove(aligned_buffer_, bdata() + size, size_ - size);
  size_ -= size;
}

bool AlignedBuffer::add(const void* buffer, size_t size, size_t& outCopiedSize) {
  outCopiedSize = 0;
  if (!isValid()) {
    return false;
  }
  size_t tocopy = std::min<size_t>(size, capacity_ - size_);
  if (tocopy != 0) {
    memcpy(bdata() + size_, buffer, tocopy);
    size_ += tocopy;
    outCopiedSize = tocopy;
  }
  return true;
}

std::unique_ptr<AsyncBuffer> AsyncBuffer::make(size_t size, size_t memalign, size_t lenalign) {
  std::unique_ptr<AsyncBuffer> buffer(new AsyncBuffer(size, memalign, lenalign));
  return buffer->isValid() ? std::move(buffer) : nullptr;
}

int AsyncBuffer::start_write(
    [[maybe_unused]] const AsyncHandle& file,
    [[maybe_unused]] int64_t offset) {
#if POSIX_AIO_SUPPORTED()
  return startWrite(file, offset, aioWrite);
#else
  return ENOSYS; // init_parameters() never selects the AIO engine without POSIX AIO
#endif
}

#if POSIX_AIO_SUPPORTED()
int AsyncBuffer::startWrite(const AsyncHandle& file, int64_t offset, AioSubmitter submit) {
  aiocb_ = {};
  aiocb_.aio_fildes = file.fd_;
  aiocb_.aio_offset = offset;
  aiocb_.aio_buf = AlignedBuffer::data();
  aiocb_.aio_nbytes = size();
  aiocb_.aio_reqprio = 0;
  // No notification: libc may fail to start a SIGEV_THREAD thread, and then never notifies.
  aiocb_.aio_sigevent.sigev_notify = SIGEV_NONE;
  aiocb_.aio_lio_opcode = 0;
  if (submit(&aiocb_) != SUCCESS) {
    const int error = errno;
    return error != SUCCESS ? error : DISKFILE_INVALID_STATE;
  }
  return SUCCESS;
}

bool AsyncBuffer::poll_write(ssize_t& outWritten, int& outError) {
  outError = aioError(&aiocb_);
  if (outError == EINPROGRESS) {
    return false;
  }
  if (outError < 0) {
    outError = errno;
  }
  outWritten = aioReturn(&aiocb_);
  return true;
}
#endif

AsyncDiskFileChunk::~AsyncDiskFileChunk() {
  int error = close();
  if (error != 0) {
    XR_LOGCE(VRS_DISKFILECHUNK, "close() failed during destruction: {}", errorCodeToMessage(error));
  }
}

int AsyncDiskFileChunk::create(const string& newpath, const FileSpec::Extras& options) {
  close();
  free_write_buffers();

  path_ = newpath;
  offset_ = 0;
  size_ = 0;

  file_position_ = 0;
  async_error_ = SUCCESS;
  rejected_submission_logged_ = false;
  file_mode_ = "wb+";

  IF_ERROR_RETURN(init_parameters(options));
  int error = ensureOpenDirect();
  if (error != 0 && 0 != (O_DIRECT & supported_flags_)) {
    error = ensureOpenNonDirect();
    if (error == 0) {
      disableDirectIo();
    }
  }
  opened_ = error == SUCCESS;
  return error;
}

int AsyncDiskFileChunk::open(bool readOnly, const FileSpec::Extras& options) {
  close();
  free_write_buffers();

  file_position_ = 0;
  async_error_ = SUCCESS;
  rejected_submission_logged_ = false;
  file_mode_ = readOnly ? "rb" : "rb+";

  IF_ERROR_RETURN(init_parameters(options));
  const int error = ensureOpenNonDirect();
  opened_ = error == SUCCESS;
  return error;
}

int AsyncDiskFileChunk::close() {
  if (!isOpened()) {
    return SUCCESS;
  }

  int error = waitForAsyncWrites();
  const int tailError = writeTail();
  if (error == SUCCESS) {
    error = tailError;
  }
  // Like fclose(), close even when the tail can't be written: the returned error reports it.
  if (current_buffer_) {
    free_buffer(current_buffer_);
  }

  // Release the write buffers, if any. File chunking is a rare enough event that it's not worth
  // trying to move these to the next currentChunk.
  free_write_buffers();

  int error2 = file_.close();
  opened_ = false;
  return error != 0 ? error : error2;
}

int AsyncDiskFileChunk::rewind() {
  IF_ERROR_RETURN(flushWriteBuffer());
  file_position_ = 0;
  return SUCCESS;
}

bool AsyncDiskFileChunk::eof() const {
  int64_t pos = 0;
  if (tell(pos) != 0) {
    return false;
  }

  return pos == getSize();
}

bool AsyncDiskFileChunk::isOpened() {
  return opened_;
}

int AsyncDiskFileChunk::write(const void* buffer, size_t count, size_t& outWrittenSize) {
  outWrittenSize = 0;
  if (!isOpened()) {
    XR_LOGCE(VRS_DISKFILECHUNK, "DiskFile not opened");
    return DISKFILE_NOT_OPEN;
  }
  if (isReadOnlyMode(file_mode_)) {
    return DISKFILE_READ_ONLY;
  }
  reapWrites(false);
  IF_ERROR_RETURN(async_error_);
  if (count == 0) {
    return SUCCESS;
  }
  const auto* bbuffer = static_cast<const char*>(buffer);

  // compute the number of bytes to write synchronously, if any.
  size_t towrite = 0;
  if (ioengine_ == IoEngine::Sync) {
    // Write the entire buffer synchronously
    towrite = count;
  } else if (use_directio_ && (file_position_ % offset_align_) != 0) {
    // A buffer only starts unaligned when it holds what a short write left over: write that out.
    if (current_buffer_ != nullptr && !current_buffer_->empty()) {
      IF_ERROR_RETURN(flushWriteBuffer());
    }
    // Write as much as we need to in order to hit offset_align_, then fill the buffers
    if ((file_position_ % offset_align_) != 0) {
      towrite = std::min<size_t>(count, offset_align_ - (file_position_ % offset_align_));
    }
  } // Otherwise writes can be aligned to anything, write nothing synchronously here

  if (towrite != 0) {
    // Rather than read-modify-write offset_align_ blocks of the file, and deal with all of the
    // corner cases of "do we overlap the end of the file or not, previously written data or
    // not, etc", we do the handful of partial writes required by the library without O_DIRECT.

    IF_ERROR_RETURN(flushWriteBuffer());

    IF_ERROR_RETURN(ensureOpenNonDirect());

    size_t thiswritten = 0;
    const int error = file_.pwrite(bbuffer, towrite, file_position_, thiswritten);
    bbuffer += thiswritten;
    count -= thiswritten;
    outWrittenSize += thiswritten;
    file_position_ += thiswritten;
    IF_ERROR_RETURN(error);
  }

  if (count != 0 && current_buffer_ == nullptr) {
    current_buffer_ = get_free_buffer();
    if (current_buffer_ == nullptr) {
      return ENOMEM;
    }
    IF_ERROR_RETURN(async_error_);
  }

  while (count != 0) {
    // The file position is aligned to offset_align_, so stage the data in the current_buffer_
    size_t additionalBuffered = 0;
    if (!current_buffer_->add(bbuffer, count, additionalBuffered)) {
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
          // Other async IO engines like uring or libaio would go here in the future, and the
          // `start_write` call would dispatch it
          buffers_queued_.emplace_back(current_buffer_, file_, file_position_);
          current_buffer_ = nullptr;
          file_position_ += towrite;
          reapWrites(false);
          IF_ERROR_RETURN(async_error_);
          if (count != 0) {
            current_buffer_ = get_free_buffer();
            if (!current_buffer_) {
              return ENOMEM;
            }
            IF_ERROR_RETURN(async_error_);
          }
          break;
        }
        case IoEngine::PSync: {
          size_t thiswritten = 0;
          int err = file_.pwrite(
              current_buffer_->data(), current_buffer_->size(), file_position_, thiswritten);
          file_position_ += thiswritten;
          if (err != SUCCESS) {
            current_buffer_->consume(thiswritten);
            return err;
          }
          // There's no need to release this buffer, as we've already written it. Save a fetch
          // later
          current_buffer_->clear();
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

void AsyncDiskFileChunk::setSize(int64_t newSize) {
  size_ = newSize;
}

int AsyncDiskFileChunk::flush() {
  return flushWriteBuffer();
}

int AsyncDiskFileChunk::truncate(int64_t newSize) {
  if (!isOpened()) {
    return DISKFILE_NOT_OPEN;
  }
  IF_ERROR_RETURN(flushWriteBuffer());
  if (!file_.isOpened()) {
    IF_ERROR_RETURN(ensureOpenNonDirect());
  }
  IF_ERROR_RETURN(file_.truncate(newSize));
  size_ = newSize;
  return SUCCESS;
}

int AsyncDiskFileChunk::read(void* buffer, size_t count, size_t& outReadSize) {
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

int64_t AsyncDiskFileChunk::getSize() const {
  return size_;
}

bool AsyncDiskFileChunk::contains(int64_t fileOffset) const {
  return fileOffset >= offset_ && fileOffset < offset_ + size_;
}

int AsyncDiskFileChunk::tell(int64_t& outFilepos) const {
  outFilepos = file_position_ + (current_buffer_ ? current_buffer_->size() : 0);

  return SUCCESS;
}

int AsyncDiskFileChunk::seek(int64_t pos, int origin) {
  if (!isOpened()) {
    return DISKFILE_NOT_OPEN;
  }
  // We don't know if we'll be reading or overwriting existing data, flush the buffers, and return
  // any errors that may surface from the completing operations
  IF_ERROR_RETURN(flushWriteBuffer());
  if (!file_.isOpened()) {
    IF_ERROR_RETURN(ensureOpenNonDirect());
  }

  // We track the file offset ourselves, but let the OS compute the final position, as our own
  // `size_` member may not reflect the current size of the chunk.
  int64_t position = 0;
  IF_ERROR_RETURN(file_.seek(file_position_, SEEK_SET, position));
  IF_ERROR_RETURN(file_.seek(pos, origin, position));
  file_position_ = position;
  return SUCCESS;
}

const string& AsyncDiskFileChunk::getPath() const {
  return path_;
}

void AsyncDiskFileChunk::setOffset(int64_t newOffset) {
  offset_ = newOffset;
}

int64_t AsyncDiskFileChunk::getOffset() const {
  return offset_;
}

int AsyncDiskFileChunk::flushWriteBuffer() {
  IF_ERROR_RETURN(waitForAsyncWrites());
  return writeTail();
}

int AsyncDiskFileChunk::waitForAsyncWrites() {
  while (!buffers_writing_.empty() || !buffers_queued_.empty()) {
    reapWrites(true);
  }
  return async_error_;
}

int AsyncDiskFileChunk::writeTail() {
  if (current_buffer_ == nullptr) {
    return SUCCESS;
  }
  if (!current_buffer_->empty()) {
    IF_ERROR_RETURN(ensureOpenNonDirect());
    size_t written = 0;
    const int error =
        file_.pwrite(current_buffer_->data(), current_buffer_->size(), file_position_, written);
    file_position_ += written;
    if (error != SUCCESS) {
      current_buffer_->consume(written);
      return error;
    }
  }
  free_buffer(current_buffer_);
  // Switching descriptors to write the tail may have recorded an error closing the old one.
  return async_error_;
}

int AsyncDiskFileChunk::ensureOpenNonDirect() {
  return ensureOpen_(supported_flags_ & ~O_DIRECT);
}

int AsyncDiskFileChunk::ensureOpenDirect() {
  return ensureOpen_(supported_flags_);
}

int AsyncDiskFileChunk::ensureOpen_(int requested_flags) {
  const char* mode = file_mode_;
  if (mode == nullptr) {
    return DISKFILE_NOT_OPEN;
  }
#if !IS_WINDOWS_PLATFORM()
  const bool wantDirect = (requested_flags & O_DIRECT) != 0;
#endif
  if (file_.isOpened()) {
    if (requested_flags == current_flags_) {
      return SUCCESS;
    }
#if !IS_WINDOWS_PLATFORM()
    // Keep writing to the open file: reopening by path alone can fail, or open a different file.
    // Windows can't change a handle's buffering, so it reopens.
    if (((requested_flags ^ current_flags_) & ~O_DIRECT) == 0) {
      int error = SUCCESS;
      if (wantDirect) {
        error = file_.setDirect(true);
        if (error == EINVAL) {
          disableDirectIo();
          return SUCCESS;
        }
      } else {
        // Some stacked file systems don't pass an fcntl() clearing O_DIRECT down to the lower file,
        // which then rejects unaligned writes (S705350). A new descriptor doesn't have that
        // problem.
        int closeError = SUCCESS;
        error = file_.reopenSameFile(path_, mode, requested_flags, closeError);
        if (closeError != SUCCESS) {
          recordAsyncError(closeError);
        }
        if (error != SUCCESS) {
          error = file_.setDirect(false);
        }
      }
      if (error == SUCCESS) {
        current_flags_ = requested_flags;
      }
      return error;
    }
#endif
    const int closeError = file_.close();
    if (closeError != SUCCESS) {
      recordAsyncError(closeError);
    }
  }
  if (!isReadOnlyMode(mode)) {
    IF_ERROR_RETURN(alloc_write_buffers());
  }
  int error = file_.open(path_, mode, requested_flags);
#if !IS_WINDOWS_PLATFORM()
  if (error == EINVAL && wantDirect) {
    const int bufferedFlags = requested_flags & ~O_DIRECT;
    error = file_.open(path_, mode, bufferedFlags);
    if (error == SUCCESS) {
      disableDirectIo();
      requested_flags = bufferedFlags;
    }
  }
#endif
  if (error != 0) {
    return error;
  }
  current_flags_ = requested_flags;
  // Reopening must never truncate what was written, even after a failed reopen closed the file.
  if (mode[0] == 'w') {
    file_mode_ = "rb+";
  }
  return SUCCESS;
}

void AsyncDiskFileChunk::disableDirectIo() {
  XR_LOGCW(
      VRS_DISKFILECHUNK,
      "O_DIRECT appears not to be supported for {}, falling back to non-direct IO",
      path_);
  supported_flags_ &= ~O_DIRECT;
}

void AsyncDiskFileChunk::complete_write(AsyncBuffer* buffer, ssize_t io_return, int io_errno) {
  if (io_return == buffer->size()) {
    if (io_errno != SUCCESS) {
      XR_LOGCD(
          VRS_DISKFILECHUNK, "io_return was the size of the buffer, but io_errno is {}", io_errno);
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
    recordAsyncError(io_errno);
  }
  free_buffer(buffer);
}

void AsyncDiskFileChunk::reapWrites(bool waitForOne) {
  while (true) {
    const size_t freeCount = buffers_free_.size();
#if POSIX_AIO_SUPPORTED()
    for (size_t index = 0; index < buffers_writing_.size();) {
      AsyncBuffer* buffer = buffers_writing_[index];
      ssize_t written = 0;
      int error = SUCCESS;
      if (buffer->poll_write(written, error)) {
        buffers_writing_.erase(buffers_writing_.begin() + index);
        complete_write(buffer, written, error);
      } else {
        ++index;
      }
    }
#endif
    pump_buffers();
    if (!waitForOne || buffers_free_.size() > freeCount || buffers_writing_.empty()) {
      return;
    }
    waitForAWriteToFinish();
  }
}

void AsyncDiskFileChunk::waitForAWriteToFinish() {
#if POSIX_AIO_SUPPORTED()
  writes_to_wait_for_.clear();
  std::transform(
      buffers_writing_.begin(),
      buffers_writing_.end(),
      std::back_inserter(writes_to_wait_for_),
      [](const AsyncBuffer* buffer) { return buffer->control_block(); });
  // Returns early on a signal (EINTR): the caller polls again either way.
  aioSuspend(writes_to_wait_for_.data(), static_cast<int>(writes_to_wait_for_.size()));
#endif
}

AsyncBuffer* AsyncDiskFileChunk::get_free_buffer() {
  while (buffers_free_.empty()) {
    if (buffers_writing_.empty() && buffers_queued_.empty()) {
      return nullptr;
    }
    reapWrites(true);
  }
  auto* buffer = buffers_free_.back();
  buffers_free_.pop_back();
  assert(buffer);
  assert(buffer->empty());
  return buffer;
}

void AsyncDiskFileChunk::free_buffer(AsyncBuffer*& buffer) {
  buffer->clear();
  buffers_free_.push_back(buffer);
  buffer = nullptr;
}

void AsyncDiskFileChunk::pump_buffers(const WriteSubmitter& submit) {
  while (buffers_writing_.size() < iodepth_ && !buffers_queued_.empty()) {
    auto& item = buffers_queued_.front();
    auto* buffer = item.buffer_;
    const int64_t offset = item.offset_;
    const int result = submit ? submit(item) : buffer->start_write(item.file_, item.offset_);
    buffers_queued_.pop_front();
    if (result == SUCCESS) {
      buffers_writing_.push_back(buffer);
    } else {
      writeRejectedBuffer(*buffer, offset, result);
      free_buffer(buffer);
    }
  }
}

void AsyncDiskFileChunk::writeRejectedBuffer(AsyncBuffer& buffer, int64_t offset, int submitError) {
  if (!rejected_submission_logged_) {
    rejected_submission_logged_ = true;
    XR_LOGCW(
        VRS_DISKFILECHUNK,
        "Asynchronous write rejected for {}: {}. Writing synchronously instead.",
        path_,
        errorCodeToMessage(submitError));
  }
  size_t written = 0;
  const int error = file_.pwrite(buffer.data(), buffer.size(), offset, written);
  if (error != SUCCESS) {
    recordAsyncError(error);
  }
}

void AsyncDiskFileChunk::recordAsyncError(int error) {
  if (async_error_ == SUCCESS) {
    async_error_ = error;
    XR_LOGCE(VRS_DISKFILECHUNK, "Write to {} failed: {}", path_, errorCodeToMessageWithCode(error));
  }
}

int AsyncDiskFileChunk::alloc_write_buffers() {
  assert(buffers_writing_.empty());
  buffers_free_.reserve(num_buffers_);
  buffers_.reserve(num_buffers_);
  // Tracking a write libc accepted must never fail to allocate.
  buffers_writing_.reserve(num_buffers_);
#if POSIX_AIO_SUPPORTED()
  writes_to_wait_for_.reserve(num_buffers_);
#endif
  while (buffers_.size() < num_buffers_) {
    auto buffer = AsyncBuffer::make(buffer_size_, mem_align_, offset_align_);
    if (!buffer) {
      return ENOMEM;
    }
    buffers_free_.push_back(buffer.get());
    buffers_.push_back(std::move(buffer));
  }
  return SUCCESS;
}

int AsyncDiskFileChunk::free_write_buffers() {
  assert(buffers_free_.size() == buffers_.size());
  assert(buffers_writing_.empty());
  assert(buffers_queued_.empty());
  current_buffer_ = nullptr;
  buffers_free_.clear();
  buffers_.clear();
  return SUCCESS;
}

int AsyncDiskFileChunk::init_parameters(const FileSpec::Extras& options) {
  supported_flags_ = 0;
  if (!helpers::getBool(options, "direct", use_directio_) &&
      !helpers::getBool(options, "directio", use_directio_)) {
    use_directio_ = true;
  }

#ifdef VRS_ASYNC_DISKFILE_TSAN
  // N.B. glibc runs AIO requests on threads it spawns itself, which are not tsan-instrumented.
  // Force the use of the non-aio APIs in this case.
  ioengine_ = IoEngine::Sync;
#elif POSIX_AIO_SUPPORTED()
  {
    ioengine_ = IoEngine::AIO; // default, unless overridden
    auto it = options.find("ioengine");
    if (it != options.end()) {
      // ioengine names here have been chosen to correspond to the `fio` program's `ioengine` as
      // closely as possible. `sync` writes everything right away, without O_DIRECT, without
      // buffering in this class.
      ioengine_ = IoEngineTypeConverter::toEnum(it->second);
    }
  }
#else
  // No POSIX AIO available (Android NDK does not provide <aio.h>, Windows).
  // Default to PSync (pwrite, with O_DIRECT unless disabled), which needs no POSIX AIO.
  {
    ioengine_ = IoEngine::PSync;
    auto it = options.find("ioengine");
    if (it != options.end()) {
      ioengine_ = IoEngineTypeConverter::toEnum(it->second);
      if (ioengine_ == IoEngine::AIO) {
        XR_LOGCW(VRS_DISKFILECHUNK, "AIO engine not available, falling back to PSync");
        ioengine_ = IoEngine::PSync;
      }
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

  // Allow overrides, clamped, but don't check that they are powers of two, on the assumption that
  // the underlying allocations or writes will fail if they're bad values. The buffer size is
  // checked below.

  uint64_t temp_u64 = 0;
  mem_align_ = helpers::getByteSize(options, "mem_align", temp_u64) ? temp_u64 : mem_align_;
  mem_align_ = std::clamp<size_t>(mem_align_, 1, 16 * 1024);
  offset_align_ =
      helpers::getByteSize(options, "offset_align", temp_u64) ? temp_u64 : offset_align_;
  offset_align_ = std::clamp<size_t>(offset_align_, 1, 16 * 1024);

  // The defaults below might not be optimal for your rig.
  // They can still be overwritten with the parameter names below from the input URI.
  // fio testing showed each worker using 32MB buffers for non-pre-allocated disk was pretty
  // good.

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

  // By default, every buffer can be in flight
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

} // namespace vrs

#endif
