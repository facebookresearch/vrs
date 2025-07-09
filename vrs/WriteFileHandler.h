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

#include <vrs/os/CompilerAttributes.h>

#include "ErrorCode.h"
#include "FileHandler.h"

namespace vrs {

using std::string;
using std::vector;

/// \brief The WriteFileHandler interface adds write operations to the FileHandler interface.
///
/// There are two classes of WriteFileHandler implementations:
///  - the ones that can edit data already written, whether editing a previously created file, or
///  while creating a new one, as typically possible with local files,
///  - the immutable kind, which don't allow for overwriting parts of the file already written.
///  They can append content, maybe concatenate chunks, but not modify previously written content.
///  That's a typical restriction for cloud storage systems.
///
/// VRS' DiskFile offers the most comprehensive implementation, because it is designed to fit all
/// the needs of advanced VRS file creations, with chunking, but it can be used with non-VRS files.
///
/// On the other hand, network WriteFileHandler implementations often have very specific behaviors
/// designed to accommodate the specificities of the network storage they implement. These
/// implementation details should ideally be abstracted, but in order to optimize cloud VRS file
/// creation, the abstraction is compromised, and cloud WriteFileHandler implementations are not
/// easily reusable for other applications that VRS.
class WriteFileHandler : public FileHandler {
 public:
  WriteFileHandler() = default;

  /// Create a new WriteFileHandler from a name.
  static unique_ptr<WriteFileHandler> make(const string& fileHandlerName);

  /// Create a new file for writing, using a spec.
  /// The path of the file to create is expected to be in the first chunk.
  /// Optional URI parameters might be provided in the spec' extras.
  virtual int create(const FileSpec& spec) {
    return spec.chunks.empty() ? INVALID_FILE_SPEC : create(spec.chunks.front(), spec.extras);
  }

  /// Create a new file for writing.
  /// @param newFilePath: a disk path to create the file.
  /// @param options: optional parameters to pass when creating the file.
  /// @return A status code, 0 meaning success.
  virtual int create(const string& newFilePath, const map<string, string>& options = {}) = 0;

  /// Create a new file for writing, in split-head file mode, the body part.
  /// @param spec: spec as converted already from initialFilePath, if that helps.
  /// @param initialFilePath: path as given when the file creation was started.
  /// @return A status code, 0 meaning success.
  virtual int createSplitFile(const FileSpec& spec, const string& initialFilePath) {
    // create the (first) user record chunk
    if (spec.chunks.size() == 1) {
      return create(spec.chunks.front() + "_1", spec.extras);
    } else {
      return create(initialFilePath, spec.extras);
    }
  }

  /// When creating a split-head file, we may need to add a new chunk for the head file.
  /// @param inOutSpec: file spec used for the file creation, that will be passed to a
  /// DiskFile's create(inOutSpec) to create the head file,
  /// and to createSplitFile(inOutSpec, options) to create the body file.
  virtual void addSplitHead(MAYBE_UNUSED FileSpec& inOutSpec) {}

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
  /// In case of error, you can use getLastRWSize() to know how many bytes were really written.
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
