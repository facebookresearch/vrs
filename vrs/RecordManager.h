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

#include <functional>
#include <list>
#include <mutex>

#include "ForwardDefinitions.h"
#include "Record.h"

namespace vrs {

using std::list;

class DataLayout;

/// \brief VRS internal class to manage the records of a specific Recordable after their creation.
///
/// Each Recordable has its own RecordManager to minize (ideally avoid) inter-thread locking.
/// All timestamps are in seconds since some arbitrary point in time, and must be using the same
/// time domain for the entire VRS file.
/// @internal
class RecordManager {
  friend class Record;

 public:
  RecordManager();
  ~RecordManager();

  /// Create & hold a record using the given parameters. RecordManager is responsible for deleting
  /// the record. Copies the data referenced by the DataSource.
  /// @param timestamp: Timestamp of the record, in seconds.
  /// @param type: Type of the record.
  /// @param formatVersion: Version number of the format of the record, so that when the record is
  /// read, the data can be interpreted appropriately.
  /// @return A pointer to the record created.
  Record*
  createRecord(double timestamp, Record::Type type, uint32_t formatVersion, const DataSource& data);

  /// Recycle or delete buffers older than a time.
  /// @param oldestTimestamp: Max timestamp of the records to purge.
  /// @param recycleBuffers: Tell if old records should be recycled (true) or delete (false).
  /// @return Number of records purged.
  uint32_t purgeOldRecords(double oldestTimestamp, bool recycleBuffers = true);

  /// Release as much memory as possible, by deleting all the records in the cache.
  void purgeCache();

  /// Collect records older than a specific timestamp.
  /// The ownership of the records returned is transferred to the caller.
  /// @param maxAge: Max timestamp value of the records to collect.
  /// @param collectedRecords: A container that will contain the collected records.
  void collectOldRecords(double maxAge, list<Record*>& outCollectedRecords);

  /// Get the compression preset to use when writing the records of this RecordManager.
  /// @return The compression preset.
  CompressionPreset getCompression() const {
    return compression_;
  }

  /// Set the compression preset to use when writing the records of this RecordManager,
  /// to override the default value: CompressionPreset::Default.
  /// @param compression: CompressionPreset to use.
  void setCompression(CompressionPreset compression) {
    compression_ = compression;
  }

  /// If you know that you have a lot samples that are small. You can increase
  /// the cache size to avoid allocations. The default value is kMaxCacheSize = 10.
  /// @param max: The maximum amount of reusable records to keep around.
  void setMaxCacheSize(size_t max) {
    maxCacheSize_ = max;
  }

  /// When a record's buffer is allocated, we may want to over-allocate a little bit to improve
  /// the chances that when the record is recycled & reused, it we don't need to grow it.
  /// This optional configuration allows to allocate more space, by either a min number of bytes,
  /// a min percentage of bytes, or both, in which case the *min* of either setting is used.
  /// If one of the values is set to 0, then it is considered not set. By default, both are 0.
  /// @param minBytes: absolute number of bytes.
  /// @param minPercent: percentage of bytes to allocate on top of the request.
  void setRecordBufferOverAllocationMins(size_t minBytes, size_t minPercent) {
    minBytesOverAllocation_ = minBytes;
    minPercentOverAllocation_ = minPercent;
  }

  /// Get how many bytes should actually be allocated when we want to use a specific number of
  /// bytes. This number may be greater than the requested size, to improve the ability to reuse an
  /// allocated buffer without having to reallocate memory. See setRecordBufferOverAllocationMins().
  /// @param requestedSise: number of bytes needed.
  /// @return Number of bytes that should be allocated.
  /// @internal only VRS itself should need to use this API.
  size_t getAdjustedRecordBufferSize(size_t requestedSize) const;

  /// Get the count of recycled records are currently waiting to be reused.
  /// @return Number of recycled records available.
  /// @internal For use by unit tests.
  size_t getCurrentCacheSize() const {
    return cache_.size();
  }

 private:
  size_t getAcceptableOverCapacity(size_t capacity) const;
  void recycle(Record* record);

  std::mutex mutex_;
  list<Record*> activeRecords_; // Records ready to be written
  list<Record*> cache_; // Written/recycled records available for reuse

  CompressionPreset compression_;
  size_t maxCacheSize_;
  size_t minBytesOverAllocation_;
  size_t minPercentOverAllocation_;
  uint64_t creationOrder_;
};

} // namespace vrs
