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

#include <vrs/utils/FrameRateEstimator.h>

#include <limits>
#include <map>

#define DEFAULT_LOG_CHANNEL "FrameRateEstimator"
#include <logging/Log.h>

#include <vrs/IndexRecord.h>

using namespace std;
using namespace vrs;

namespace {

// number of hits in a bucket to qualify for framerate
const uint32_t kBucketCountQualification = 500;

const double kMilliseconds = 0.001;

uint32_t msToBucketIndex(double seconds) {
  if (seconds < 10 * kMilliseconds) {
    // Under 10ms, one bucket per ms
    uint32_t bucketIndex = static_cast<uint32_t>(seconds * 1000);
    if (bucketIndex > 0) {
      return bucketIndex;
    }
    // bucket 0 is excluded from averages.
    // In case we have a device running at 1kHz, let's push-up samples slightly under 1ms up to 1...
    return seconds < 0.75 * kMilliseconds ? 0 : 1;
  }
  if (seconds < 1) {
    // after, but under 1s, one bucket for 5ms intervals
    uint32_t bucketIndex = static_cast<uint32_t>(seconds * 1000);
    return bucketIndex - (bucketIndex % 5);
  }
  // after 1s, one bucket per second
  return static_cast<uint32_t>(seconds) * 1000;
}

struct Bucket {
  void recordInterval(double seconds) {
    count++;
    sum += seconds;
  }
  void add(const Bucket& bucket) {
    count += bucket.count;
    sum += bucket.sum;
  }
  double getAverage() const {
    return count / sum;
  }

  uint32_t count{};
  double sum{};
};

} // namespace

namespace vrs::utils {

#define LOG_BUCKETS 0 // for debugging purposes

double frameRateEstimationFps(const vector<IndexRecord::RecordInfo>& index, vrs::StreamId id) {
  double start = std::numeric_limits<double>::max();
  map<uint32_t, Bucket> buckets;
  uint32_t maxBucketCount = 0;
  uint32_t maxBucketIndex = 0;
  uint32_t gapCount = 0;
  double previousTimestamp = 0;
  for (const auto& record : index) {
    double timeGap = 0;
    if (record.streamId != id || record.recordType != vrs::Record::Type::DATA) {
      continue;
    }
    if (start > record.timestamp) {
      // first timestamp, we don't have any interval yet
      start = record.timestamp;
    } else {
      timeGap = record.timestamp - previousTimestamp;
      uint32_t bucketIndex = msToBucketIndex(timeGap);
      Bucket& bucket = buckets[bucketIndex];
      bucket.recordInterval(timeGap);
      if (bucketIndex != 0 && maxBucketCount < bucket.count) {
        maxBucketCount = bucket.count;
        maxBucketIndex = bucketIndex;
      }
      if (++gapCount > kBucketCountQualification) {
        break;
      }
    }
    previousTimestamp = record.timestamp;
  }
  if (gapCount < 1) {
    // No gap: arbitrary default response
    return 30;
  }
  Bucket sum;
  if (gapCount < 10) {
    // Too few samples: just average everything
    for (auto iter : buckets) {
      sum.add(iter.second);
    }
    return sum.getAverage();
  }
#if LOG_BUCKETS
  for (const auto& bucket : buckets) {
    XR_LOGI("Bucket around {} ms: {} values", bucket.first, bucket.second.count);
  }
#endif
  // enough samples: accumulate buckets starting with the one with the most hits
  auto mostIter = buckets.find(maxBucketIndex);
  sum.add(mostIter->second);
  auto before = buckets.crbegin();
  while (before != buckets.rend() && before->first != mostIter->first) {
    ++before;
  }
  if (before != buckets.rend()) {
    ++before;
  }
  auto after = mostIter;
  ++after;
  const uint32_t gapCountTarget = gapCount * 80 / 100;
  while (sum.count < gapCountTarget) {
    uint32_t countBefore = 0;
    if (before != buckets.rend() && before->first != 0) {
      countBefore = before->second.count;
    }
    uint32_t countAfter = 0;
    if (after != buckets.end()) {
      countAfter = after->second.count;
    }
    if (countBefore > countAfter) {
      sum.add(before->second);
      ++before;
    } else if (countAfter > 0) {
      sum.add(after->second);
      ++after;
    } else {
      break;
    }
  }
#if LOG_BUCKETS
  XR_LOGI("Final Estimation: {:.02f} fps, {} ms", sum.getAverage(), int(1000. / sum.getAverage()));
#endif
  return sum.getAverage();
}

} // namespace vrs::utils
