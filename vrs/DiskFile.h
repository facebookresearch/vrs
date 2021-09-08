// Facebook Technologies, LLC Proprietary and Confidential.

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
  static int writeToFile(const string& path, const void* data, size_t dataSize);
  static int writeToFile(const string& path, const string& string);
  template <typename T, std::enable_if_t<std::is_trivially_copyable<T>::value, int> = 0>
  int writeToFile(const string& path, const T& object) {
    return writeToFile(path, &object, sizeof(T));
  }
  template <typename T, std::enable_if_t<std::is_trivially_copyable<T>::value, int> = 0>
  int writeToFile(const string& path, const vector<T>& v) {
    return writeToFile(path, v.data(), v.size() * sizeof(T));
  }
  /// Read a buffer or a string (automatically adjusts the size)
  static int readFromFile(const string& path, vector<char>& outContent);
  static int readFromFile(const string& path, string& outString);
  template <typename T, std::enable_if_t<std::is_trivially_copyable<T>::value, int> = 0>
  int readFromFile(const string& path, T& object) {
    return readFromFile(path, &object, sizeof(T));
  }
  /// Read a buffer of an exact size, fails if the size isn't perfectly right.
  static int readFromFile(const string& path, void* data, size_t dataSize);

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

} // namespace vrs
