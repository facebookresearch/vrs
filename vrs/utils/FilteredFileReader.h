// Copyright (c) Meta Platforms, Inc. and affiliates.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#pragma once

#include <functional>
#include <limits>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include <vrs/Record.h>
#include <vrs/RecordFileReader.h>
#include <vrs/RecordFileWriter.h>

#include <vrs/utils/ThrottleHelpers.h>

namespace vrs::utils {

struct FilteredFileReader;

using RecordReaderFunc = function<bool(RecordFileReader&, const IndexRecord::RecordInfo&)>;

struct DecimationParams {
  // Per stream decimation intervals
  vector<pair<string, double>> decimationIntervals;
  // Divide time where we have all records into intervals, 0 to disable bucketing
  double bucketInterval = 0.0;
  // Disregard records which timestamp is more than this delta away from the bucket's
  double bucketMaxTimestampDelta = 1.0 / 30.0;
};

/// Filters as specified using the command line, as a series of parameters, grouped by type
struct RecordFilterParams {
  vector<string> streamFilters;
  vector<string> typeFilters;
  unique_ptr<DecimationParams> decimationParams;

  // Add contraints, typically from command line options
  bool includeStream(const string& numericName);
  bool excludeStream(const string& numericName);
  bool includeType(const string& type);
  bool excludeType(const string& type);
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

  // Resolve relative time constraints based on the given start & end timestamps
  bool resolveTimeConstraints(double startTimestamp, double endTimestamp);
  string getTimeConstraintDescription() const;
};

/// Class handling stream interval & bucket decimation
class Decimator {
 public:
  Decimator(FilteredFileReader& filteredReader, DecimationParams& params);

  // chance to reset internal state before each iteration
  void reset();
  // tell if a record should be read decimated (return true)
  bool decimate(
      RecordReaderFunc& recordReaderFunc,
      ThrottledWriter* throttledWriter,
      const IndexRecord::RecordInfo& record,
      bool& inOutKeepGoing);
  // chance to process final records before the end of an iteration
  void flush(RecordReaderFunc& recordReaderFunc, ThrottledWriter* throttledWriter);

  double getGraceWindow() const;

 private:
  bool submitBucket(RecordReaderFunc& recordReaderFunc, ThrottledWriter* throttledWriter);

  FilteredFileReader& filteredReader_;
  // Timestamp intervals used to skip data records (does not apply to config and state records)
  map<StreamId, double> decimationIntervals_;
  // Divide time where we have all records into intervals, 0 to disable bucketing
  const double bucketInterval_;
  // Disregard records which timestamp is more than this delta away from the bucket's
  const double bucketMaxTimestampDelta_;
  // Grace time window to avoid unsorted records because of pending buckets
  const double graceWindow_;

  // iteration specific variables
  map<StreamId, double> decimateCursors_;
  // Timestamp of the current bucket we are creating
  double bucketCurrentTimestamp_;
  map<StreamId, const IndexRecord::RecordInfo*> bucketCandidates_;
};

/// Encapsulation of a VRS file to read, along with filters to only reads some records/streams.
struct FilteredFileReader {
  string path;
  RecordFileReader reader;
  RecordFilter filter;
  // custom filter: return true to skip record
  function<bool(const IndexRecord::RecordInfo&)> skipRecordFilter;
  // optional decimator
  unique_ptr<Decimator> decimator_;
  bool firstRecordsOnly = false;

  FilteredFileReader() = default;
  FilteredFileReader(const string& filePath, const unique_ptr<FileHandler>& vrsFileProvider = {}) {
    setSource(filePath, vrsFileProvider);
  }
  virtual ~FilteredFileReader() = default;

  void setSource(const string& filePath, const unique_ptr<FileHandler>& vrsFileProvider = {}) {
    path = filePath;
    if (vrsFileProvider) {
      reader.setFileHandler(vrsFileProvider->makeNew());
    }
  }

  virtual bool fileExists() const;
  virtual string getPathOrUri() const;
  virtual string getFileName();
  virtual int64_t getFileSize() const;

  virtual int openFile(const RecordFilterParams& filters = {});

  // Open the file, local or not, as a standard file
  virtual int openFile(unique_ptr<FileHandler>& file) const;

  string getCopyPath();

  // Add constraints, typically from command line options
  bool afterConstraint(const string& after);
  bool beforeConstraint(const string& before);
  bool includeStream(const string& numericName);
  bool excludeStream(const string& numericName);

  // Set time constraints, maybe relative to first/last data records
  void setMinTime(double minimumTime, bool relativeToBegin);
  void setMaxTime(double maximumTime, bool relativeToEnd);

  // Apply time constrains & get resulting range in one call.
  void getConstrainedTimeRange(double& outStartTimestamp, double& outEndTimestamp);

  // Get the time range including the data records of the considered streams only.
  // The file must be opened already.
  // The resulting values are used to convert file-relative timestamps into absolute timestamps.
  void getTimeRange(double& outStartTimestamp, double& outEndTimestamp);
  // Expand an existing timerange to include the data records of the considered streams only.
  void expandTimeRange(double& inOutStartTimestamp, double& inOutEndTimestamp);
  // Constrain the given time range to the current filter's time constraints
  void constrainTimeRange(double& inOutStartTimestamp, double& inOutEndTimestamp) const;

  // Apply filters, which can only be done after the file was opened already
  void applyFilters(const RecordFilterParams& filters);
  void applyRecordableFilters(const vector<string>& filter);
  void applyTypeFilters(const vector<string>& filter);

  // Time constraints can be relative to the begin/end timestamps.
  // If necessary, convert relative time constraints into relative constraints on the file.
  bool resolveTimeConstraints();
  string getTimeConstraintDescription();

  // Make an index of the filtered records. Useful to pre-allocate the index during copy operations.
  unique_ptr<deque<IndexRecord::DiskRecordInfo>> buildIndex();

  // Make sure the lastest config & state records are read before reading.
  // Needed when we don't read from the start
  // This version reads the records
  void preRollConfigAndState();
  // Make sure the lastest config & state records are read before reading.
  // Needed when we don't read from the start
  // This version hands the records to the function provided
  void preRollConfigAndState(RecordReaderFunc recordReaderFunc);

  // Read all the records of the reader than meet the specs
  // Use a ThrottledWriter object to get a callback after each record is decoded
  uint32_t iterate(ThrottledWriter* throttledWriter = nullptr);

  // Iterate and call the provided function for each record
  // Use a ThrottledWriter object to get a callback after each record is decoded
  void iterate(RecordReaderFunc recordReaderFunc, ThrottledWriter* throttledWriter = nullptr);
};

}; // namespace vrs::utils
