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

#include <cassert>
#include <utility>

#define DEFAULT_LOG_CHANNEL "FilterCopySamples"
#include <logging/Log.h>

#include <vrs/DataLayoutConventions.h>
#include <vrs/RecordFileReader.h>
#include <vrs/helpers/FileMacros.h>
#include <vrs/os/Utils.h>
#include <vrs/utils/FilterCopy.h>

using namespace std;
using namespace vrs;
using namespace vrs::utils;

namespace {

class CalibrationLayout : public AutoDataLayout {
 public:
  DataPieceString factoryCalibration{"factory_calibration"};
  AutoDataLayoutEnd endLayout;
};

class CalibrationPatcher : public RecordFilterCopier {
 public:
  CalibrationPatcher(
      vrs::RecordFileReader& fileReader,
      vrs::RecordFileWriter& fileWriter,
      vrs::StreamId id,
      const CopyOptions& copyOptions,
      string calibration)
      : RecordFilterCopier(fileReader, fileWriter, id, copyOptions),
        calibration_{std::move(calibration)} {}
  bool shouldCopyVerbatim(const CurrentRecord& record) override {
    return record.recordType != Record::Type::CONFIGURATION;
  }
  void doDataLayoutEdits(const CurrentRecord&, size_t block, DataLayout& datalayout) override {
    // this code is both efficient & safe
    const auto& calibration = getExpectedLayout<CalibrationLayout>(datalayout, block);
    calibration.factoryCalibration.patchValue(calibration_);
  }

 private:
  string calibration_;
};

unique_ptr<StreamPlayer> makeCalibrationPatcherFilter(
    RecordFileReader& fileReader,
    RecordFileWriter& fileWriter,
    StreamId streamId,
    const CopyOptions& copyOptions) {
  if (fileReader.mightContainImages(streamId)) {
    return make_unique<CalibrationPatcher>(
        fileReader, fileWriter, streamId, copyOptions, streamId.getTypeName());
  } else {
    return make_unique<Copier>(fileReader, fileWriter, streamId, copyOptions);
  }
}

} // namespace

// Sample function that copies a file, and patches the factory calibration
int calibrationPatcher(const string& sourceFile, const string& outputFile) {
  CopyOptions options(false);

  FilteredFileReader filteredReader(sourceFile);
  IF_ERROR_LOG_AND_RETURN(filteredReader.openFile());

  return filterCopy(filteredReader, outputFile, options, makeCalibrationPatcherFilter);
}

namespace {

using namespace vrs::datalayout_conventions;

// Image filter that demonstrates changing both the image spec & the image data
// This filter simply drops the lower half of the image
class HalfHeightFilter : public RecordFilterCopier {
 public:
  HalfHeightFilter(
      vrs::RecordFileReader& fileReader,
      vrs::RecordFileWriter& fileWriter,
      vrs::StreamId id,
      const CopyOptions& copyOptions)
      : RecordFilterCopier(fileReader, fileWriter, id, copyOptions) {}
  bool shouldCopyVerbatim(const CurrentRecord& record) override {
    return false;
  }
  void doDataLayoutEdits(const CurrentRecord& record, size_t blockIndex, DataLayout& dl) override {
    if (record.recordType == Record::Type::CONFIGURATION) {
      const auto& newSpec = getExpectedLayout<ImageSpec>(dl, blockIndex);
      assert(newSpec.height.isMapped());
      newSpec.height.patchValue(newSpec.height.get() / 2);
    }
  }
  void filterImage(
      const CurrentRecord& record,
      size_t blockIndex,
      const ContentBlock& imageBlock,
      vector<uint8_t>& pixels) override {
    pixels.resize(pixels.size() / 2);
  }
};

unique_ptr<StreamPlayer> makeImageResizeFilter(
    RecordFileReader& fileReader,
    RecordFileWriter& fileWriter,
    StreamId streamId,
    const CopyOptions& copyOptions) {
  if (fileReader.mightContainImages(streamId)) {
    return make_unique<HalfHeightFilter>(fileReader, fileWriter, streamId, copyOptions);
  } else {
    return make_unique<Copier>(fileReader, fileWriter, streamId, copyOptions);
  }
}

} // namespace

// Sample function that copies a file, and reduces the image height by half
int halfHeightImageFilter(const string& sourceFile, const string& outputFile) {
  CopyOptions options(false);

  FilteredFileReader filteredReader(sourceFile);
  IF_ERROR_LOG_AND_RETURN(filteredReader.openFile());

  return filterCopy(filteredReader, outputFile, options, makeImageResizeFilter);
}

namespace {

// Filter that adds 1 to every timestamp in the header
class TimestampIncrementFilter : public RecordFilterCopier {
 public:
  TimestampIncrementFilter(
      vrs::RecordFileReader& fileReader,
      vrs::RecordFileWriter& fileWriter,
      vrs::StreamId id,
      const CopyOptions& copyOptions)
      : RecordFilterCopier(fileReader, fileWriter, id, copyOptions) {}

  bool shouldCopyVerbatim(const CurrentRecord& record) override {
    return false;
  }
  void doHeaderEdits(CurrentRecord& record) override {
    record.timestamp += 1;
  }
};

unique_ptr<StreamPlayer> makeTimestampIncrementFilter(
    RecordFileReader& fileReader,
    RecordFileWriter& fileWriter,
    StreamId streamId,
    const CopyOptions& copyOptions) {
  return make_unique<TimestampIncrementFilter>(fileReader, fileWriter, streamId, copyOptions);
}

} // namespace

// Sample function that copies a file, and increments every timestamp by 1
int incrementTimestampFilter(const string& sourceFile, const string& outputFile) {
  CopyOptions options(false);

  FilteredFileReader filteredReader(sourceFile);
  IF_ERROR_LOG_AND_RETURN(filteredReader.openFile());

  return filterCopy(filteredReader, outputFile, options, makeTimestampIncrementFilter);
}
