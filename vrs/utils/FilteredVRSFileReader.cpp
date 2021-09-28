// Copyright (c) Facebook, Inc. and its affiliates. Confidential and proprietary.

#include "FilteredVRSFileReader.h"

#include <cmath>

#include <algorithm>
#include <iomanip>
#include <iostream>
#include <sstream>

#if defined(_MSC_VER)
#include <BaseTsd.h>
typedef SSIZE_T ssize_t;
#endif

#include <vrs/ErrorCode.h>
#include <vrs/helpers/Strings.h>
#include <vrs/os/Utils.h>

#include "CopyHelpers.h"

namespace vrs::utils {

using namespace std;
using namespace vrs;

#define LOG_ERROR(x)                                                                              \
  do {                                                                                            \
    int error = x;                                                                                \
    if (error) {                                                                                  \
      cerr << "Error while doing '" << #x << "': " << error << ", " << errorCodeToMessage(error); \
    }                                                                                             \
  } while (false)

// string ids: a text description of one or more streams.
// const set<StreamId>& fileStreams: set of known streams
// set<StreamId>& outStreamIds: set of stream ids extracted
// returns true if no parsing error occurred.
// Supported forms for ids:
//   N-M  where N is a recordable type id as a number, and M an instance id (also a number)
//   N-   where N is a recordable type id as a number.
//        Returns all the streams in fileStreams with that recordable type id
//   N    Same as N-
// Actual examples: 1004-1 or 1005- or 1005
static bool
stringToIds(const string& ids, const set<StreamId>& fileStreams, set<StreamId>& outStreamIds) {
  stringstream ss(ids);
  int typeIdNum;
  bool error = false;
  if (ss >> typeIdNum) {
    RecordableTypeId typeId = static_cast<RecordableTypeId>(typeIdNum);
    bool singleRecordable = false;
    if (ss.peek() == '-') {
      ss.ignore();
      uint16_t instanceId;
      if (ss >> instanceId) {
        outStreamIds.insert({typeId, instanceId});
        singleRecordable = true;
      } else {
        string rest;
        if (ss >> rest && !rest.empty()) {
          error = true;
        }
      }
    } else if (!ss.eof()) {
      error = true;
    }
    if (!error && !singleRecordable) {
      for (const auto& id : fileStreams) {
        if (id.getTypeId() == typeId) {
          outStreamIds.insert(id);
        }
      }
    }
  }
  if (error) {
    cerr << "Can't parse '" << ids << "' as one or more stream id." << endl;
  }
  return !error;
}

static Record::Type stringToType(const string& type) {
  if (helpers::startsWith("configuration", type)) {
    return Record::Type::CONFIGURATION;
  }
  if (helpers::startsWith("state", type)) {
    return Record::Type::STATE;
  }
  if (helpers::startsWith("data", type)) {
    return Record::Type::DATA;
  }
  cerr << "Can't parse '" << type << "' as a record type." << endl;
  return Record::Type::UNDEFINED;
}

static inline bool isSigned(const string& str) {
  return !str.empty() && (str.front() == '+' || str.front() == '-');
}

static bool isValidNumericName(const string& numericName) {
  if (StreamId::fromNumericName(numericName).isValid()) {
    return true;
  }
  try {
    unsigned long id = stoul(numericName);
    return id > 0 && id < 0xffff;
  } catch (logic_error&) {
    return false;
  }
}

bool RecordFilterParams::includeStream(const string& numericName) {
  if (!isValidNumericName(numericName)) {
    return false;
  }
  streamFilters.emplace_back("+");
  streamFilters.emplace_back(numericName);
  return true;
}

bool RecordFilterParams::excludeStream(const string& numericName) {
  if (!isValidNumericName(numericName)) {
    return false;
  }
  streamFilters.emplace_back("-");
  streamFilters.emplace_back(numericName);
  return true;
}

bool RecordFilterParams::includeType(const string& type) {
  if (stringToType(type) == Record::Type::UNDEFINED) {
    return false;
  }
  typeFilters.emplace_back("+");
  typeFilters.emplace_back(type);
  return true;
}

bool RecordFilterParams::excludeType(const string& type) {
  if (stringToType(type) == Record::Type::UNDEFINED) {
    return false;
  }
  typeFilters.emplace_back("-");
  typeFilters.emplace_back(type);
  return true;
}

int FilteredVRSFileReader::setGaiaSource(GaiaIdFileVersion idv) {
  isUsingGaiaId = true;
  path = to_string(idv.id);
  gaiaClient = GaiaClient::makeInstance();

  cout << "Looking up " << idv.toUri() << "... ";
  cout.flush();
  gaiaLookupReturnCode = gaiaClient->lookup(idv);
  if (gaiaLookupReturnCode == 0) {
    cout << "found version " << gaiaClient->getFileVersion() << "." << endl;
  } else {
    cout << endl;
    cerr << "Failed: " << gaiaLookupReturnCode << ", " << errorCodeToMessage(gaiaLookupReturnCode)
         << endl;
  }
  return gaiaLookupReturnCode;
}

void FilteredVRSFileReader::clearGaiaSourceCachedLookup() const {
  if (isUsingGaiaId && gaiaLookupReturnCode == 0) {
    gaiaClient->clearCachedLookup(getGaiaIdFileVersion());
  }
}

bool FilteredVRSFileReader::fileExists() const {
  if (isUsingGaiaId) {
    return gaiaLookupReturnCode == 0;
  }
  if (!path.empty() && path[0] == '{') {
    return true; // Assume json paths exist, to avoid breaking sanity checks
  }
  return os::pathExists(path);
}

string FilteredVRSFileReader::getPathOrUri() const {
  return isUsingGaiaId ? "gaia:" + path : path;
}

GaiaId FilteredVRSFileReader::getGaiaId() const {
  if (isUsingGaiaId) {
    try {
      return static_cast<uint64_t>(stoull(path));
    } catch (logic_error&) {
      // do nothing
    }
  }
  return 0;
}

int FilteredVRSFileReader::getFileVersion() const {
  return isUsingGaiaId && gaiaClient ? gaiaClient->getFileVersion() : 0;
}

GaiaIdFileVersion FilteredVRSFileReader::getGaiaIdFileVersion() const {
  return {getGaiaId(), getFileVersion()};
}

string FilteredVRSFileReader::getFileName() {
  if (isUsingGaiaId) {
    return gaiaLookupReturnCode != 0 ? "" : gaiaClient->getFileName();
  }
  if (path.empty()) {
    return {};
  }
  if (path.front() != '{') {
    return os::getFilename(path);
  }
  // if json filepath is used, we want to use either filename or first chunk.
  FileSpec spec;
  if (spec.fromJson(path)) {
    if (spec.fileName.empty() && !spec.chunks.empty()) {
      spec.fileName = os::getFilename(spec.chunks[0]);
    }
  }
  return spec.fileName;
}

int64_t FilteredVRSFileReader::getFileSize() const {
  if (isUsingGaiaId) {
    return gaiaLookupReturnCode != 0 ? -1 : gaiaClient->getFileSize();
  }
  return os::getFileSize(path);
}

int FilteredVRSFileReader::openFile(const RecordFilterParams& filters) {
  int status = 0;
  if (isUsingGaiaId) {
    status = gaiaLookupReturnCode != 0 ? gaiaLookupReturnCode : gaiaClient->open(reader);
  } else {
    status = path.length() == 0 ? ErrorCode::INVALID_REQUEST : reader.openFile(path);
  }
  if (status != 0) {
    return status;
  }
  applyFilters(filters);
  return status;
}

int FilteredVRSFileReader::openFile(unique_ptr<FileHandler>& file) const {
  int status = 0;
  if (isUsingGaiaId) {
    status = gaiaLookupReturnCode != 0 ? gaiaLookupReturnCode : gaiaClient->open(file);
  } else {
    file = make_unique<DiskFile>();
    status = file->open(path);
  }
  if (status != 0) {
    cerr << "Can't open '" << getPathOrUri() << "': " << errorCodeToMessage(status) << endl;
  }
  return status;
}

string FilteredVRSFileReader::getCopyPath() {
  // if uploading, but no temp file path has been provided, automatically generate one
  string fileName = getFileName();
  return os::getTempFolder() + (fileName.empty() ? "file.tmp" : fileName);
}

bool FilteredVRSFileReader::afterConstraint(const string& after) {
  return filter.afterConstraint(after);
}

bool FilteredVRSFileReader::beforeConstraint(const string& before) {
  return filter.beforeConstraint(before);
}

void FilteredVRSFileReader::setMinTime(double minimumTime, bool relativeToBegin) {
  return filter.setMinTime(minimumTime, relativeToBegin);
}

void FilteredVRSFileReader::setMaxTime(double maximumTime, bool relativeToEnd) {
  return filter.setMaxTime(maximumTime, relativeToEnd);
}

void FilteredVRSFileReader::getTimeRange(double& outStartTimestamp, double& outEndTimestamp) {
  outStartTimestamp = numeric_limits<double>::max();
  outEndTimestamp = numeric_limits<double>::lowest();
  expandTimeRange(outStartTimestamp, outEndTimestamp);
}

void FilteredVRSFileReader::expandTimeRange(
    double& inOutStartTimestamp,
    double& inOutEndTimestamp) {
  const auto& index = reader.getIndex();
  if (!index.empty()) {
    double firstTimestamp = inOutStartTimestamp;
    double lastTimestamp = inOutEndTimestamp;
    for (const auto& r : index) {
      if (filter.streams.find(r.streamId) != filter.streams.end() &&
          r.recordType == Record::Type::DATA) {
        firstTimestamp = r.timestamp;
        break;
      }
    }
    for (auto iter = index.rbegin(); iter != index.rend(); ++iter) {
      if (filter.streams.find(iter->streamId) != filter.streams.end() &&
          iter->recordType == Record::Type::DATA) {
        lastTimestamp = iter->timestamp;
        break;
      }
    }
    if (firstTimestamp < inOutStartTimestamp) {
      inOutStartTimestamp = firstTimestamp;
    }
    if (lastTimestamp > inOutEndTimestamp) {
      inOutEndTimestamp = lastTimestamp;
    }
  }
}

void FilteredVRSFileReader::constrainTimeRange(
    double& inOutStartTimestamp,
    double& inOutEndTimestamp) const {
  if (inOutStartTimestamp < filter.minTime) {
    inOutStartTimestamp = filter.minTime;
  }
  if (inOutEndTimestamp > filter.maxTime) {
    inOutEndTimestamp = filter.maxTime;
  }
}

void FilteredVRSFileReader::getConstrainedTimeRange(
    double& outStartTimestamp,
    double& outEndTimestamp) {
  getTimeRange(outStartTimestamp, outEndTimestamp);
  filter.resolveTimeConstraints(outStartTimestamp, outEndTimestamp);
  constrainTimeRange(outStartTimestamp, outEndTimestamp);
}

void FilteredVRSFileReader::applyFilters(const RecordFilterParams& filters) {
  applyRecordableFilters(filters.streamFilters);
  applyTypeFilters(filters.typeFilters);
  applyDecimateIntervals(filters.decimateIntervals);
}

void FilteredVRSFileReader::applyRecordableFilters(const vector<string>& filters) {
  const auto& fileStreams = reader.getStreams();
  unique_ptr<set<StreamId>> newSet;
  for (auto iter = filters.begin(); iter != filters.end(); ++iter) {
    if (*iter == "+") {
      set<StreamId> argIds;
      stringToIds(*(++iter), fileStreams, argIds);
      if (!newSet) {
        // first command is add? start from empty set
        newSet = make_unique<set<StreamId>>(move(argIds));
      } else {
        for (auto id : argIds) {
          newSet->insert(id);
        }
      }
    } else if (*iter == "-") {
      set<StreamId> argIds;
      stringToIds(*(++iter), fileStreams, argIds);
      if (!newSet) {
        // first command is remove? start with set of all known streams
        newSet = make_unique<set<StreamId>>(fileStreams);
      }
      for (auto id : argIds) {
        newSet->erase(id);
      }
    }
  }
  if (newSet) {
    // Only include streams present in the file
    this->filter.streams.clear();
    for (auto id : *newSet) {
      if (fileStreams.find(id) != fileStreams.end()) {
        this->filter.streams.insert(id);
      }
    }
  } else {
    this->filter.streams = fileStreams;
  }
}

void FilteredVRSFileReader::applyDecimateIntervals(const vector<pair<string, double>>& intervals) {
  const auto& fileStreams = reader.getStreams();

  this->filter.decimateIntervals.clear();
  for (const auto& interval : intervals) {
    set<StreamId> argIds;
    stringToIds(interval.first, fileStreams, argIds);
    for (auto id : argIds) {
      this->filter.decimateIntervals[id] = interval.second;
    }
  }
}

void FilteredVRSFileReader::applyTypeFilters(const vector<string>& filters) {
  set<Record::Type> types = {Record::Type::CONFIGURATION, Record::Type::DATA, Record::Type::STATE};
  set<Record::Type>* newSet = nullptr;
  for (auto iter = filters.begin(); iter != filters.end(); ++iter) {
    const bool isPlus = *iter == "+";
    Record::Type readType = stringToType(*(++iter));
    if (readType != Record::Type::UNDEFINED) {
      if (isPlus) {
        if (newSet == nullptr) {
          // first command is add? start from empty set
          newSet = new set<Record::Type>();
        }
        newSet->insert(readType);
      } else { // if it's not a '+', then it was a '-'
        if (newSet == nullptr) {
          // first command is remove? start with set of all known streams
          newSet = new set<Record::Type>(types);
        }
        newSet->erase(readType);
      }
    }
  }
  if (newSet != nullptr) {
    this->filter.types = move(*newSet);
  } else {
    this->filter.types = move(types);
  }
}

bool RecordFilter::afterConstraint(const string& after) {
  try {
    setMinTime(stod(after), isSigned(after));
    return true;
  } catch (logic_error&) {
    return false;
  }
}

void RecordFilter::setMinTime(double minimumTime, bool relativeToBegin) {
  minTime = minimumTime;
  relativeMinTime = relativeToBegin;
}

bool RecordFilter::beforeConstraint(const string& before) {
  try {
    setMaxTime(stod(before), isSigned(before));
    return true;
  } catch (logic_error&) {
    return false;
  }
}

void RecordFilter::setMaxTime(double maximumTime, bool relativeToEnd) {
  maxTime = maximumTime;
  relativeMaxTime = relativeToEnd;
}

bool RecordFilter::resolveTimeConstraints(double startTimestamp, double endTimestamp) {
  if (relativeMinTime || relativeMaxTime || aroundTime) {
    if (relativeMinTime) {
      minTime += (minTime < 0) ? endTimestamp : startTimestamp;
    }
    if (aroundTime) {
      // maxTime is actually a duration centered around minTime: interpret both
      double baseTime = minTime;
      double diameter = abs(maxTime) / 2;
      minTime = baseTime - diameter;
      maxTime = baseTime + diameter;
    } else if (relativeMaxTime) {
      maxTime += (maxTime < 0) ? endTimestamp : startTimestamp;
    }
    relativeMinTime = false;
    relativeMaxTime = false;
    aroundTime = false;
  }
  return minTime <= maxTime;
}

bool FilteredVRSFileReader::resolveTimeConstraints() {
  const auto& index = reader.getIndex();
  return index.empty()
      ? true
      : filter.resolveTimeConstraints(index.front().timestamp, index.back().timestamp);
}

string RecordFilter::getTimeConstraintDescription() const {
  bool minLimited = minTime > numeric_limits<double>::lowest();
  bool maxLimited = maxTime < numeric_limits<double>::max();
  stringstream oss;
  oss << fixed << setprecision(3);
  if (minLimited && maxLimited) {
    oss << " between " << minTime << " and " << maxTime << " sec";
  } else if (minLimited) {
    oss << " after " << minTime << " sec";
  } else if (maxLimited) {
    oss << " before " << maxTime << " sec";
  }
  return oss.str();
}

string FilteredVRSFileReader::getTimeConstraintDescription() {
  resolveTimeConstraints();
  return filter.getTimeConstraintDescription();
}

inline bool configOrStateRecord(const IndexRecord::RecordInfo& record) {
  return record.recordType == Record::Type::CONFIGURATION ||
      record.recordType == Record::Type::STATE;
}

void FilteredVRSFileReader::preRollConfigAndState() {
  preRollConfigAndState(
      [](RecordFileReader& recordFileReader, const IndexRecord::RecordInfo& record) {
        LOG_ERROR(recordFileReader.readRecord(record));
      });
}

void FilteredVRSFileReader::preRollConfigAndState(
    function<void(RecordFileReader&, const IndexRecord::RecordInfo&)> f) {
  if (!resolveTimeConstraints()) {
    return;
  }
  if (filter.minTime <= numeric_limits<double>::lowest()) {
    return; // not needed: we'll read records from the start
  }
  vector<size_t> indexes;
  const auto& records = reader.getIndex();
  // to compare: only the timestamps matters!
  IndexRecord::RecordInfo firstTime(filter.minTime, 0, StreamId(), Record::Type());
  auto lowerBound = lower_bound(records.begin(), records.end(), firstTime);
  if (lowerBound != records.end()) {
    size_t index = static_cast<size_t>(lowerBound - records.begin()); // guaranteed positive
    // For each stream & record type, track the records we need to write
    set<pair<StreamId, Record::Type>> foundRecords;
    // for each stream, 1 config + 1 state record
    size_t requiredCount = filter.streams.size() * 2;
    indexes.reserve(requiredCount);
    // search records *before* the lower bound index we found
    while (requiredCount > 0 && index-- > 0) {
      const IndexRecord::RecordInfo& record = records[index];
      if (configOrStateRecord(record) &&
          filter.types.find(record.recordType) != filter.types.end() &&
          filter.streams.find(record.streamId) != filter.streams.end() &&
          foundRecords.find({record.streamId, record.recordType}) == foundRecords.end()) {
        foundRecords.insert({record.streamId, record.recordType});
        indexes.push_back(index);
        requiredCount--;
      }
    }
  }
  // we found the records in reserse chronological order: read records sequentially now
  for (size_t k = indexes.size(); k-- > 0; /* size_t is unsigned: loop carefully! */) {
    f(reader, records[indexes[k]]);
  }
}

unique_ptr<deque<IndexRecord::DiskRecordInfo>> FilteredVRSFileReader::buildIndex() {
  auto preliminaryIndex = make_unique<deque<IndexRecord::DiskRecordInfo>>();
  int64_t offset = 0;
  function<bool(RecordFileReader&, const IndexRecord::RecordInfo&)> f =
      [&preliminaryIndex, &offset](RecordFileReader&, const IndexRecord::RecordInfo& record) {
        preliminaryIndex->push_back(
            {record.timestamp,
             static_cast<uint32_t>(record.fileOffset - offset),
             record.streamId,
             record.recordType});
        offset = record.fileOffset;
        return true;
      };
  preRollConfigAndState(f);
  iterate(f);
  return preliminaryIndex;
}

uint32_t FilteredVRSFileReader::iterate(ThrottledWriter* throttledWriter) {
  if (!resolveTimeConstraints()) {
    cerr << "Time Range invalid: " << getTimeConstraintDescription() << endl;
    return 0;
  }
  uint32_t readCounter = 0;
  iterate(
      [&readCounter](RecordFileReader& recordFileReader, const IndexRecord::RecordInfo& record) {
        LOG_ERROR(recordFileReader.readRecord(record));
        readCounter++;
        return true;
      },
      throttledWriter);
  for (auto id : filter.streams) {
    reader.setStreamPlayer(id, nullptr);
  }
  return readCounter;
}

void FilteredVRSFileReader::iterate(
    function<bool(RecordFileReader&, const IndexRecord::RecordInfo&)> f,
    ThrottledWriter* throttledWriter) {
  if (!resolveTimeConstraints()) {
    return;
  }

  using RecordFlavor = tuple<StreamId, Record::Type>;
  set<RecordFlavor> firstRecordsOnlyTracking;
  map<StreamId, double> decimateCursors;
  // Timestamp of the current bucket we are creating
  double bucketCurrentTimestamp = numeric_limits<double>::quiet_NaN();
  map<StreamId, IndexRecord::RecordInfo> bucketCandidates;
  bool keepGoing = true;

  double graceWindow = (filter.bucketInterval > 0) ? filter.bucketInterval * 1.2 : 0;
  auto submitBucket = [&]() {
    double maxTimestamp = 0.0;
    for (const auto& bucketRecord : bucketCandidates) {
      keepGoing &= f(reader, bucketRecord.second);
      maxTimestamp = max(maxTimestamp, bucketRecord.second.timestamp);
    }
    bucketCandidates.clear();
    if (throttledWriter != nullptr) {
      throttledWriter->onRecordDecoded(maxTimestamp, graceWindow);
    }
  };

  const auto& records = reader.getIndex();
  IndexRecord::RecordInfo firstTime(filter.minTime, 0, StreamId(), Record::Type());
  auto lowerBound = lower_bound(records.begin(), records.end(), firstTime);
  if (lowerBound != records.end()) {
    size_t first = static_cast<size_t>(lowerBound - records.begin());
    for (size_t k = first; keepGoing && k < records.size(); ++k) {
      const IndexRecord::RecordInfo& record = records[k];

      if (record.timestamp > filter.maxTime) {
        break; // Records are sorted by timestamp: no need to keep trying
      }
      if (filter.streams.find(record.streamId) == filter.streams.end() ||
          filter.types.find(record.recordType) == filter.types.end()) {
        continue;
      }

      if (firstRecordsOnly) {
        if (firstRecordsOnlyTracking.size() >= filter.streams.size() * 3) {
          break; // we found 1 config, state and data record per stream, we can stop now
        }
        RecordFlavor recordFlavor{record.streamId, record.recordType};
        if (firstRecordsOnlyTracking.find(recordFlavor) == firstRecordsOnlyTracking.end()) {
          firstRecordsOnlyTracking.insert(recordFlavor);
        } else {
          continue;
        }
      }

      if (!filter.decimateIntervals.empty() && record.recordType == Record::Type::DATA) {
        auto decimateInterval = filter.decimateIntervals.find(record.streamId);
        if (decimateInterval != filter.decimateIntervals.end()) {
          auto decimateCursor = decimateCursors.find(record.streamId);
          if (decimateCursor != decimateCursors.end() &&
              record.timestamp - decimateCursor->second < decimateInterval->second) {
            continue; // Decimate this record
          }
          // Keep this record & remember its timestamp
          decimateCursors[record.streamId] = record.timestamp;
        }
      } else if (filter.bucketInterval > 0 && record.recordType == Record::Type::DATA) {
        if (isnan(bucketCurrentTimestamp)) {
          bucketCurrentTimestamp = record.timestamp;
        }

        // are we past the limit for the current bucket?
        if (record.timestamp - bucketCurrentTimestamp > filter.bucketMaxTimestampDelta) {
          // no chance of finding better candidates, we need to "submit" this bucket
          submitBucket();
          bucketCurrentTimestamp += filter.bucketInterval;
        }
        // does this frame qualify for the bucket?
        else if (
            fabs(record.timestamp - bucketCurrentTimestamp) <= filter.bucketMaxTimestampDelta) {
          // can we find a closer candidate for the bucket for this stream id?
          const auto it = bucketCandidates.find(record.streamId);
          if (it == bucketCandidates.end() ||
              fabs(it->second.timestamp - bucketCurrentTimestamp) <
                  fabs(record.timestamp - bucketCurrentTimestamp)) {
            bucketCandidates[record.streamId] = record;
          }
        }

        continue;
      }

      if (skipRecordFilter && skipRecordFilter(record)) {
        continue;
      }

      keepGoing &= f(reader, record);
      if (throttledWriter != nullptr) {
        throttledWriter->onRecordDecoded(record.timestamp, graceWindow);
      }
    }

    if (filter.bucketInterval > 0) {
      submitBucket();
    }
  }
}

void TagOverrides::overrideTags(RecordFileWriter& writer) {
  writer.addTags(fileTags);
  if (!streamTags.empty()) {
    for (Recordable* recordable : writer.getRecordables()) {
      auto iter = streamTags.find(recordable->getStreamId());
      if (iter != streamTags.end()) {
        recordable->addTags(iter->second);
      }
    }
  }
}

} // namespace vrs::utils
