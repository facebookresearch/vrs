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

#include <map>

#include <vrs/utils/FilteredFileReader.h>

namespace vrs::utils {

/// Default decimator handling stream interval & bucket decimation
class DefaultDecimator : public Decimator {
 public:
  struct Params {
    Params(double minIntervalSec = 0, double bucketMaxTimestampDeltaSec = 1.0 / 30.0)
        : bucketInterval{minIntervalSec}, bucketMaxTimestampDelta{bucketMaxTimestampDeltaSec} {}

    // Per stream decimation intervals
    vector<pair<string, double>> decimationIntervals;
    // Divide time where we have all records into intervals, 0 to disable bucketing
    double bucketInterval;
    // Disregard records which timestamp is more than this delta away from the bucket's
    double bucketMaxTimestampDelta;

    // Create DefaultDecimator from params and apply to a FilteredFileReader
    void decimate(FilteredFileReader& filteredReader) const;
  };

  DefaultDecimator(FilteredFileReader& filteredReader, const Params& params);

  void reset() override;
  bool decimate(
      RecordReaderFunc& recordReaderFunc,
      ThrottledWriter* throttledWriter,
      const IndexRecord::RecordInfo& record,
      bool& inOutKeepGoing) override;
  void flush(RecordReaderFunc& recordReaderFunc, ThrottledWriter* throttledWriter) override;
  double getGraceWindow() const override;

 private:
  bool submitBucket(RecordReaderFunc& recordReaderFunc, ThrottledWriter* throttledWriter);

  FilteredFileReader& filteredReader_;
  // Timestamp intervals used to skip data records (does not apply to config and state records)
  std::map<StreamId, double> decimationIntervals_;
  // Divide time where we have all records into intervals, 0 to disable bucketing
  const double bucketInterval_;
  // Disregard records which timestamp is more than this delta away from the bucket's
  const double bucketMaxTimestampDelta_;
  // Grace time window to avoid unsorted records because of pending buckets
  const double graceWindow_;

  // iteration specific variables
  std::map<StreamId, double> decimateCursors_;
  // Timestamp of the current bucket we are creating
  double bucketCurrentTimestamp_{};
  std::map<StreamId, const IndexRecord::RecordInfo*> bucketCandidates_;
};

} // namespace vrs::utils
