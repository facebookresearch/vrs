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

#include <cstdio>

#include "WriteFileHandler.h"

namespace vrs {

using std::string;
using std::vector;

/// FileHandler implementation for disk files, with chunked file support.
class DiskFile : public WriteFileHandler {
 public:
  static const std::string& staticName();

  struct Chunk {
    FILE* file; // may be nullptr or not, but must be set when currentChunk_ points here!
    string path; // path of this chunk
    int64_t offset; // offset of this chunk in the file
    int64_t size; // size of the chunk

    bool contains(int64_t fileOffset) const {
      return fileOffset >= offset && fileOffset < offset + size;
    }
  };

  DiskFile();
  ~DiskFile() override;

  /// Make a new DiskFile object, with a default state.
  std::unique_ptr<FileHandler> makeNew() const override;

  /// Open a file in read-only mode.
  int openSpec(const FileSpec& fileSpec) override;

  /// Tell if a file is actually open.
  bool isOpened() const override;

  /// Create a new file
  int create(const string& newFilePath) override;
  /// Call this method to forget any chunk beyond this file size.
  void forgetFurtherChunks(int64_t fileSize) override;
  /// Get the total size of all the chunks considered.
  int64_t getTotalSize() const override;
  /// Get the list of chunks, path + size.
  vector<std::pair<string, int64_t>> getFileChunks() const override;
  /// Close the file.
  int close() override;

  /// Skip a number of bytes further in the file, in a chunk aware way.
  int skipForward(int64_t offset) override;
  /// Set the file position at an arbitrary position, in a chunk aware way.
  int setPos(int64_t offset) override;

  /// Read a number of bytes, in a chunk aware way.
  int read(void* buffer, size_t length) override;
  /// Helper to read trivially copyable objects, in a chunk aware way.
  template <typename T, std::enable_if_t<std::is_trivially_copyable<T>::value, int> = 0>
  int read(T& object) {
    return read(&object, sizeof(object));
  }
  /// Get the number of bytes actually moved by the last read or write operation.
  size_t getLastRWSize() const override;

  /// Tell if modifying files is supported by this FileHandler implementation.
  bool reopenForUpdatesSupported() const override;
  /// Switch from read-only to read-write mode.
  int reopenForUpdates() override;
  /// Find out if the file is in read-only mode.
  bool isReadOnly() const override;
  /// Write to the current chunk, possibly expanding it.
  int write(const void* buffer, size_t length) override;
  /// Helper for trivially copyable objects.
  template <typename T, std::enable_if_t<std::is_trivially_copyable<T>::value, int> = 0>
  int write(const T& object) {
    return write(&object, sizeof(object));
  }
  /// Write a number of bytes to the file, in a chunk aware way,
  /// only ever extending the file's last chunk.
  int overwrite(const void* buffer, size_t length) override;
  /// Helper for trivially copyable objects.
  template <typename T, std::enable_if_t<std::is_trivially_copyable<T>::value, int> = 0>
  int overwrite(const T& object) {
    return overwrite(&object, sizeof(object));
  }
  /// Append a new chunk to the current file.
  int addChunk() override;
  /// Truncate chunk to the current file position. Use with care.
  int truncate() override;

  /// Get the last error code. 0 means no error.
  int getLastError() const override;
  /// Tell if we are at the end of the last chunk.
  bool isEof() const override;
  /// Get the absolute position in the file, in a chunk aware way.
  int64_t getPos() const override;
  /// Get position in the current chunk.
  int64_t getChunkPos() const override;
  /// Get range of the current chunk.
  int getChunkRange(int64_t& outChunkOffset, int64_t& outChunkSize) const override;
  /// Get the path of the current chunk, or an empty string if no chunk is open.
  bool getCurrentChunk(string& outChunkPath, size_t& outChunkIndex) const override;

  bool isRemoteFileSystem() const override;

  /// Helper methods to write a blob of data, or a string, to disk in one shot.
  /// Note that the data is compressed, so that we have high confidence it wasn't edited.
  static int writeZstdFile(const string& path, const void* data, size_t dataSize);
  static int writeZstdFile(const string& path, const string& string);
  template <typename T, std::enable_if_t<std::is_trivially_copyable<T>::value, int> = 0>
  int writeZstdFile(const string& path, const T& object) {
    return writeZstdFile(path, &object, sizeof(T));
  }
  template <typename T, std::enable_if_t<std::is_trivially_copyable<T>::value, int> = 0>
  int writeZstdFile(const string& path, const vector<T>& v) {
    return writeZstdFile(path, v.data(), v.size() * sizeof(T));
  }
  /// Read a compressed buffer or a string (automatically adjusts the size)
  static int readZstdFile(const string& path, vector<char>& outContent);
  static int readZstdFile(const string& path, string& outString);
  template <typename T, std::enable_if_t<std::is_trivially_copyable<T>::value, int> = 0>
  int readZstdFile(const string& path, T& object) {
    return readZstdFile(path, &object, sizeof(T));
  }
  /// Read a compressed buffer of an exact size, fails if the size isn't perfectly right.
  static int readZstdFile(const string& path, void* data, size_t dataSize);

  /// Read a local file, expected to contain some text
  /// @param path: local file to read
  /// @return The content of the file, if found, or the empty string otherwise.
  /// Will log errors if some unexpected access error happens, but will be silent and
  /// return an empty string if the file doesn't exist.
  static string readTextFile(const string& path);

  virtual int parseUri(FileSpec& intOutFileSpec, size_t colonIndex) const override;

 protected:
  int checkChunks(const vector<string>& chunks);
  int openChunk(Chunk* chunk);
  int closeChunk(Chunk* chunk);
  int addChunk(const string& chunkFilePath);
  bool isLastChunk() const {
    return currentChunk_ == &chunks_.back();
  }
  bool trySetPosInCurrentChunk(int64_t offset);

  vector<Chunk> chunks_; // all the chunks, when a VRS file is opened.
  Chunk* currentChunk_{}; // if a file is opened, **always** points to a valid chunk within chunks_.
  static const int kMaxFilesOpenCount = 2;
  int filesOpenCount_{};

  size_t lastRWSize_;
  int lastError_;
  bool readOnly_;
};

/// Helper class to create a new file with better chances that the content won't be clobbered by
/// another process creating a file with the same name at the same time.
/// The file will be created using a unique name, then after it's closed, it will be renamed to
/// the name requested originally, possibly deleting/replacing what is there.
/// In practice, it's a best effort atomic file behavior, using a temporary file and late renaming,
/// appropriate when file integrity matters more than file persistence in case of collision.
/// The intent:
/// - if multiple processes try to create a file with the same name at the same time, they don't
/// overwrite each other's data.
/// - if a process tries to read the file at about the same time it is being replaced by one or more
/// other processes, it won't find a file, or it will find an older version of the file, or it will
/// find a new version, but it won't find a partially written version of the file.
/// The actual behavior will depend on the actual file system, how atomic its file rename operations
/// are, and how it handles file locking, but the behavior should be appropriate for storing data
/// for caching purposes, when its better to fail saving a cache entry, than creating a corrupt one.
class AtomicDiskFile : public DiskFile {
 public:
  ~AtomicDiskFile() override;

  int create(const string& newFilePath) override;
  int close() override;

  void abort();

 private:
  string finalName_;
};

} // namespace vrs
