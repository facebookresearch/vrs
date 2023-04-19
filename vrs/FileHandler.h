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

#include <cstdint>

#include <functional>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <vrs/FileSpec.h>
#include <vrs/helpers/EnumTemplates.hpp>

namespace vrs {

using std::map;
using std::string;
using std::vector;

/// Caching strategy requests
enum class CachingStrategy {
  Undefined = 0,

  Passive, ///< (default) Read & cache on-demand (don't prefetch).
  Streaming, ///< Automatically download data "forward", using last read-request as a hint.
  StreamingBidirectional, ///< Automatically download data "forward" and "backward", using last
                          ///< read-request as a hint.
  StreamingBackward, ///< Automatically download data "backward", using last read-request as a hint.
  ReleaseAfterRead, ///< Same as "Passive" but release used cache blocks immediately after read.

  COUNT
};

string toString(CachingStrategy cachingStrategy);

template <>
CachingStrategy toEnum<>(const string& name);

/// \brief Class to abstract VRS file system operations, to enable support for alternate storage
/// methods, in particular network/cloud storage implementations.
///
/// For simplicity, in this documentation, we will references "files", but they might be one or more
/// data blobs on a network storage.
///
/// VRS file users probably only need to use RecordFileReader & RecordFileWriter, but they have the
/// option to use FileHandler directly to access files stored on remote file systems, same as VRS.
/// Use FileHandlerFactory::delegateOpen() to find the proper FileHandler implementation and open a
/// file. FileHandler only exposes read operations, because it's the most implementation, while
/// WriteFileHandler extends FileHandler for write operations. Both are abstract classes.
///
/// 'int' return values are status codes: 0 means success, while other values are error codes,
/// which can always be converted to a human readable string using vrs::errorCodeToMessage(code).
/// File sizes and offset are specified using int64_t, which is equivalent to the POSIX behavior.
/// Byte counts use size_t.
class FileHandler {
 public:
  /// Stats for cache.
  struct CacheStats {
    double startTime;
    double waitTime;
    size_t blockReadCount;
    size_t blockMissingCount;
    size_t blockPendingCount;
    size_t sequenceSize;
  };

  using CacheStatsCallbackFunction = std::function<void(const CacheStats& stats)>;

  FileHandler(const string& fileHandlerName) : fileHandlerName_{fileHandlerName} {}

  /// Make a new instance of the concrete class implementing this interface in its default state,
  /// no matter what this object's state is, so that we can access more files using the same method.
  /// @return A new object of the concrete type, ready to be used to open a new file.
  virtual std::unique_ptr<FileHandler> makeNew() const = 0;

  /// Delete the object. Derived classes should make sure to close their file(s)/handle(s),
  /// probably simply by calling close().
  virtual ~FileHandler() = default;

  /// Open a file in read-only mode.
  /// @param filePath: a disk path, or anything that the particular module recognizes.
  /// @return A status code, 0 meaning success.
  virtual int open(const string& filePath);
  /// Open a file in read-only mode.
  /// @param fileSpec: a file spec supported by this file handler.
  /// @return A status code, 0 meaning success.
  virtual int openSpec(const FileSpec& fileSpec) = 0;

  /// Open a file, while giving the opportunity to the FileHandler to delegate the file operations
  /// to another FileHandler. With this method, a FileHandler can be capable of deciding which other
  /// FileHandler is the right one to open a file, after inspection, parsing of the path, or lookup.
  /// @param path: file specification.
  /// @param outNewDelegate: If provided, might be a fallback FileHandler to use.
  /// On exit, may be set to a different FileHandler than the current object, if the current
  /// FileHandler was not ultimately the right one to handle the provided path,
  /// or cleared if the current FileHandler should be used to continue accessing the file.
  /// @return A status code, 0 meaning success.
  /// Use errorCodeToString() to get an error description.
  virtual int delegateOpenSpec(
      const FileSpec& fileSpec,
      std::unique_ptr<FileHandler>& outNewDelegate);

  /// Tell if a file is actually open.
  /// @return True if a file is currently open.
  virtual bool isOpened() const = 0;

  /// Get the total size of all the chunks considered.
  /// @return The total size of the open file, or 0.
  virtual int64_t getTotalSize() const = 0;
  /// Close the file & free all the held resources, even if an error occurs.
  /// @return A status code for first error while closing, or 0, meaning success.
  virtual int close() = 0;

  /// Skip a number of bytes further in the file, in a chunk aware way.
  /// @param offset: the number of bytes to skip.
  /// @return A status code, 0 meaning success.
  virtual int skipForward(int64_t offset) = 0;
  /// Set the file position at an arbitrary position, in a chunk aware way.
  /// @param offset: the absolute position to jump to, which may be forward or backward.
  /// @return A status code, 0 meaning success.
  virtual int setPos(int64_t offset) = 0;
  /// Check if a number of bytes are available for immediate return (e.g. on disk or in-cache)
  /// @param length: the number of bytes to check availability of.
  /// @return true if available, false if unavailable (e.g. requiring a network fetch)
  virtual bool isAvailableOrPrefetch(size_t length) {
    return !isRemoteFileSystem();
  }
  /// Read a number of bytes, in a chunk aware way.
  /// If fewer than length bytes can be read, an error code is returned,
  /// then use getLastRWSize() to know how many bytes were really read.
  /// If there are too few remaining bytes in the current chunk, then the new chunk is opened and
  /// read, until enough data can be read.
  /// @param buffer: a buffer to the bytes to write.
  /// @param length: the number of bytes to write.
  /// @return A status code, 0 meaning success and length bytes were successfully read.
  virtual int read(void* buffer, size_t length) = 0;
  /// Helper to read trivially copyable objects, in a chunk aware way.
  template <typename T, std::enable_if_t<std::is_trivially_copyable<T>::value, int> = 0>
  int read(T& object) {
    return read(&object, sizeof(object));
  }
  /// Get the number of bytes actually moved during the last read or write operation.
  /// @return The number of bytes last read or written during the last read or write call.
  virtual size_t getLastRWSize() const = 0;

