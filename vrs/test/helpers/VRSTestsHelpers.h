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
#include <utility>

#include <fmt/format.h>

#include <vrs/DataLayout.h>
#include <vrs/DataPieces.h>
#include <vrs/DiskFile.h>
#include <vrs/RecordFileReader.h>
#include <vrs/RecordFileWriter.h>
#include <vrs/Recordable.h>

namespace vrs {
namespace test {

using namespace vrs;

void deleteChunkedFile(const string& path);
void deleteChunkedFile(DiskFile& file);

// backdoor operations for testing
struct RecordFileWriterTester {
  static void skipFinalizeIndexRecord(RecordFileWriter& file);
  static uint64_t addRecordBatchesToSortedRecords(
      const RecordFileWriter::RecordBatches& batch,
      RecordFileWriter::SortedRecords& inOutSortedRecords) {
    return RecordFileWriter::addRecordBatchesToSortedRecords(batch, inOutSortedRecords);
  }
  static int64_t getCurrentFileSize(RecordFileWriter& file) {
    return file.indexRecordWriter_.getSplitHead()
        ? file.indexRecordWriter_.getSplitHead()->getPos() + file.file_->getPos()
        : file.file_->getPos();
  }
};

// Shared infra to create reference VRS files used in different places

enum TestOptions { REALTIME = 1 << 0, SPLIT_HEADER = 1 << 1, SKIP_FINALIZE_INDEX = 1 << 2 };

constexpr double kPrerollTime = 0.5;

constexpr uint32_t kFrameWidth = 320;
constexpr uint32_t kFrameHeight = 240;
constexpr uint32_t kFrameSaveFrequency = 15; // save every X frames
constexpr uint32_t kCameraCount = 4;

constexpr const char* kTopLeftCameraFlavor = "tech/unit_test/top_left";
constexpr const char* kTopRightCameraFlavor = "tech/unit_test/top_right";
constexpr const char* kBottomLeftCameraFlavor = "tech/unit_test/bottom_left";
constexpr const char* kBottomRightCameraFlavor = "tech/unit_test/bottom_right";

constexpr const char* kCameraFlavor[kCameraCount] = {
    kTopLeftCameraFlavor,
    kTopRightCameraFlavor,
    kBottomLeftCameraFlavor,
    kBottomRightCameraFlavor};

constexpr uint32_t kStateVersion = 1;
constexpr uint32_t kConfigurationVersion = 1;
constexpr uint32_t kDataVersion = 1;

struct FileConfig {
  FileConfig(uint32_t _frameRate, uint32_t _simulationDurationMs) {
    frameRate = _frameRate;
    simulationDurationMs = _simulationDurationMs;
    frameCount = simulationDurationMs * frameRate / 1000;
    totalRecordCount = kCameraCount * (2 + frameCount);
  }
  uint32_t frameRate;
  uint32_t simulationDurationMs;
  size_t frameCount;
  size_t totalRecordCount;
};

const FileConfig& getClassicFileConfig();
const FileConfig& getLongFileConfig();
const FileConfig& getVeryLongFileConfig();

#define kClassicFileConfig test::getClassicFileConfig()
#define kLongFileConfig test::getLongFileConfig()
#define kVeryLongFileConfig test::getVeryLongFileConfig()

/// Parameterization of the VRS file creation, so we can simulate a wide variety of cases.
struct CreateParams {
  using CustomCreateFileFunction =
      function<int(CreateParams& createParams, RecordFileWriter& fileWriter)>;

  explicit CreateParams(string _path, const FileConfig& _fileConfig = kClassicFileConfig)
      : path{std::move(_path)}, fileConfig{_fileConfig} {}

  // Required params
  string path;
  const FileConfig& fileConfig;

  // Setters for readability
  CreateParams& setPreallocateIndexSize(size_t indexSize) {
    preallocateIndexSize = indexSize;
    return *this;
  }
  CreateParams& setTestOptions(int options) {
    testOptions = options;
    return *this;
  }
  CreateParams& setMaxChunkSizeMB(size_t chunkSizeMB) {
    maxChunkSizeMB = chunkSizeMB;
    return *this;
  }
  CreateParams& setFileWriterThreadCount(size_t _fileWriterThreadCount = UINT32_MAX) {
    fileWriterThreadCount = _fileWriterThreadCount;
    return *this;
  }
  CreateParams& setChunkHandler(unique_ptr<NewChunkHandler>&& handler) {
    chunkHandler = std::move(handler);
    return *this;
  }
  CreateParams& setCustomCreateFileFunction(
      const CustomCreateFileFunction& _customCreateFileFunction) {
    customCreateFileFunction = _customCreateFileFunction;
    return *this;
  }
  CreateParams& useAsyncDiskFile(const char* asyncOptions = "") {
    path = "asyncdiskfile:" + path;
    if (asyncOptions && *asyncOptions != 0) {
      path = path + "?" + asyncOptions;
    }
    return *this;
  }

  static string getCameraStreamTag(uint32_t cameraIndex) {
    return fmt::format("camera_{}", cameraIndex);
  }

  // More params with "neutral" default
  size_t preallocateIndexSize = 0;
  int testOptions = TestOptions::REALTIME;
  size_t maxChunkSizeMB = 0;
  size_t fileWriterThreadCount = 0; // 0 is the default value
  unique_ptr<NewChunkHandler> chunkHandler;
  CustomCreateFileFunction customCreateFileFunction;

  // Size of the file after it was created, but before any record was written.
  // Any file shorter than this is not salvageable, because the description record isn't complete.
  // This value is set on exit, so we know how much we can truncate the file without breaking.
  int64_t outMinFileSize = -1;
};

/// Both functions create the same VRS, except that the former uses multiple threads,
/// while the later creates them all in the calling thread.
int threadedCreateRecords(CreateParams& createParams);
int singleThreadCreateRecords(CreateParams& createParams);

/// Parameterization of the checks to be performed, so we can verify a wide variety of situations.
struct CheckParams {
  explicit CheckParams(const string& _path, const FileConfig& _fileConfig = kClassicFileConfig)
      : filePath{_path}, fileConfig{_fileConfig} {}

  // Required params
  const string& filePath;
  const FileConfig& fileConfig;

  CheckParams& setTruncatedUserRecords(size_t truncatedRecords) {
    truncatedUserRecords = truncatedRecords;
    return *this;
  }
  CheckParams& setHasIndex(size_t _hasIndex) {
    hasIndex = _hasIndex;
    return *this;
  }
  CheckParams& setJumpbackCount(size_t jumpbacks) {
    jumpbackCount = jumpbacks;
    return *this;
  }
  CheckParams& setJumpbackAfterFixingIndex(size_t jumpbacks) {
    jumpbackCountAfterFixingIndex = jumpbacks;
    return *this;
  }

  // Optional params with default
  size_t truncatedUserRecords = 0;
  bool hasIndex = true;
  size_t jumpbackCount = 0;
  size_t jumpbackCountAfterFixingIndex = 0;
};

void checkRecordCountAndIndex(const CheckParams& checkParams);

} // namespace test
} // namespace vrs
