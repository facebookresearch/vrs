// Copyright (c) Facebook, Inc. and its affiliates. Confidential and proprietary.

#pragma once

#include "FileHandler.h"

namespace vrs {

using std::string;
using std::vector;

/// \brief The WriteFileHandler interface adds write operations to the FileHandler interface.
///
/// There are two types of WriteFileHandler implementations. They might be able to:
/// - create files, which which case the create() API should be implemented.
/// - modify an existing file, in which case reopenForUpdates() gives write access.
/// Some class of cloud storage are immutable, hence the important distinction.
/// VRS' DiskFile offers the most comprehensive implementation, because it is designed to fit all
/// the needs of advanced VRS file creations, with chunking, but it should be perfectly usable
/// on its own. On the other hand, network WriteFileHandler implementations might have very specific
/// behaviors designed to accomodate the specifities of the network storage they implement.
/// In particular, they might have specific behaviors to handle VRS file creation in the cloud,
/// which requires the first part of the file to be edited last (and probably uploaded last too).
/// Therefore, network WriteFileHandler implementations might not be usable for anything else than
/// VRS file creation.
class WriteFileHandler : public FileHandler {
 public:
  WriteFileHandler(const string& fileHandlerName) : FileHandler(fileHandlerName) {}

  /// Create a new file for writing.
  /// @param newFilePath: a disk path to create the file.
  /// @return A status code, 0 meaning success.
  virtual int create(const string& newFilePath) = 0;

  /// Tell if modifying files is supported by this FileHandler implementation.
  /// @return True if file modification and creation is supported.
  virtual bool reopenForUpdatesSupported() const = 0;

  /// Switch from read-only to read-write mode.
  /// Reopen the same file for modification writes.
  /// @return A status code, 0 meaning success.
  virtual int reopenForUpdates() = 0;

  /// Find out if the file is currently open in read-only mode.
  /// @return True if the file is currently open in read-only mode. Undefined if no file is open.
  bool isReadOnly() const override = 0;

  /// Write to the current chunk, possibly expanding it.
  /// @param buffer: a pointer to the data bytes to write.
  /// @param length: the number of bytes to write.
  /// @return A status code, 0 meaning success.
  /// In case of error, you can use getLastRWSize() to know how many bytes were really writen.
  virtual int write(const void* buffer, size_t length) = 0;
  /// Helper for trivially copyable objects.
  template <typename T, std::enable_if_t<std::is_trivially_copyable<T>::value, int> = 0>
  int write(const T& object) {
    return write(&object, sizeof(object));
  }

  /// Write a number of bytes to the file, in a chunk aware way,
  /// only ever extending the file's last chunk.
  /// @param buffer: a pointer to the data bytes to write.
  /// @param length: the number of bytes to write.
  /// @return A status code, 0 meaning success.
  virtual int overwrite(const void* buffer, size_t length) = 0;
  /// Helper for trivially copyable objects.
  template <typename T, std::enable_if_t<std::is_trivially_copyable<T>::value, int> = 0>
  int overwrite(const T& object) {
    return overwrite(&object, sizeof(object));
  }

  /// Append a new chunk to the current file, when in writing to disk.
  /// The next write will happen at the beginning of the new chunk.
  /// @return A status code, 0 meaning success.
  virtual int addChunk() = 0;
  /// Truncate chunk to the current file position. Use with care.
  /// @return A status code, 0 meaning success.
  virtual int truncate() = 0;
  /// Get the path of the current chunk, or an empty string if no chunk is open.
  virtual bool getCurrentChunk(string& outChunkPath, size_t& outChunkIndex) const = 0;
};

} // namespace vrs