  /// Find out if the file is currently open in read-only mode.
  /// @return True if the file is currently open in read-only mode. Undefined if no file is open.
  virtual bool isReadOnly() const;

  /// Get the list of chunks, path + size.
  /// @return A succession of path-size pairs.
  virtual vector<std::pair<string, int64_t>> getFileChunks() const = 0;
  /// Call this method to forget any chunk beyond this file size.
  virtual void forgetFurtherChunks(int64_t maxSize) = 0;

  /// Get the last error code.
  /// @return A status code, 0 meaning success.
  virtual int getLastError() const = 0;
  /// Tell if we are at the end of the last chunk.
  /// @return True if the read/write pointer is past the last byte of the file.
  virtual bool isEof() const = 0;
  /// Get the absolute position in the file, in a chunk aware way.
  /// @return The absolute position in the file, including all previous chunks.
  virtual int64_t getPos() const = 0;
  /// Get position in the current chunk.
  /// @return The current position in the current chunk.
  virtual int64_t getChunkPos() const = 0;
  /// Get range of the current chunk.
  /// @param outChunkOffset: index of the first byte of the chunk.
  /// @param outChunkSize: number of bytes in the chunk.
  /// @return A status of 0 if the request succeeded, or some error code (no file is open...)
  virtual int getChunkRange(int64_t& outChunkOffset, int64_t& outChunkSize) const = 0;

  /// Set caching strategy.
  /// @param CachingStrategy: Caching strategy desired.
  /// @return True if the caching strategy was set.
  /// False if the file handler doesn't support the requested strategy, or any particular strategy.
  virtual bool setCachingStrategy(CachingStrategy /*cachingStrategy*/) {
    return false;
  }
  /// Get caching strategy.
  /// @return Caching strategy.
  virtual CachingStrategy getCachingStrategy() const {
    return CachingStrategy::Passive; // default, in particular when there is no caching implemented.
  }

  /// Tell what read operations are going to happen, so that, if the file handler supports it,
  /// data can be cached ahead of time.
  /// @param sequence: a series of (file_offset, length), ordered by anticipated request order.
  /// Read request must not happen exactly as described:
  /// - each segment may be read in multiple successive requests
  /// - section or entire segments may be skipped entirely
  /// Warning: If a read request is made out of order (backward), or outside the sequence, the
  /// predictive cache may be disabled, in part or entirely.
  /// @param clearSequence: Flag on whether to cancel any pre-existing custom read sequence upon
  /// caching starts.
  /// @return True if the file handler support custom read sequences.
  virtual bool prefetchReadSequence(
      const std::vector<std::pair<size_t, size_t>>& sequence,
      bool clearSequence = true) {
    return false;
  }

  virtual bool setStatsCallback(CacheStatsCallbackFunction /* callback */) {
    return false;
  }

  /// Purge read cache buffer, if any.
  /// Sets the caching strategy to Passive, and clears any pending read sequence.
  /// @return True if the read caches were cleared (or there were none to begin with).
  virtual bool purgeCache() {
    return true;
  }

  string getFileHandlerName() const {
    return fileHandlerName_;
  }

  bool isFileHandlerMatch(const FileSpec& fileSpec) const;

  /// When converting a URI "path" to a FileSpec, some custom parsing maybe required, or maybe
  /// the FileHandler want to delegate to another FileHandler for when the file needs to be opened.
  /// @param inOutFileSpec: on input, both the fileHandlerName & uri fields are set.
  /// All the other fields of the FileSpec object are cleared, and uri holds the full original uri.
  /// @param colonIndex: index of the ':' character of the uri.
  /// @return A status code, 0 on success, which doesn't necessarily mean that the file/object
  /// exists or can be opened, merely, that parsing did not fail.
  /// On success, any of the fields may have been set or changed, including fileHandlerName and uri.
  virtual int parseUri(FileSpec& inOutFileSpec, size_t colonIndex) const;

  /// Tell if the file handler is handling remote data, that might need caching for instance.
  /// Because most custom file systems implementation are not local FS, defaults to true!
  virtual bool isRemoteFileSystem() const;
  /// Tell if the file handler is probably slow, and extra progress information might be useful.
  virtual bool showProgress() const {
    return isRemoteFileSystem();
  }

 protected:
  int parseFilePath(const string& filePath, FileSpec& outFileSpec) const;

  string fileHandlerName_;
};

/// Helper class to temporarily modify a FileHandler's caching strategy.
class TemporaryCachingStrategy {
 public:
  TemporaryCachingStrategy(FileHandler& handler, CachingStrategy temporaryStrategy)
      : handler_{handler}, originalStrategy_{handler.getCachingStrategy()} {
    handler_.setCachingStrategy(temporaryStrategy);
  }
  ~TemporaryCachingStrategy() {
    handler_.setCachingStrategy(originalStrategy_);
  }

 private:
  FileHandler& handler_;
  CachingStrategy originalStrategy_;
};

} // namespace vrs
