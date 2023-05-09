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

#include "FilteredFileReader.h"

#include <cmath>

#include <iomanip>
#include <sstream>

#include <vrs/helpers/Strings.h>
#include <vrs/os/Utils.h>

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

namespace {

/// @param ids: a text description of one or more streams.
/// @param reader: an open file
/// @param outStreamIds: on exit, a set of stream ids extracted
/// @return true if no parsing error occurred.
/// Supported forms for ids:
///   N-M  where N is a recordable type id as a number, and M an instance id (also a number)
///   N-   where N is a recordable type id as a number.
///        Returns all the streams in the file with that recordable type id
///   N    Same as N-
///   N-<flavor> Returns all the streams in the file with that recordable type id and flavor
/// Actual examples: 1004-1 or 1005- or 1005 or 100-test/synthetic/grey8
bool stringToIds(const string& ids, RecordFileReader& reader, set<StreamId>& outStreamIds) {
  StreamId singleId = StreamId::fromNumericName(ids);
  if (singleId.isValid()) {
    outStreamIds.insert(singleId);
    return true;
  }
  stringstream ss(ids);
  int typeIdNum;
  bool error = false;
  if (ss >> typeIdNum) {
    bool multiTypeId = false;
    RecordableTypeId typeId = static_cast<RecordableTypeId>(typeIdNum);
    if (ss.peek() == '-') {
      ss.ignore();
      string flavor;
      ss >> flavor;
      if (!flavor.empty()) {
        vector<StreamId> flavorIds = reader.getStreams(typeId, flavor);
        outStreamIds.insert(flavorIds.begin(), flavorIds.end());
      } else {
        multiTypeId = true;
      }
    } else if (ss.eof()) {
      multiTypeId = true;
    } else {
      error = true;
    }
    if (multiTypeId) {
      // No instance ID were provided, insert all the streams with that RecordableTypeId.
      for (const auto& id : reader.getStreams()) {
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

Record::Type stringToType(const string& type) {
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

inline bool isSigned(const string& str) {
  return !str.empty() && (str.front() == '+' || str.front() == '-');
}

bool isValidNumericName(const string& numericName) {
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

} // namespace

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

int FilteredFileReader::setSource(
    const string& filePath,
    const unique_ptr<FileHandler>& fileHandler) {
  if (fileHandler) {
    reader.setFileHandler(fileHandler->makeNew());
  }
  if (helpers::endsWith(filePath, ".vrs")) {
    return RecordFileReader::vrsFilePathToFileSpec(filePath, spec);
  }
  return spec.fromPathJsonUri(filePath);
}

bool FilteredFileReader::fileExists() const {
  if (spec.isDiskFile()) {
    return !spec.chunks.empty() && os::pathExists(spec.chunks.front());
  }
  return !spec.fileHandlerName.empty();
}

string FilteredFileReader::getPathOrUri() const {
  return spec.toPathJsonUri();
}

string FilteredFileReader::getFileName() {
  return spec.getFileName();
}

int64_t FilteredFileReader::getFileSize() const {
  return spec.getFileSize();
}

int FilteredFileReader::openFile(const RecordFilterParams& filters) {
  int status = spec.empty() ? ErrorCode::INVALID_REQUEST : reader.openFile(spec);
  if (status != 0) {
    return status;
  }
  applyFilters(filters);
  return status;
}

int FilteredFileReader::openFile(unique_ptr<FileHandler>& file) const {
  file = make_unique<DiskFile>();
  int status = file->openSpec(spec);
  if (status != 0) {
    cerr << "Can't open '" << getPathOrUri() << "': " << errorCodeToMessage(status) << endl;
  }
  return status;
}

string FilteredFileReader::getCopyPath() {
  // if uploading, but no temp file path has been provided, automatically generate one
  string fileName = getFileName();
  return os::getTempFolder() + (fileName.empty() ? "file.tmp" : fileName);
}

bool FilteredFileReader::afterConstraint(const string& after) {
  return filter.afterConstraint(after);
}

bool FilteredFileReader::beforeConstraint(const string& before) {
  return filter.beforeConstraint(before);
}

void FilteredFileReader::setMinTime(double minimumTime, bool relativeToBegin) {
  return filter.setMinTime(minimumTime, relativeToBegin);
}

void FilteredFileReader::setMaxTime(double maximumTime, bool relativeToEnd) {
  return filter.setMaxTime(maximumTime, relativeToEnd);
}

void FilteredFileReader::getTimeRange(double& outStartTimestamp, double& outEndTimestamp) const {
  outStartTimestamp = numeric_limits<double>::max();
  outEndTimestamp = numeric_limits<double>::lowest();
  expandTimeRange(outStartTimestamp, outEndTimestamp);
}

void FilteredFileReader::expandTimeRange(double& inOutStartTimestamp, double& inOutEndTimestamp)
    const {
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

void FilteredFileReader::constrainTimeRange(double& inOutStartTimestamp, double& inOutEndTimestamp)
    const {
  if (inOutStartTimestamp < filter.minTime) {
    inOutStartTimestamp = filter.minTime;
  }
  if (inOutEndTimestamp > filter.maxTime) {
    inOutEndTimestamp = filter.maxTime;
  }
}

void FilteredFileReader::getConstrainedTimeRange(
    double& outStartTimestamp,
    double& outEndTimestamp) {
  getTimeRange(outStartTimestamp, outEndTimestamp);
  filter.resolveRelativeTimeConstraints(outStartTimestamp, outEndTimestamp);
  constrainTimeRange(outStartTimestamp, outEndTimestamp);
}

void FilteredFileReader::applyFilters(const RecordFilterParams& filters) {
  applyRecordableFilters(filters.streamFilters);
  applyTypeFilters(filters.typeFilters);
  if (filters.decimationParams) {
    decimator_ = make_unique<Decimator>(*this, *filters.decimationParams);
  }
}

void FilteredFileReader::applyRecordableFilters(const vector<string>& filters) {
  const auto& fileStreams = reader.getStreams();
  unique_ptr<set<StreamId>> newSet;
  for (auto iter = filters.begin(); iter != filters.end(); ++iter) {
    if (*iter == "+") {
      set<StreamId> argIds;
      stringToIds(*(++iter), reader, argIds);
      if (!newSet) {
        // first command is add? start from empty set
        newSet = make_unique<set<StreamId>>(std::move(argIds));
      } else {
        newSet->insert(argIds.begin(), argIds.end());
      }
    } else if (*iter == "-") {
      set<StreamId> argIds;
      stringToIds(*(++iter), reader, argIds);
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

void FilteredFileReader::applyTypeFilters(const vector<string>& filters) {
  set<Record::Type> types = {Record::Type::CONFIGURATION, Record::Type::DATA, Record::Type::STATE};
  std::unique_ptr<set<Record::Type>> newSet = nullptr;
  for (auto iter = filters.begin(); iter != filters.end(); ++iter) {
    const bool isPlus = *iter == "+";
    Record::Type readType = stringToType(*(++iter));
    if (readType != Record::Type::UNDEFINED) {
      if (isPlus) {
        if (newSet == nullptr) {
          // first command is add? start from empty set
          newSet = std::make_unique<set<Record::Type>>();
        }
        newSet->insert(readType);
      } else { // if it's not a '+', then it was a '-'
        if (newSet == nullptr) {
          // first command is remove? start with set of all known streams
          newSet = std::make_unique<set<Record::Type>>(types);
        }
        newSet->erase(readType);
      }
    }
  }
  if (newSet != nullptr) {
    this->filter.types = std::move(*newSet);
  } else {
    this->filter.types = std::move(types);
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

bool RecordFilter::resolveRelativeTimeConstraints(double startTimestamp, double endTimestamp) {
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

bool RecordFilter::timeRangeValid() const {
  return !relativeMinTime && !relativeMaxTime && !aroundTime && minTime <= maxTime;
}

Decimator::Decimator(FilteredFileReader& filteredReader, DecimationParams& params)
    : filteredReader_{filteredReader},
      bucketInterval_{params.bucketInterval},
      bucketMaxTimestampDelta_{params.bucketMaxTimestampDelta},
      graceWindow_{bucketInterval_ * 1.2} {
  for (const auto& interval : params.decimationIntervals) {
    set<StreamId> argIds;
    stringToIds(interval.first, filteredReader_.reader, argIds);
    for (auto id : argIds) {
      decimationIntervals_[id] = interval.second;
    }
  }
}

void Decimator::reset() {
  decimateCursors_.clear();
  bucketCurrentTimestamp_ = numeric_limits<double>::quiet_NaN();
  bucketCandidates_.clear();
}

double Decimator::getGraceWindow() const {
  return graceWindow_;
}

bool Decimator::decimate(
    RecordReaderFunc& recordReaderFunc,
    ThrottledWriter* throttledWriter,
    const IndexRecord::RecordInfo& record,
    bool& inOutKeepGoing) {
  // only decimate data records
  if (record.recordType != Record::Type::DATA) {
    return false;
  }
  // Interval decimation
  if (!decimationIntervals_.empty()) {
    auto decimateInterval = decimationIntervals_.find(record.streamId);
    if (decimateInterval != decimationIntervals_.end()) {
      auto decimateCursor = decimateCursors_.find(record.streamId);
      if (decimateCursor != decimateCursors_.end() &&
          record.timestamp < decimateCursor->second + decimateInterval->second) {
        return true; // Decimate this record
      }
      // Keep this record & remember its timestamp
      decimateCursors_[record.streamId] = record.timestamp;
    }
    return false;
  }
  // Bucket decimation
  if (bucketInterval_ > 0) {
    if (isnan(bucketCurrentTimestamp_)) {
      bucketCurrentTimestamp_ = record.timestamp;
    }
    // are we past the limit for the current bucket?
    if (record.timestamp - bucketCurrentTimestamp_ > bucketMaxTimestampDelta_) {
      // no chance of finding better candidates, we need to "submit" this bucket
      inOutKeepGoing = submitBucket(recordReaderFunc, throttledWriter);
      bucketCurrentTimestamp_ += bucketInterval_;
    }
    // does this frame qualify for the bucket?
    else if (fabs(record.timestamp - bucketCurrentTimestamp_) <= bucketMaxTimestampDelta_) {
      // can we find a closer candidate for the bucket for this stream id?
      const auto it = bucketCandidates_.find(record.streamId);
      if (it == bucketCandidates_.end() ||
          fabs(it->second->timestamp - bucketCurrentTimestamp_) <
              fabs(record.timestamp - bucketCurrentTimestamp_)) {
        bucketCandidates_[record.streamId] = &record;
      }
    }
    return true;
  }
  return false;
}

void Decimator::flush(RecordReaderFunc& recordReaderFunc, ThrottledWriter* throttledWriter) {
  if (bucketInterval_ > 0) {
    submitBucket(recordReaderFunc, throttledWriter);
  }
}

bool Decimator::submitBucket(RecordReaderFunc& recordReaderFunc, ThrottledWriter* throttledWriter) {
  bool keepGoing = true;
  double maxTimestamp = 0.0;
  for (const auto& bucketRecord : bucketCandidates_) {
    keepGoing = keepGoing && recordReaderFunc(filteredReader_.reader, *bucketRecord.second);
    maxTimestamp = max(maxTimestamp, bucketRecord.second->timestamp);
  }
  bucketCandidates_.clear();
  if (throttledWriter != nullptr) {
    throttledWriter->onRecordDecoded(maxTimestamp, graceWindow_);
  }
  return keepGoing;
}

bool FilteredFileReader::timeRangeValid() const {
  return reader.getIndex().empty() || filter.timeRangeValid();
}

string FilteredFileReader::getTimeConstraintDescription() {
  return filter.getTimeConstraintDescription();
}

inline bool configOrStateRecord(const IndexRecord::RecordInfo& record) {
  return record.recordType == Record::Type::CONFIGURATION ||
      record.recordType == Record::Type::STATE;
}

void FilteredFileReader::preRollConfigAndState() {
  preRollConfigAndState(
      [](RecordFileReader& recordFileReader, const IndexRecord::RecordInfo& record) {
        LOG_ERROR(recordFileReader.readRecord(record));
        return true;
      });
}

void FilteredFileReader::preRollConfigAndState(RecordReaderFunc recordReaderFunc) {
  if (!timeRangeValid()) {
    return;
  }
  if (filter.minTime <= numeric_limits<double>::lowest()) {
    return; // not needed: we'll read records from the start
  }
  vector<size_t> indexes;
  const auto& records = reader.getIndex();
  // to compare: only the timestamps matters!
  IndexRecord::RecordInfo firstTime(filter.minTime, 0, StreamId(), Record::Type::UNDEFINED);
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
  // we found the records in reverse chronological order: read records sequentially now
  for (size_t k = indexes.size(); k-- > 0; /* size_t is unsigned: loop carefully! */) {
    recordReaderFunc(reader, records[indexes[k]]);
  }
}

uint32_t FilteredFileReader::iterateSafe() {
  double startTimestamp, endTimestamp;
  getConstrainedTimeRange(startTimestamp, endTimestamp);
  preRollConfigAndState();
  return iterateAdvanced();
}

unique_ptr<deque<IndexRecord::DiskRecordInfo>> FilteredFileReader::buildIndex() {
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
  iterateAdvanced(f);
  return preliminaryIndex;
}

uint32_t FilteredFileReader::iterateAdvanced(ThrottledWriter* throttledWriter) {
  if (!timeRangeValid()) {
    cerr << "Time Range invalid: " << getTimeConstraintDescription() << endl;
    return 0;
  }
  uint32_t readCounter = 0;
  iterateAdvanced(
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

void FilteredFileReader::iterateAdvanced(
    RecordReaderFunc recReaderF,
    ThrottledWriter* throttledWriter) {
  if (!timeRangeValid()) {
    return;
  }

  using RecordFlavor = tuple<StreamId, Record::Type>;
  set<RecordFlavor> firstRecordsOnlyTracking;
  bool keepGoing = true;

  double graceWindow = decimator_ ? decimator_->getGraceWindow() : 0;
  if (decimator_) {
    decimator_->reset();
  }

  const auto& records = reader.getIndex();
  IndexRecord::RecordInfo firstTime(filter.minTime, 0, StreamId(), Record::Type::UNDEFINED);
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

      if ((decimator_ && decimator_->decimate(recReaderF, throttledWriter, record, keepGoing)) ||
          (skipRecordFilter && skipRecordFilter(record))) {
        continue;
      }

      keepGoing = keepGoing && recReaderF(reader, record);
      if (throttledWriter != nullptr) {
        throttledWriter->onRecordDecoded(record.timestamp, graceWindow);
      }
    }

    if (decimator_) {
      decimator_->flush(recReaderF, throttledWriter);
    }
  }
}

} // namespace vrs::utils
