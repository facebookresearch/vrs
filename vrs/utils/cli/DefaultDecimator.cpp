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

#include "DefaultDecimator.h"

#include <cmath>

#include <algorithm>
#include <iostream>
#include <sstream>

#define DEFAULT_LOG_CHANNEL "DataExtraction"
#include <logging/Verify.h>

#include <vrs/helpers/Strings.h>
#include <vrs/utils/FilteredFileReader.h>
#include <vrs/utils/ThrottleHelpers.h>

namespace vrs::utils {

using namespace std;

namespace {

/// @param ids: a text description of one or more streams.
/// @param reader: an open file
/// @param outStreamIds: on exit, a set of stream ids extracted
/// @return true if no parsing error occurred.
/// Supported forms for ids:
///   R-N  where R is a recordable type id as a number, and N an instance id (also a number)
///   R+N  where R is a recordable type id as a number, and N relative instance id (Nth stream)
///   R-   where R is a recordable type id as a number.
///        Returns all the streams in the file with that recordable type id
///   R    Same as R-
///   R-<flavor> Returns all the streams in the file with that recordable type id and flavor
/// Actual examples: 1004-1 or 1004+3 or 1005- or 1005 or 100-test/synthetic/grey8
bool stringToIds(const string& ids, const RecordFileReader& reader, set<StreamId>& outStreamIds) {
  StreamId singleId = reader.getStreamForName(ids);
  if (singleId.isValid()) {
    outStreamIds.insert(singleId);
    return true;
  }
  stringstream ss(ids);
  int typeIdNum = 0;
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
    cerr << "Can't parse '" << ids << "' as one or more stream id.\n";
  }
  return !error;
}

} // namespace

DefaultDecimator::DefaultDecimator(
    FilteredFileReader& filteredReader,
    const DecimationParams& params)
    : filteredReader_{filteredReader},
      bucketInterval_{params.bucketInterval},
      bucketMaxTimestampDelta_{params.bucketMaxTimestampDelta},
      graceWindow_{bucketInterval_ * 1.2} {
  if (XR_VERIFY(filteredReader_.reader.isOpened())) {
    for (const auto& interval : params.decimationIntervals) {
      set<StreamId> argIds;
      stringToIds(interval.first, filteredReader_.reader, argIds);
      for (auto id : argIds) {
        decimationIntervals_[id] = interval.second;
      }
    }
  }
}

void DefaultDecimator::decimate(
    FilteredFileReader& filteredReader,
    const DecimationParams& params) {
  filteredReader.decimator_ = make_unique<DefaultDecimator>(filteredReader, params);
}

void DefaultDecimator::reset() {
  decimateCursors_.clear();
  bucketCurrentTimestamp_ = numeric_limits<double>::quiet_NaN();
  bucketCandidates_.clear();
}

double DefaultDecimator::getGraceWindow() const {
  return graceWindow_;
}

bool DefaultDecimator::decimate(
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

void DefaultDecimator::flush(RecordReaderFunc& recordReaderFunc, ThrottledWriter* throttledWriter) {
  if (bucketInterval_ > 0) {
    submitBucket(recordReaderFunc, throttledWriter);
  }
}

bool DefaultDecimator::submitBucket(
    RecordReaderFunc& recordReaderFunc,
    ThrottledWriter* throttledWriter) {
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

} // namespace vrs::utils
