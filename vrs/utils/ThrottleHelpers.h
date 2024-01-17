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

#include <vrs/RecordFileWriter.h>
#include <vrs/utils/FilterCopyHelpers.h>

namespace vrs::utils {

extern const size_t kDownloadChunkSize;
extern const char* const kResetCurrentLine;

class ThrottledFileDelegate;

/// Class to control memory usage while writing out to a VRS file
/// using a RecordFileWriter object.
class ThrottledWriter {
 public:
  explicit ThrottledWriter(const CopyOptions& options);
  ThrottledWriter(const CopyOptions& options, ThrottledFileDelegate& fileDelegate);

  /// Init writer with latest copy options values (if they were changed since constructor)
  void initWriter();

  /// Get a reference to the RecordFileWriter object which progress is being monitored.
  /// @return The RecordFileWriter used to write the output file.
  RecordFileWriter& getWriter();

  /// Set the range of timestamps expected, to track progress on the time range.
  /// @param minTimestamp: earliest timestamp of the operation
  /// @param maxTimestamp: latest timestamp of the operation
  void initTimeRange(double minTimestamp, double maxTimestamp);

  /// Called when a record is read, which can allow you to slow down decoding by adding a mere sleep
  /// in the callback itself. This is the main use case of this callback, as data is queued for
  /// processing & writing to Disk in a different thread, and we could run out of memory if we don't
  /// allow the background thread to run further, while we slow down the decoding thread.
  /// Can also be used to build a UI (GUI or terminal) to monitor progress when copying large files.
  /// @param timestamp: Timestamp of the record decoded.
  /// @param writeGraceWindow: Grace window in which records are allowed to be out-of-order.
  void onRecordDecoded(double timestamp, double writeGraceWindow = 0.0);

  /// Called when we're ready to close the file. On exit, it is expected that the writer is closed.
  int closeFile();

  void waitForBackgroundThreadQueueSize(size_t maxSize);

  void printPercentAndQueueSize(uint64_t queueByteSize, bool waiting);

  void addWaitCondition(const function<bool()>& waitCondition) {
    waitCondition_ = waitCondition;
  }

  bool showProgress() const {
    return copyOptions_.showProgress;
  }

  const CopyOptions& getCopyOptions() const {
    return copyOptions_;
  }

 private:
  RecordFileWriter writer_;
  function<bool()> waitCondition_;
  const CopyOptions& copyOptions_;
  double nextUpdateTime_ = 0;
  int32_t percent_ = 0;
  double minTimestamp_ = 0;
  double duration_ = 0;
};

/// Default handling of file creation & closing, offering customization opportunities
/// in particular when handling uploads.
class ThrottledFileDelegate {
 public:
  ThrottledFileDelegate() = default;
  explicit ThrottledFileDelegate(ThrottledWriter& throttledWriter) {
    // overrides not available in constructors & destructors
    ThrottledFileDelegate::init(throttledWriter);
  }
  virtual ~ThrottledFileDelegate() = default;
  virtual void init(ThrottledWriter& throttledWriter) {
    throttledWriter_ = &throttledWriter;
  }
  virtual bool shouldPreallocateIndex() const {
    return true;
  }
  virtual int createFile(const string& pathToCopy);
  virtual int closeFile();

 protected:
  ThrottledWriter* throttledWriter_{};
};

} // namespace vrs::utils
