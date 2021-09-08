// Facebook Technologies, LLC Proprietary and Confidential.

#include "RecordManager.h"

#include <algorithm>

#include <vrs/os/Time.h>

#include "Compressor.h"
#include "DataSource.h"

using namespace std;

namespace vrs {

static const size_t kUnlockToCopySizeLimit =
    1024; // over 1 KB of data? release/relock while copying!
static const size_t kDefaultMaxCacheSize = 50;
static const double kMaxCycledRecordAge = 1; // to reuse old records more aggressively

RecordManager::RecordManager()
    : compression_{CompressionPreset::Default},
      maxCacheSize_{kDefaultMaxCacheSize},
      minBytesOverAllocation_{},
      minPercentOverAllocation_{},
      creationOrder_{0} {}

RecordManager::~RecordManager() {
  std::unique_lock<std::mutex> guard{mutex_};
  // Records should never be deleted directly by anyone else,
  // so their destructor is private. Which means even containers
  // are not allowed to do that, so we're forced to manually handle that.
  // In particular, using unique_ptr is not an option!
  for (Record* record : cache_) {
    delete record;
  }
  for (Record* record : activeRecords_) {
    delete record;
  }
}

Record* RecordManager::createRecord(
    double timestamp,
    Record::Type recordType,
    uint32_t formatVersion,
    const DataSource& data) {
  // Step 1: find a record to reuse, if there is any
  Record* record = nullptr;
  std::unique_lock<std::mutex> guard{mutex_};
  const size_t dataSize = data.getDataSize();
  const size_t maxSize = getAcceptableOverCapacity(dataSize);
  // reuse the most recently inserted records first, as they're less likely to have been swapped out
  // of memory
  for (list<Record*>::iterator iter = cache_.begin(); iter != cache_.end(); ++iter) {
    const size_t bufferCapacity = (*iter)->buffer_.capacity();
    if (bufferCapacity >= dataSize && bufferCapacity <= maxSize) {
      record = *iter;
      cache_.erase(iter);
      break;
    }
  }
  // If we haven't found a match, maybe use one anyways, if the cache is full, or records old
  if (record == nullptr && !cache_.empty() &&
      (cache_.size() >= kDefaultMaxCacheSize ||
       cache_.back()->timestamp_ + kMaxCycledRecordAge < os::getTimestampSec())) {
    record = cache_.back();
    cache_.pop_back();
  }
  uint64_t creationOrder = ++creationOrder_; // get a creation order while we hold the lock
  // Step 2: copy the data in the record
  bool largeData = dataSize >= kUnlockToCopySizeLimit;
  // If the copy operation is large, unlock the mutex while copying the data, then relock
  if (largeData) {
    guard.unlock();
  }
  if (record == nullptr) {
    record = new Record(*this);
  }
  record->set(timestamp, recordType, formatVersion, data, creationOrder);
  if (largeData) {
    guard.lock();
  }
  // Step 3: Insert the record in the active records, sorted
  // activeRecords_ is *always* sorted, oldest first, newest last
  // Newer records to insert should be newer than what we have, so we start from the back
  if (activeRecords_.empty() || activeRecords_.back()->timestamp_ <= timestamp) {
    activeRecords_.emplace_back(record); // normal case: the new record is more recent
  } else {
    // starting from the end, find the last record more recent than our "new" record
    // index always points to a record more recent than the record we want to insert
    list<Record*>::iterator iter = --activeRecords_.end();
    while (iter != activeRecords_.begin() && timestamp < (*iter)->timestamp_) {
      --iter;
    }
    if (timestamp >= (*iter)->timestamp_) {
      ++iter;
    }
    activeRecords_.insert(iter, record);
  }
  return record;
}

uint32_t RecordManager::purgeOldRecords(double oldestTimestamp, bool recycleBuffers) {
  uint32_t count = 0;
  std::unique_lock<std::mutex> guard{mutex_};
  // we purge everything "old", but we keep the newest state & configuration records,
  // and all TagsRecords
  Record* lastState = nullptr;
  Record* lastConfiguration = nullptr;
  list<Record*> tagsRecords;
  auto iter = activeRecords_.begin();
  while (iter != activeRecords_.end() && (*iter)->timestamp_ < oldestTimestamp) {
    Record* record = *iter;
    activeRecords_.erase(iter);
    if (record->getRecordType() == Record::Type::STATE &&
        (lastState == nullptr || lastState->getTimestamp() < record->getTimestamp())) {
      swap(record, lastState);
    } else if (
        record->getRecordType() == Record::Type::CONFIGURATION &&
        (lastConfiguration == nullptr ||
         lastConfiguration->getTimestamp() < record->getTimestamp())) {
      swap(record, lastConfiguration);
    } else if (record->getRecordType() == Record::Type::TAGS) {
      tagsRecords.push_back(record);
      record = nullptr;
    }
    if (record) {
      if (recycleBuffers && cache_.size() < maxCacheSize_) {
        cache_.emplace_back(record);
      } else {
        delete record;
      }
      ++count;
    }
    iter = activeRecords_.begin();
  }
  if (lastState) {
    activeRecords_.emplace_front(lastState);
  }
  if (lastConfiguration) {
    activeRecords_.emplace_front(lastConfiguration);
  }
  if (!tagsRecords.empty()) {
    activeRecords_.insert(activeRecords_.begin(), tagsRecords.begin(), tagsRecords.end());
  }
  return count;
}

void RecordManager::purgeCache() {
  std::unique_lock<std::mutex> guard{mutex_};
  for (Record* record : cache_) {
    delete record;
  }
  cache_.clear();
}

void RecordManager::collectOldRecords(double maxAge, list<Record*>& outCollectedRecords) {
  outCollectedRecords.clear();
  std::lock_guard<std::mutex> guard{mutex_};
  if (!activeRecords_.empty()) {
    auto iterator = std::upper_bound(
        activeRecords_.begin(), activeRecords_.end(), maxAge, [](double age, Record* record) {
          return age < record->timestamp_;
        });

    // Move a range without copying elements.
    outCollectedRecords.splice(
        outCollectedRecords.begin(), activeRecords_, activeRecords_.begin(), iterator);
  }
}

size_t RecordManager::getAdjustedRecordBufferSize(size_t requestedSize) {
  if (minPercentOverAllocation_ == 0) {
    return requestedSize + minBytesOverAllocation_; // at most one value set
  }
  size_t percentOverAllocation = requestedSize * minPercentOverAllocation_ / 100;
  if (minBytesOverAllocation_ == 0) {
    return requestedSize + percentOverAllocation;
  }
  // Use min, not max, to prevent massive over-allocation!
  return requestedSize + std::min<size_t>(minBytesOverAllocation_, percentOverAllocation);
}

size_t RecordManager::getAcceptableOverCapacity(size_t capacity) const {
  return capacity + capacity / 5; // 20%
}

void RecordManager::recycle(Record* record) {
  { // mutex scope limiting
    std::unique_lock<std::mutex> guard{mutex_};
    if (cache_.size() < maxCacheSize_) {
      record->timestamp_ = os::getTimestampSec();
      cache_.emplace_back(record);
      record = nullptr;
    }
  } // mutex scope limiting
  if (record != nullptr) {
    delete record;
  }
}

} // namespace vrs
