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

#include "ThrottleHelpers.h"

#include <iomanip>

#include <vrs/helpers/Strings.h>
#include <vrs/os/Time.h>

namespace {

#if IS_LINUX_PLATFORM() || IS_MAC_PLATFORM() || IS_WINDOWS_PLATFORM()
// Be more generous on memory usage with desktop platforms
const size_t kMaxQueueByteSize = 600 * 1024 * 1024ULL; // 600 MB
#else
// Mobile environments are memory constrained, and might kill a memory greedy app
const size_t kMaxQueueByteSize = 400 * 1024 * 1024; // 400 MB
#endif
const size_t kReadAgainQueueByteSize = kMaxQueueByteSize * 9 / 10; // 90%
const size_t kLowQueueByteSize = 40 * 1024 * 1024ULL;

const double kRefreshDelaySec = 1. / 3; // limit how frequently we show updates

} // namespace

namespace vrs::utils {

using namespace std;
using namespace vrs;

const size_t kDownloadChunkSize = 1024 * 1024 * 4;

#if IS_WINDOWS_PLATFORM()
// "\r" works, but escape sequences don't by default: overwrite with white spaces... :-(
const char* const kResetCurrentLine = "\r                                            \r";
#else
const char* const kResetCurrentLine = "\r\33[2K\r";
#endif

ThrottledWriter::ThrottledWriter(const CopyOptions& options) : copyOptions_{options} {
  writer_.trackBackgroundThreadQueueByteSize();
  initWriter();
  nextUpdateTime_ = 0;
}

ThrottledWriter::ThrottledWriter(const CopyOptions& options, ThrottledFileDelegate& fileDelegate)
    : ThrottledWriter(options) {
  fileDelegate.init(*this);
}

void ThrottledWriter::initWriter() {
  writer_.setCompressionThreadPoolSize(
      min<size_t>(copyOptions_.compressionPoolSize, thread::hardware_concurrency()));
  writer_.setMaxChunkSizeMB(copyOptions_.maxChunkSizeMB);
}

RecordFileWriter& ThrottledWriter::getWriter() {
  return writer_;
}

void ThrottledWriter::initTimeRange(double minTimestamp, double maxTimestamp) {
  minTimestamp_ = minTimestamp;
  duration_ = maxTimestamp - minTimestamp;
}

void ThrottledWriter::onRecordDecoded(double timestamp, double writeGraceWindow) {
  uint64_t queueByteSize = writer_.getBackgroundThreadQueueByteSize();
  const uint32_t writeInterval = copyOptions_.outRecordCopiedCount < 100 ? 10 : 100;
  if (queueByteSize == 0 || copyOptions_.outRecordCopiedCount % writeInterval == 0) {
    writer_.writeRecordsAsync(timestamp - max<double>(writeGraceWindow, copyOptions_.graceWindow));
  }
  // don't go crazy with memory usage, if we read data much faster than we can process it...
  if (queueByteSize > kMaxQueueByteSize || (waitCondition_ && waitCondition_())) {
    writer_.writeRecordsAsync(timestamp - max<double>(writeGraceWindow, copyOptions_.graceWindow));
    // wait that most of the buffers are processed to resume,
    // limiting collisions between input & output file operations.
    do {
      printPercentAndQueueSize(queueByteSize, true);
      this_thread::sleep_for(chrono::duration<double>(kRefreshDelaySec));
      queueByteSize = writer_.getBackgroundThreadQueueByteSize();
    } while (queueByteSize > kReadAgainQueueByteSize || (waitCondition_ && waitCondition_()));
    if (showProgress()) {
      cout << kResetCurrentLine;
      nextUpdateTime_ = 0;
    }
  }
  if (showProgress()) {
    double now = os::getTimestampSec();
    if (now >= nextUpdateTime_) {
      double progress = duration_ > 0.0001 ? (timestamp - minTimestamp_) / duration_ : 1.;
      // timestamp ranges only include data records, but config & state records might be beyond
      percent_ = max<int32_t>(static_cast<int32_t>(progress * 100), 0);
      percent_ = min<int32_t>(percent_, 100);
      printPercentAndQueueSize(writer_.getBackgroundThreadQueueByteSize(), false);
      nextUpdateTime_ = now + kRefreshDelaySec;
    }
  }
}

void ThrottledWriter::printPercentAndQueueSize(uint64_t queueByteSize, bool waiting) {
  if (showProgress()) {
    if (writer_.isWriting()) {
      cout << kResetCurrentLine << (waiting ? "Waiting " : "Reading ") << setw(2) << percent_
           << "%, processing " << setw(7) << helpers::humanReadableFileSize(queueByteSize);
    } else {
      cout << kResetCurrentLine << "Reading " << setw(2) << percent_ << "%";
    }
    cout.flush();
  }
}

int ThrottledWriter::closeFile() {
  if (showProgress()) {
    writer_.closeFileAsync(); // non-blocking
    waitForBackgroundThreadQueueSize(kLowQueueByteSize / 3);
  }
  int copyResult = writer_.waitForFileClosed(); // blocking call
  if (showProgress()) {
    cout << kResetCurrentLine;
  }
  return copyResult;
}

void ThrottledWriter::waitForBackgroundThreadQueueSize(size_t maxSize) {
  if (showProgress()) {
    cout << kResetCurrentLine;
  }
  // To avoid stalls, don't wait quite until we have nothing left to process,
  uint64_t queueByteSize = 0;
  while ((queueByteSize = writer_.getBackgroundThreadQueueByteSize()) > maxSize) {
    if (showProgress()) {
      cout << kResetCurrentLine << "Processing " << setw(7)
           << helpers::humanReadableFileSize(queueByteSize);
      cout.flush();
    }
    // Check more frequently when we're getting close. This is Science.
    const double sleepDuration = queueByteSize > 3 * kLowQueueByteSize ? kRefreshDelaySec
        : queueByteSize > kLowQueueByteSize                            ? kRefreshDelaySec / 2
                                                                       : kRefreshDelaySec / 5;
    this_thread::sleep_for(chrono::duration<double>(sleepDuration));
  }
  if (showProgress()) {
    cout << kResetCurrentLine << "Finishing...";
    cout.flush();
  }
}

int ThrottledFileDelegate::createFile(const string& pathToCopy) {
  return throttledWriter_->getWriter().createFileAsync(pathToCopy);
}

int ThrottledFileDelegate::closeFile() {
  return throttledWriter_->closeFile();
}

} // namespace vrs::utils
