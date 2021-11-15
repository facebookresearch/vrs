// Copyright (c) Facebook, Inc. and its affiliates. Confidential and proprietary.

#pragma once

#include <functional>

#include "WriteFileHandler.h"

namespace vrs {

/// \brief Listener to be notified when a new file chunk is finalized.
///
/// Definition of a class of objects that can be attached to a RecordFileWriter, to monitor the
/// creation of file chunks, to process them in some way (maybe to upload them in the cloud?).
class NewChunkHandler {
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
};

/// \brief Helper to make new chunks notifications simpler and safer.
///
/// New chunks notifications must come after the chunk has been closed, which leads to ugly/unsafe
/// code. This helper class gathers the details about the current chunk, so that the notification,
/// if any, can be sent at the right time.
class NewChunkNotifier {
 public:
  NewChunkNotifier(WriteFileHandler& file, const std::unique_ptr<NewChunkHandler>& chunkHandler)
      : chunkHandler_{chunkHandler.get()} {
    if (chunkHandler_) {
      file.getCurrentChunk(path_, index_);
    }
  }
  void notify(size_t indexOffset = 0, bool isLastChunk = false) {
    if (chunkHandler_) {
      chunkHandler_->newChunk(path_, index_ + indexOffset, isLastChunk);
    }
  }

 private:
  NewChunkHandler* chunkHandler_;
  string path_;
  size_t index_;
};

} // namespace vrs
