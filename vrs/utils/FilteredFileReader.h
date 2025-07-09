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

#include <limits>
#include <string_view>

#include <vrs/Record.h>
#include <vrs/RecordFileReader.h>
#include <vrs/RecordFileWriter.h>

#include "ThrottleHelpers.h"

namespace vrs::utils {

struct FilteredFileReader;

using RecordReaderFunc = function<bool(RecordFileReader&, const IndexRecord::RecordInfo&)>;

/// Filters as specified using the command line, as a series of parameters, grouped by type
struct RecordFilterParams {
  vector<string> streamFilters;
  vector<string> typeFilters;

  // Add constraints, typically from command line options
  bool includeStream(const string& streamFilter);
  bool excludeStream(const string& streamFilter);
  // Same as above, but assumes streamFilter starts with '+' to add,
  // or '-', or '~'to remove streams. '~' is useful for CLI tools use cases.
  bool includeExcludeStream(const string& plusMinusStreamFilter);
  bool includeType(const string& type);
  bool excludeType(const string& type);

  void getIncludedStreams(RecordFileReader& reader, set<StreamId>& outFilteredSet) const;
  unique_ptr<set<StreamId>> getIncludedStreams(RecordFileReader& reader) const; // if nullptr, all
  string getStreamFiltersConfiguration(std::string_view configName) const;
};

/// Class to filter out some parts of a VRS file when reading it.
/// This class merely holds some constraints.
struct RecordFilter {
  set<StreamId> streams;
  set<Record::Type> types;
  bool relativeMinTime = false;
  bool relativeMaxTime = false;
  bool aroundTime = false;
  double minTime = std::numeric_limits<double>::lowest();
  double maxTime = std::numeric_limits<double>::max();

  // Add time constraints, typically from command line options,
  // with interpretation of an eventual sign character as relative to the file's begin/end
  bool afterConstraint(const string& after);
  bool beforeConstraint(const string& before);
  void setMinTime(double minimumTime, bool relativeToBegin);
  void setMaxTime(double maximumTime, bool relativeToEnd);

  // Copy time constraints from another filter
  void copyTimeConstraints(const RecordFilter& sourceFilter);

  // Resolve relative time constraints based on the given start & end timestamps
  bool resolveRelativeTimeConstraints(double startTimestamp, double endTimestamp);
  string getTimeConstraintDescription() const;

  // Make sure time constraints have been applied and the resulting time range makes sense
  bool timeRangeValid() const;
};

class Decimator {
 public:
  virtual ~Decimator() = default;

  // chance to reset internal state before each iteration
  virtual void reset() = 0;
  // tell if a record should be read decimated (return true)
  virtual bool decimate(
      RecordReaderFunc& recordReaderFunc,
      ThrottledWriter* throttledWriter,
      const IndexRecord::RecordInfo& record,
      bool& inOutKeepGoing) = 0;
  // chance to process final records before the end of an iteration
  virtual void flush(RecordReaderFunc& recordReaderFunc, ThrottledWriter* throttledWriter) = 0;

  virtual double getGraceWindow() const = 0;
};

/// Encapsulation of a VRS file to read, along with filters to only reads some records/streams.
struct FilteredFileReader {
  FileSpec spec;
  RecordFileReader reader;
  RecordFilter filter;
  // custom filter: return true to skip record
  function<bool(const IndexRecord::RecordInfo&)> skipRecordFilter;
  // optional decimator
  unique_ptr<Decimator> decimator_;
  bool firstRecordsOnly = false;

  FilteredFileReader() = default;
  explicit FilteredFileReader(
      const string& filePath,
      const unique_ptr<FileHandler>& vrsFileProvider = {}) {
    FilteredFileReader::setSource(filePath, vrsFileProvider);
  }
  virtual ~FilteredFileReader() = default;

  virtual int setSource(const string& filePath, const unique_ptr<FileHandler>& fileHandler = {});

  virtual bool fileExists() const;
  virtual string getPathOrUri() const;
  virtual string getFileName();
  virtual int64_t getFileSize() const;

  virtual int openFile(const RecordFilterParams& filters = {});

  string getCopyPath();

  // Add constraints, typically from command line options
  bool afterConstraint(const string& after);
  bool beforeConstraint(const string& before);

  // Set time constraints, maybe relative to first/last data records
  void setMinTime(double minimumTime, bool relativeToBegin);
  void setMaxTime(double maximumTime, bool relativeToEnd);

  // Apply time constrains & get resulting range in one call.
  // This should be called for proper time range iterations.
  void getConstrainedTimeRange(double& outStartTimestamp, double& outEndTimestamp);

  // Get the time range including the data records of the filtered streams only.
  // The file must be opened already.
  // The resulting values are used to convert file-relative timestamps into absolute timestamps.
  void getTimeRange(double& outStartTimestamp, double& outEndTimestamp) const;
  // Expand an existing timerange to include the data records of the considered streams only.
  void expandTimeRange(double& inOutStartTimestamp, double& inOutEndTimestamp) const;
  // Constrain the given time range to the current filter's time constraints
  void constrainTimeRange(double& inOutStartTimestamp, double& inOutEndTimestamp) const;

  // Apply filters, which can only be done after the file was opened already
  void applyFilters(const RecordFilterParams& filters);
  void applyRecordableFilters(const vector<string>& filter);
  void applyTypeFilters(const vector<string>& filter);

  // Validate that relative time constraints (if any) have been applied and the result is valid.
  bool timeRangeValid() const;
  string getTimeConstraintDescription();

  // Make an index of the filtered records. Useful to pre-allocate the index during copy operations.
  unique_ptr<deque<IndexRecord::DiskRecordInfo>> buildIndex();

  // Make sure the latest config & state records are read before reading.
  // Needed when we don't read from the start
  // This version reads the records
  void preRollConfigAndState();
  // Make sure the latest config & state records are read before reading.
  // Needed when we don't read from the start
  // This version hands the records to the function provided
  void preRollConfigAndState(const RecordReaderFunc& recordReaderFunc);

  // ** Preferred iteration method for code that doesn't require expert internal knowledge **
  // Determine the time range boundaries based on the file and the filters,
  // pre-roll config and state records as required, then iterates over records.
  uint32_t iterateSafe();

  // Read all the records of the reader than meet the specs, assuming time range is already valid.
  // Use a ThrottledWriter object to get a callback after each record is decoded
  uint32_t iterateAdvanced(ThrottledWriter* throttledWriter = nullptr);

  // Iterate and call the provided function for each record
  // Use a ThrottledWriter object to get a callback after each record is decoded
  void iterateAdvanced(
      RecordReaderFunc recordReaderFunc,
      ThrottledWriter* throttledWriter = nullptr);
};

}; // namespace vrs::utils
