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

#include "DiskFile.h"

#include <algorithm>
#include <memory>
#include <utility>

#ifdef GTEST_BUILD
#include <gtest/gtest.h>
#endif

#define DEFAULT_LOG_CHANNEL "DiskFile"
#include <logging/Log.h>
#include <logging/Verify.h>

#include <vrs/helpers/FileMacros.h>
#include <vrs/helpers/Strings.h>
#include <vrs/os/Platform.h>
#include <vrs/os/Utils.h>

#include "Compressor.h"
#include "Decompressor.h"
#include "ErrorCode.h"

#include "DiskFileChunk.hpp"

using namespace std;

namespace vrs {

constexpr int kMaxFilesOpenCount = 2;

const string& DiskFile::staticName() {
  static const string sDiskFileHandlerName = "diskfile";
  return sDiskFileHandlerName;
}

DiskFile::DiskFile() : chunks_(std::make_unique<std::vector<DiskFileChunk>>()) {}

DiskFile::~DiskFile() {
  DiskFile::close(); // overrides not available in constructors & destructors
}

unique_ptr<FileHandler> DiskFile::makeNew() const {
  return make_unique<DiskFile>();
}

const string& DiskFile::getFileHandlerName() const {
  return staticName();
}

int DiskFile::close() {
  lastError_ = 0;
  for (auto& chunk : *chunks_) {
    if (chunk.isOpened()) {
      int error = chunk.close();
      if (error != 0 && lastError_ == 0) {
        lastError_ = error;
      }
      filesOpenCount_--;
    }
  }
#ifdef GTEST_BUILD
  EXPECT_EQ(filesOpenCount_, 0);
#endif
  options_.clear();
  chunks_->clear();
  currentChunk_ = nullptr;
  filesOpenCount_ = 0;
  lastRWSize_ = 0;
  return lastError_;
}

int DiskFile::openSpec(const FileSpec& fileSpec) {
  close();
  readOnly_ = true;
  if (!fileSpec.fileHandlerName.empty() && !fileSpec.isDiskFile()) {
    return FILE_HANDLER_MISMATCH;
  }
  options_ = fileSpec.extras;
  if (checkChunks(fileSpec.chunks) != 0 || openChunk(chunks_->data()) != 0) {
    chunks_->clear();
    options_.clear();
  }
  return lastError_;
}

bool DiskFile::isOpened() const {
  return currentChunk_ != nullptr;
}

int DiskFile::create(const string& newFilePath, const map<string, string>& options) {
  close();
  readOnly_ = false;
  options_ = options;
  return addChunk(newFilePath);
}

void DiskFile::forgetFurtherChunks(int64_t fileSize) {
  size_t currentIndex = static_cast<size_t>(currentChunk_ - chunks_->data());
  size_t minChunkCount = currentIndex + 1;
  // forget any chunk past our read location & given file size
  while (chunks_->size() > minChunkCount && chunks_->back().getOffset() >= fileSize) {
    chunks_->pop_back();
  }
  currentChunk_ = chunks_->data() + currentIndex; // in case resize re-allocated the vector
}

int DiskFile::skipForward(int64_t offset) {
  int64_t chunkPos{};
  if ((lastError_ = currentChunk_->tell(chunkPos)) != 0) {
    return lastError_;
  }
  if (chunkPos + offset < currentChunk_->getSize()) {
    lastError_ = currentChunk_->seek(offset, SEEK_CUR);
    return lastError_;
  }
  return setPos(currentChunk_->getOffset() + chunkPos + offset);
}

int DiskFile::setPos(int64_t offset) {
  if (trySetPosInCurrentChunk(offset)) {
    return lastError_;
  }
  DiskFileChunk* lastChunk = &chunks_->back();
  DiskFileChunk* chunk = currentChunk_;
  if (offset < chunk->getOffset()) {
    chunk = &chunks_->front();
  }
  while (chunk < lastChunk && offset >= chunk->getOffset() + chunk->getSize()) {
    chunk++;
  }
  if (chunk != currentChunk_ && (openChunk(chunk) != 0 || trySetPosInCurrentChunk(offset))) {
    return lastError_;
  }
  lastError_ = DISKFILE_INVALID_OFFSET;
  return lastError_;
}

// Try to set position within the current chunk.
// Returns false is chunk isn't the right one, otherwise,
// calls file seek in current chunk, sets lastError_ accordingly, and always returns true.
bool DiskFile::trySetPosInCurrentChunk(int64_t offset) {
  if (currentChunk_->contains(offset) ||
      (currentChunk_ == &chunks_->back() &&
       (readOnly_ ? offset == currentChunk_->getOffset() + currentChunk_->getSize()
                  : offset >= currentChunk_->getOffset()))) {
    lastError_ = currentChunk_->seek(offset - currentChunk_->getOffset(), SEEK_SET);
    return true;
  }
  return false;
}

int64_t DiskFile::getTotalSize() const {
  if (chunks_->empty()) {
    return 0;
  }
  const DiskFileChunk& lastChunk = chunks_->back();
  return lastChunk.getOffset() + lastChunk.getSize();
}

vector<pair<string, int64_t>> DiskFile::getFileChunks() const {
  vector<pair<string, int64_t>> chunks;
  chunks.reserve(chunks_->size());
  for (const DiskFileChunk& chunk : *chunks_) {
    chunks.emplace_back(chunk.getPath(), chunk.getSize());
  }
  return chunks;
}

int DiskFile::read(void* buffer, size_t length) {
  lastRWSize_ = 0;
  lastError_ = 0;
  if (length == 0) {
    return lastError_;
  }
  do {
    size_t requestSize = length - lastRWSize_;
    size_t readSize{};
    lastError_ =
        currentChunk_->read(static_cast<char*>(buffer) + lastRWSize_, requestSize, readSize);
    lastRWSize_ += readSize;
    if (readSize == requestSize) {
      return lastError_;
    }
    if (currentChunk_->eof() == 0 || isLastChunk()) {
      // give up and make sure that if we did not read enough bytes, we flag it
      if (lastError_ == 0) {
        lastError_ = DISKFILE_NOT_ENOUGH_DATA;
      }
      return lastError_;
    }
    // we reached the end of a chunk, but we have more to read in the next one
    if (openChunk(currentChunk_ + 1) != 0) {
      return lastError_; // we can't open the next chunk
    }
    lastError_ = currentChunk_->seek(0, SEEK_SET);
  } while (lastError_ == 0);
  return lastError_;
}

size_t DiskFile::getLastRWSize() const {
  return lastRWSize_;
}

bool DiskFile::reopenForUpdatesSupported() const {
  return true;
}

int DiskFile::reopenForUpdates() {
  if (!isOpened()) {
    return DISKFILE_NOT_OPEN;
  }
  // close all possibly opened chunks, as they are not opened with the right mode.
  for (auto& chunk : *chunks_) {
    closeChunk(&chunk);
  }
  readOnly_ = false;
  if (openChunk(currentChunk_) != 0) {
    readOnly_ = true;
    return lastError_;
  }
  return 0;
}

bool DiskFile::isReadOnly() const {
  return readOnly_;
}

int DiskFile::write(const void* buffer, size_t length) {
  lastRWSize_ = 0;
  if (!isOpened()) {
    return DISKFILE_NOT_OPEN;
  }
  if (readOnly_) {
    return DISKFILE_READ_ONLY;
  }
  lastError_ = 0;
  if (length == 0) {
    return lastError_;
  }
  lastError_ = currentChunk_->write(buffer, length, lastRWSize_);
  return lastError_;
}

int DiskFile::overwrite(const void* buffer, size_t length) {
  lastRWSize_ = 0;
  if (readOnly_) {
    lastError_ = DISKFILE_READ_ONLY;
    return lastError_;
  }
  lastError_ = 0;
  if (length == 0) {
    return lastError_;
  }
  do {
    size_t requestSize = (length > lastRWSize_) ? length - lastRWSize_ : 0;
    if (!isLastChunk()) {
      int64_t pos{};
      if ((lastError_ = currentChunk_->tell(pos)) != 0) {
        return lastError_;
      }
      int64_t maxRequest = max<int64_t>(currentChunk_->getSize() - pos, 0);
      requestSize = min<size_t>(requestSize, static_cast<size_t>(maxRequest));
    }
    size_t writtenSize{};
    lastError_ = currentChunk_->write(
        static_cast<const char*>(buffer) + lastRWSize_, requestSize, writtenSize);
    lastRWSize_ += writtenSize;
    if (lastRWSize_ == length || lastError_ != 0) {
      return lastError_;
    }
    // we reached the end of a chunk, but we have more to write in the next one
    openChunk(currentChunk_ + 1);
  } while (lastError_ == 0);
  return lastError_; // we could not open the next chunk
}

int DiskFile::truncate() {
  if (readOnly_) {
    lastError_ = DISKFILE_READ_ONLY;
    return lastError_;
  }
  int64_t chunkSize{};
  if ((lastError_ = currentChunk_->tell(chunkSize)) == 0 &&
      (lastError_ = currentChunk_->truncate(chunkSize)) == 0) {
    // update the following chunks' offset
    size_t chunkIndex = static_cast<size_t>(currentChunk_ - chunks_->data());
    int64_t nextChunkOffset = currentChunk_->getOffset() + currentChunk_->getSize();
    while (++chunkIndex < chunks_->size()) {
      (*chunks_)[chunkIndex].setOffset(nextChunkOffset);
      nextChunkOffset += (*chunks_)[chunkIndex].getSize();
    }
  }
  return lastError_;
}

int DiskFile::getLastError() const {
  return lastError_;
}

bool DiskFile::isEof() const {
  return isLastChunk() && currentChunk_->eof() != 0;
}

int DiskFile::checkChunks(const vector<string>& chunks) {
  int64_t offset = 0;
  for (const string& path : chunks) {
    int64_t chunkSize = os::getFileSize(path);
    if (chunkSize < 0) {
      lastError_ = DISKFILE_FILE_NOT_FOUND;
      break;
    }
    chunks_->emplace_back(path, offset, chunkSize);
    offset += chunkSize;
  }
  return lastError_;
}

int DiskFile::openChunk(DiskFileChunk* chunk) {
  if (chunk->isOpened()) {
    currentChunk_ = chunk;
    chunk->rewind();
    lastError_ = 0;
  } else {
    lastError_ = chunk->open(readOnly_, options_);
    if (lastError_ == 0) {
      if (filesOpenCount_++ > kMaxFilesOpenCount && currentChunk_ != nullptr) {
        closeChunk(currentChunk_);
      }
      currentChunk_ = chunk;
    }
  }
  return lastError_;
}

int DiskFile::addChunk() {
  if (chunks_->empty()) {
    return DISKFILE_NOT_OPEN;
  }
  string newChunkPath = chunks_->front().getPath();
  // if the first file ends with "_1", the second chunk is numbered "_2", etc.
  if (helpers::endsWith(newChunkPath, "_1")) {
    newChunkPath.pop_back();
    newChunkPath += to_string(chunks_->size() + 1);
  } else {
    newChunkPath += '_' + to_string(chunks_->size());
  }
  return addChunk(newChunkPath);
}

int DiskFile::closeChunk(DiskFileChunk* chunk) {
  int error = 0;
  if (chunk->isOpened()) {
    error = chunk->close();
    filesOpenCount_--;
  }
  return error;
}

int DiskFile::addChunk(const string& chunkFilePath) {
  if (!chunks_->empty() && !isLastChunk()) {
    return DISKFILE_INVALID_STATE;
  }
  DiskFileChunk newChunk;
  lastError_ = newChunk.create(chunkFilePath, options_);
  if (lastError_ != SUCCESS) {
    return lastError_;
  }
  filesOpenCount_++;
  int64_t chunkOffset = 0;
  if (currentChunk_ != nullptr && currentChunk_->isOpened()) {
    int64_t pos{};
    if ((lastError_ = currentChunk_->tell(pos)) != 0) {
      return lastError_;
    }
    currentChunk_->setSize(pos);
    if ((lastError_ = currentChunk_->flush()) != 0) {
      // We're s***d: the last chunk is messed up, no point in trying to use the new one
      newChunk.close();
      os::remove(chunkFilePath);
      return lastError_;
    }
    if (!readOnly_ || filesOpenCount_ > kMaxFilesOpenCount) {
      int error = closeChunk(currentChunk_);
      // the chunk is flushed: log an error, but not block the transition to a new chunk
      XR_VERIFY(
          error == 0,
          "Error closing '{}': {}, {}",
          currentChunk_->getPath(),
          error,
          errorCodeToMessage(error));
    }
    chunkOffset = currentChunk_->getOffset() + currentChunk_->getSize();
  }
  newChunk.setOffset(chunkOffset);
  chunks_->push_back(std::move(newChunk));
  currentChunk_ = &chunks_->back();
  lastError_ = 0;
  return 0;
}

bool DiskFile::isLastChunk() const {
  return currentChunk_ == &chunks_->back();
}

int64_t DiskFile::getPos() const {
  int64_t pos{};
  IF_ERROR_LOG(currentChunk_->tell(pos));
  return currentChunk_->getOffset() + pos;
}

int64_t DiskFile::getChunkPos() const {
  int64_t pos{};
  IF_ERROR_LOG(currentChunk_->tell(pos));
  return pos;
}

int DiskFile::getChunkRange(int64_t& outChunkOffset, int64_t& outChunkSize) const {
  if (currentChunk_ != nullptr) {
    DiskFileChunk* chunk = currentChunk_;
    // if we're at the edge of a chunk, return the following chunk
    if (getChunkPos() == chunk->getSize() && !isLastChunk()) {
      chunk++;
    }
    outChunkOffset = chunk->getOffset();
    outChunkSize = chunk->getSize();
    return 0;
  }
  return DISKFILE_NOT_OPEN;
}

bool DiskFile::getCurrentChunk(string& outChunkPath, size_t& outChunkIndex) const {
  if (currentChunk_ == nullptr) {
    return false;
  }
  outChunkPath = currentChunk_->getPath();
  outChunkIndex = static_cast<size_t>(currentChunk_ - chunks_->data());
  return true;
}

bool DiskFile::isRemoteFileSystem() const {
  return false;
}

int DiskFile::writeZstdFile(const string& path, const void* data, size_t dataSize) {
  AtomicDiskFile file;
  IF_ERROR_LOG_AND_RETURN(file.create(path));
  if (dataSize > 0) {
    Compressor compressor;
    uint32_t frameSize = 0;
    IF_ERROR_LOG_AND_RETURN(
        compressor.startFrame(dataSize, CompressionPreset::ZstdMedium, frameSize));
    IF_ERROR_RETURN(compressor.addFrameData(file, data, dataSize, frameSize));
    IF_ERROR_RETURN(compressor.endFrame(file, frameSize));
  }
  return SUCCESS;
}

int DiskFile::writeZstdFile(const string& path, const string& string) {
  return writeZstdFile(path, string.data(), string.size());
}

namespace {
template <class T>
int readZstdFileTemplate(const string& path, T& outContent) {
  outContent.clear();
  DiskFile file;
  IF_ERROR_LOG_AND_RETURN(file.open(path));
  int64_t fileSize = file.getTotalSize();
  if (fileSize <= 0) {
    return (fileSize < 0) ? FAILURE : 0;
  }
  Decompressor decompressor;
  size_t frameSize = 0;
  size_t maxReadSize = static_cast<size_t>(fileSize);
  IF_ERROR_LOG_AND_RETURN(decompressor.initFrame(file, frameSize, maxReadSize));
  outContent.resize(frameSize / sizeof(typename T::value_type));
  IF_ERROR_LOG_AND_RETURN(
      decompressor.readFrame(file, &outContent.front(), frameSize, maxReadSize));
  return maxReadSize == 0 ? SUCCESS : FAILURE;
}
} // namespace

int DiskFile::readZstdFile(const string& path, vector<char>& outContent) {
  return readZstdFileTemplate(path, outContent);
}

int DiskFile::readZstdFile(const string& path, string& outString) {
  return readZstdFileTemplate(path, outString);
}

int DiskFile::readZstdFile(const string& path, void* data, size_t dataSize) {
  DiskFile file;
  IF_ERROR_LOG_AND_RETURN(file.open(path));
  int64_t fileSize = file.getTotalSize();
  if (fileSize <= 0) {
    return (fileSize < 0) ? FAILURE : 0;
  }
  Decompressor decompressor;
  size_t frameSize = 0;
  size_t maxReadSize = static_cast<size_t>(fileSize);
  IF_ERROR_LOG_AND_RETURN(decompressor.initFrame(file, frameSize, maxReadSize));
  if (frameSize != dataSize) {
    return FAILURE;
  }
  IF_ERROR_LOG_AND_RETURN(decompressor.readFrame(file, data, frameSize, maxReadSize));
  return maxReadSize == 0 ? SUCCESS : FAILURE;
}

string DiskFile::readTextFile(const std::string& path) {
  DiskFile file;
  if (file.open(path) == 0) {
    int64_t size = file.getTotalSize();
    const int64_t kMaxReasonableTextFileSize = 50 * 1024 * 1024; // 50 MB is a huge text file...
    if (size > 0 && XR_VERIFY(size < kMaxReasonableTextFileSize)) {
      string str(size, 0);
      if (VERIFY_SUCCESS(file.read(str.data(), size))) {
        return str;
      }
    }
  }
  return {};
}

int DiskFile::writeTextFile(const std::string& path, const std::string& text) {
  DiskFile file;
  IF_ERROR_LOG_AND_RETURN(file.create(path));
  IF_ERROR_LOG_AND_RETURN(file.write(text.data(), text.size()));
  return file.close();
}

int DiskFile::parseUri(FileSpec& inOutFileSpec, size_t /*colonIndex*/) const {
  string scheme;
  string path;
  map<string, string> queryParams;
  IF_ERROR_RETURN(FileSpec::parseUri(inOutFileSpec.uri, scheme, path, queryParams));

  if (!XR_VERIFY(scheme == getFileHandlerName())) {
    return FILE_HANDLER_MISMATCH;
  }

  inOutFileSpec.fileHandlerName = getFileHandlerName();
  inOutFileSpec.chunks = {path};
  inOutFileSpec.extras = std::move(queryParams);

  return SUCCESS;
}

AtomicDiskFile::~AtomicDiskFile() {
  AtomicDiskFile::close(); // overrides not available in constructors & destructors
}

int AtomicDiskFile::create(const std::string& newFilePath, const map<string, string>& options) {
  finalName_ = newFilePath;
  return DiskFile::create(os::getUniquePath(finalName_, 10), options);
}

int AtomicDiskFile::close() {
  if (chunks_->empty() || finalName_.empty() || finalName_ == chunks_->front().getPath()) {
    return DiskFile::close();
  }
  string currentName = chunks_->front().getPath();
  IF_ERROR_RETURN(DiskFile::close());
  int retry = 3;
  int status = 0;
  while ((status = os::rename(currentName, finalName_)) != 0 && os::isFile(currentName) &&
         retry-- > 0) {
    os::remove(finalName_); // if there was a collision, make room
  }
  return status;
}

void AtomicDiskFile::abort() {
  if (isOpened() && !isReadOnly()) {
    vector<string> chunkPaths;
    chunkPaths.reserve(chunks_->size());
    for (const auto& chunk : *chunks_) {
      chunkPaths.emplace_back(chunk.getPath());
    }
    DiskFile::close();
    for (const string& path : chunkPaths) {
      os::remove(path);
    }
  }
}

} // namespace vrs
