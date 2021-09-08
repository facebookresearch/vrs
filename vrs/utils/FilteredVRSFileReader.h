// Facebook Technologies, LLC Proprietary and Confidential.

#pragma once

#include <cfloat>

#include <limits.h>
#include <functional>
#include <iostream>
#include <string>
#include <vector>

#include <vrs/Record.h>
#include <vrs/RecordFileReader.h>
#include <vrs/RecordFileWriter.h>
#include <vrs/gaia/GaiaClient.h>

namespace vrs::utils {

struct CopyOptions;

/// Filters as specified using the command line, as a series of parameters, grouped by type
struct RecordFilterParams {
  std::vector<std::string> streamFilters;
  std::vector<std::string> typeFilters;
  std::vector<std::pair<std::string, double>> decimateIntervals;

  // Add contraints, typically from command line options
  bool includeStream(const string& numericName);
  bool excludeStream(const string& numericName);
  bool includeType(const string& type);
  bool excludeType(const string& type);
};

/// Class to filter out some parts of a VRS file when reading it.
/// This class merely holds some constraints.
struct RecordFilter {
  std::set<vrs::StreamId> streams;
  std::set<vrs::Record::Type> types;
  bool relativeMinTime = false;
  bool relativeMaxTime = false;
  bool aroundTime = false;
  double minTime = std::numeric_limits<double>::lowest();
  double maxTime = std::numeric_limits<double>::max();

  // Timestamp intervals used to skip data records (does not apply to config and state records)
  std::map<vrs::StreamId, double> decimateIntervals;

  // Divide time where we have all records into intervals, 0 disables bucketing
  double bucketInterval = 0.0;
  // Disregard frames where timestamp is more than this delta away from the bucket's
  double bucketMaxTimestampDelta = 1.0 / 30.0;

  // Add time constraints, typically from command line options,
  // with interpretation of an eventual sign character as relative to the file's begin/end
  bool afterConstraint(const string& after);
  bool beforeConstraint(const string& before);
  void setMinTime(double minimumTime, bool relativeToBegin);
  void setMaxTime(double maximumTime, bool relativeToEnd);

  // Resolve relative time constraints based on the given start & end timestamps
  bool resolveTimeConstraints(double startTimestamp, double endTimestamp);
  std::string getTimeConstraintDescription() const;
};

struct TagOverrides {
  std::map<std::string, std::string> fileTags;
  std::map<StreamId, std::map<std::string, std::string>> streamTags;

  void overrideTags(vrs::RecordFileWriter& writer);
};

class ThrottledWriter;

/// Class to encapsulate a VRS file to read, along with filters to only reads some records/streams.
struct FilteredVRSFileReader {
  std::string path;
  vrs::RecordFileReader reader;
  RecordFilter filter;
  // custom filter: return true to skip record
  std::function<bool(const IndexRecord::RecordInfo&)> skipRecordFilter;
  bool firstRecordsOnly = false;
  bool isUsingGaiaId = false;
  int gaiaLookupReturnCode = 0;
  std::unique_ptr<GaiaClient> gaiaClient;
  TagOverrides tagOverrides;

  FilteredVRSFileReader() {}
  FilteredVRSFileReader(
      const std::string& filePath,
      const std::unique_ptr<vrs::FileHandler>& vrsFileProvider = {}) {
    setSource(filePath, vrsFileProvider);
  }

  FilteredVRSFileReader(GaiaIdFileVersion idv) {
    setGaiaSource(idv);
  }
  FilteredVRSFileReader(GaiaId gaiaId, int version) {
    setGaiaSource(GaiaIdFileVersion(gaiaId, version));
  }

  void setSource(
      const std::string& filePath,
      const std::unique_ptr<vrs::FileHandler>& vrsFileProvider = {}) {
    path = filePath;
    if (vrsFileProvider) {
      reader.setFileHandler(vrsFileProvider->makeNew());
    }
  }

  int setGaiaSource(GaiaIdFileVersion idv);
  int setGaiaSource(GaiaId gaiaId, int version) {
    return setGaiaSource(GaiaIdFileVersion(gaiaId, version));
  }
  void clearGaiaSourceCachedLookup() const;

  bool fileExists() const;
  std::string getPathOrUri() const;
  GaiaId getGaiaId() const;
  int getFileVersion() const;
  GaiaIdFileVersion getGaiaIdFileVersion() const;
  string getFileName();
  int64_t getFileSize() const;

  int openFile(const RecordFilterParams& filters = {});

  // Open the file, local or not, as a standard file
  int openFile(std::unique_ptr<FileHandler>& file) const;

  std::string getCopyPath();

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
  void applyRecordableFilters(const std::vector<std::string>& filter);
  void applyTypeFilters(const std::vector<std::string>& filter);
  void applyDecimateIntervals(const std::vector<std::pair<std::string, double>>& intervals);

  // Time constraints can be relative to the begin/end timestamps.
  // If necessary, convert relative time constraints into relative constraints on the file.
  bool resolveTimeConstraints();
  std::string getTimeConstraintDescription();

  // Make an index of the filtered records. Useful to pre-allocate the index during copy operations.
  unique_ptr<deque<IndexRecord::DiskRecordInfo>> buildIndex();

  // Make sure the lastest config & state records are read before reading.
  // Needed when we don't read from the start
  // This version reads the records
  void preRollConfigAndState();
  // Make sure the lastest config & state records are read before reading.
  // Needed when we don't read from the start
  // This version hands the records to the function provided
  void preRollConfigAndState(
      std::function<void(vrs::RecordFileReader&, const vrs::IndexRecord::RecordInfo&)> f);

  // Read all the records of the reader than meet the specs
  // Use a ThrottledWriter object to get a callback after each record is decoded
  uint32_t iterate(ThrottledWriter* throttledWriter = nullptr);

  // Iterate and call the provided function for each record
  // Use a ThrottledWriter object to get a callback after each record is decoded
  void iterate(
      std::function<bool(vrs::RecordFileReader&, const vrs::IndexRecord::RecordInfo&)> f,
      ThrottledWriter* throttledWriter = nullptr);
};

}; // namespace vrs::utils
