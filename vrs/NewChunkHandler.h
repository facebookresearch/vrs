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

#include <string>
#include <vector>

#include <vrs/VrsExport.h>
#include <vrs/WriteFileHandler.h>
#include <vrs/os/CompilerAttributes.h>

namespace vrs {

/// \brief Listener to be notified when a new file chunk is finalized.
///
/// Definition of a class of objects that can be attached to a RecordFileWriter, to monitor the
/// creation of file chunks, to process them in some way (maybe to upload them in the cloud?).
class VRS_API NewChunkHandler {
 public:
  virtual ~NewChunkHandler() = default;
  /// Callback function to be notified when new file chunks are created.
  /// @param index: the index of the chunk in the file, 0 being the first chunk.
  /// @param path: local file path to the chunk.
  /// @param isLastChunk: flag telling if the chunk is the file's last (last notification).
  /// Note that chunk notifications may come out of sequence, so do not rely on any ordering.
  /// However, when you get this notification, the chunk is complete and will never change,
  /// and you may even delete the chunk (maybe when upload streaming with limited disk space?).
  /// When isLastChunk set to true, the file is complete, and a notification for every of the file's
  /// chunks has been sent. These callbacks can happen from any thread context.
  virtual void newChunk(const string& path, size_t index, bool isLastChunk) = 0;
  /// Callback function to be notified that the recording failed because writing records failed.
  /// It replaces the remaining newChunk() notifications: no notification follows it, and the file
  /// is incomplete. The writer closes the file's chunks and leaves them on disk, unless the write
  /// file handler's abort() discards the file, as AtomicDiskFile does, so that an interrupted live
  /// recording can be recovered. Handlers whose chunks are temporary, such as upload handlers,
  /// may delete them.
  /// Errors while writing the index or closing the file are reported by the writer, but they still
  /// end with the last newChunk() notification.
  /// It is called at most once, from any thread context.
  /// @param error: the error that stopped the recording.
  /// @param unnotifiedChunks: paths of the chunks never passed to newChunk(): the chunk being
  /// written, and the head of split-head files. They may no longer exist if abort() discarded them.
  virtual void recordingFailed(
      MAYBE_UNUSED int error,
      MAYBE_UNUSED const std::vector<std::string>& unnotifiedChunks) {}
};

/// \brief Helper to make new chunks notifications simpler and safer.
///
/// New chunks notifications must come after the chunk has been closed, which leads to ugly/unsafe
/// code. This helper class gathers the details about the current chunk, so that the notification,
/// if any, can be sent at the right time.
class VRS_API NewChunkNotifier {
 public:
  NewChunkNotifier(WriteFileHandler& file, const std::unique_ptr<NewChunkHandler>& chunkHandler)
      : chunkHandler_{chunkHandler.get()} {
    if (chunkHandler_ != nullptr) {
      file.getCurrentChunk(path_, index_);
    }
  }
  void notify(size_t indexOffset = 0, bool isLastChunk = false) {
    if (chunkHandler_ != nullptr) {
      chunkHandler_->newChunk(path_, index_ + indexOffset, isLastChunk);
    }
  }

 private:
  NewChunkHandler* chunkHandler_;
  string path_;
  size_t index_{};
};

} // namespace vrs
