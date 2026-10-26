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

#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <new>
#include <tuple>
#include <utility>
#include <vector>

#include <fmt/core.h>

#include <vrs/helpers/IOVector.h>

extern "C" uint64_t overwriteAndChecksum(void* storage, size_t byteCount, uint8_t value) noexcept;
extern "C" uint64_t sampleChecksum(const void* storage, size_t byteCount) noexcept;

namespace {

using Clock = std::chrono::steady_clock;

#if defined(_MSC_VER)
#define VRS_IOVECTOR_BENCHMARK_FORCE_INLINE __forceinline
#define VRS_IOVECTOR_BENCHMARK_ISOLATED __declspec(noinline)
#elif defined(__GNUC__)
#define VRS_IOVECTOR_BENCHMARK_FORCE_INLINE inline __attribute__((always_inline))
// Keep equivalent hot loops at the same page offset so code placement does not dominate them.
#define VRS_IOVECTOR_BENCHMARK_ISOLATED __attribute__((aligned(4096), noinline))
#else
#define VRS_IOVECTOR_BENCHMARK_FORCE_INLINE inline
#define VRS_IOVECTOR_BENCHMARK_ISOLATED
#endif

constexpr size_t kLargeBufferBytes = 2 * 1024 * 1024;
constexpr size_t kSmallBufferBytes = 4 * 1024;
constexpr size_t kBytesPerReuseSample = 256 * 1024 * 1024;
constexpr size_t kElementsPerAppendSample = 8 * 1024 * 1024;
constexpr size_t kElementsPerAppendWarmup = 2 * 1024 * 1024;
constexpr size_t kColdBufferCount = 64;
constexpr size_t kReserveBufferCount = 32;
constexpr size_t kSampleCount = 12;
constexpr size_t kIndependentRunCount = 3;
const char* sBuildLabel = "unspecified";
constexpr std::array<size_t, 12> kRealisticBufferBytes{
    4 * 1024,
    8 * 1024,
    16 * 1024,
    64 * 1024,
    256 * 1024,
    1024 * 1024,
    2 * 1024 * 1024,
    1024 * 1024,
    128 * 1024,
    32 * 1024,
    512 * 1024,
    4 * 1024,
};

template <typename T>
using StandardVector = std::vector<T>;

struct UninitializedByte {
  uint8_t value;

  // Mirrors the shipped VRS wrapper whose constructor intentionally writes nothing.
  // NOLINTNEXTLINE(cppcoreguidelines-pro-type-member-init, modernize-use-equals-default)
  UninitializedByte() noexcept {}
};

static_assert(sizeof(UninitializedByte) == sizeof(uint8_t));

template <typename T>
class DirectIOBuffer {
 public:
  using value_type = T;

  DirectIOBuffer() = default;
  DirectIOBuffer(const DirectIOBuffer&) = delete;
  DirectIOBuffer& operator=(const DirectIOBuffer&) = delete;

  DirectIOBuffer(DirectIOBuffer&& other) noexcept
      : data_{std::exchange(other.data_, nullptr)},
        end_{std::exchange(other.end_, nullptr)},
        capacityEnd_{std::exchange(other.capacityEnd_, nullptr)} {}

  DirectIOBuffer& operator=(DirectIOBuffer&& other) noexcept {
    if (this != &other) {
      delete[] data_;
      data_ = std::exchange(other.data_, nullptr);
      end_ = std::exchange(other.end_, nullptr);
      capacityEnd_ = std::exchange(other.capacityEnd_, nullptr);
    }
    return *this;
  }

  ~DirectIOBuffer() {
    delete[] data_;
  }

  void reserve(size_t count) {
    if (count > capacity()) {
      reallocatePreserving(count);
    }
  }

  void clear() noexcept {
    end_ = data_;
  }

  void resizeForOverwrite(size_t count) {
    if (count > capacity()) {
      T* newData = allocate(count);
      delete[] data_;
      data_ = newData;
      capacityEnd_ = data_ + count;
    }
    setSize(count);
  }

  void resizePreserving(size_t count) {
    if (count > capacity()) {
      reallocatePreserving(growthCapacity(count));
    }
    setSize(count);
  }

  void resizeWithInitialization(size_t count) {
    const size_t oldSize = size();
    if (count > capacity()) {
      reallocatePreserving(growthCapacity(count));
    }
    if (count > oldSize) {
      std::fill_n(data_ + oldSize, count - oldSize, T{});
    }
    setSize(count);
  }

  void push_back(const T& value) {
    if (end_ == capacityEnd_) {
      const size_t oldSize = size();
      const size_t newCapacity = growthCapacity(oldSize + 1);
      T* newData = allocate(newCapacity);
      ::new (static_cast<void*>(newData + oldSize)) T(value);
      copyValues(newData, data_, oldSize);
      delete[] data_;
      data_ = newData;
      end_ = data_ + oldSize + 1;
      capacityEnd_ = data_ + newCapacity;
    } else {
      ::new (static_cast<void*>(end_)) T(value);
      ++end_;
    }
  }

  template <typename... Args>
  T& emplace_back(Args&&... args) {
    if (end_ != capacityEnd_) {
      T* result = ::new (static_cast<void*>(end_)) T(std::forward<Args>(args)...);
      ++end_;
      return *result;
    }
    const size_t oldSize = size();
    const size_t newCapacity = growthCapacity(oldSize + 1);
    T* newData = allocate(newCapacity);
    T* result = ::new (static_cast<void*>(newData + oldSize)) T(std::forward<Args>(args)...);
    copyValues(newData, data_, oldSize);
    delete[] data_;
    data_ = newData;
    end_ = data_ + oldSize + 1;
    capacityEnd_ = data_ + newCapacity;
    return *result;
  }

  T* data() noexcept {
    return data_;
  }

  size_t size() const noexcept {
    return data_ == nullptr ? 0 : static_cast<size_t>(end_ - data_);
  }

  size_t capacity() const noexcept {
    return data_ == nullptr ? 0 : static_cast<size_t>(capacityEnd_ - data_);
  }

 private:
  static T* allocate(size_t count) {
    T* result = new (std::nothrow) T[count];
    if (result == nullptr) {
      std::abort();
    }
    return result;
  }

  size_t growthCapacity(size_t minimum) const {
    const size_t currentSize = size();
    const size_t addedSize = minimum - currentSize;
    return currentSize + std::max(currentSize, addedSize);
  }

  static void copyValues(T* destination, const T* source, size_t count) noexcept {
    if (count > 0) {
      // The byte count is derived from the allocated element count.
      // NOLINTNEXTLINE(facebook-security-vulnerable-memcpy)
      std::memcpy(destination, source, count * sizeof(T));
    }
  }

  void setSize(size_t count) noexcept {
    end_ = count == 0 ? data_ : data_ + count;
  }

  void reallocatePreserving(size_t count) {
    const size_t currentSize = size();
    T* newData = allocate(count);
    copyValues(newData, data_, currentSize);
    delete[] data_;
    data_ = newData;
    end_ = data_ + currentSize;
    capacityEnd_ = data_ + count;
  }

  T* data_{};
  T* end_{};
  T* capacityEnd_{};
};

template <typename T>
class OverwriteFloor {
 public:
  using value_type = T;

  OverwriteFloor() = default;
  OverwriteFloor(const OverwriteFloor&) = delete;
  OverwriteFloor& operator=(const OverwriteFloor&) = delete;
  OverwriteFloor(OverwriteFloor&&) = delete;
  OverwriteFloor& operator=(OverwriteFloor&&) = delete;

  ~OverwriteFloor() {
    delete[] data_;
  }

  void reserve(size_t count) {
    data_ = new (std::nothrow) T[count];
    if (data_ == nullptr) {
      std::abort();
    }
    size_ = count;
  }

  T* data() noexcept {
    return data_;
  }

  size_t size() const noexcept {
    return size_;
  }

  size_t capacity() const noexcept {
    return size_;
  }

 private:
  T* data_{};
  size_t size_{};
};

template <typename T>
class HighWaterVector {
 public:
  using value_type = T;

  void reserve(size_t count) {
    storage_.reserve(count);
  }

  void clear() noexcept {
    size_ = 0;
  }

  void resizeForOverwrite(size_t count) {
    if (count > storage_.size()) {
      storage_.resize(count);
    }
    size_ = count;
  }

  void resizeWithInitialization(size_t count) {
    if (count > storage_.size()) {
      storage_.resize(count);
    } else if (count > size_) {
      std::fill_n(storage_.data() + size_, count - size_, T{});
    }
    size_ = count;
  }

  T* data() noexcept {
    return storage_.data();
  }

  size_t size() const noexcept {
    return size_;
  }

  size_t capacity() const noexcept {
    return storage_.capacity();
  }

 private:
  std::vector<T> storage_;
  size_t size_{};
};

struct Sample {
  double milliseconds{};
  uint64_t checksum{};
  size_t operationCount{};
  size_t allocationCount{};
  size_t allocatedBytes{};
  size_t peakCapacityBytes{};
  bool allocationMetricsObserved{};
};

template <typename Vector>
void resizeForOverwrite(Vector& values, size_t count);

template <typename T>
VRS_IOVECTOR_BENCHMARK_FORCE_INLINE void resizeForOverwrite(
    StandardVector<T>& values,
    size_t count) {
  values.clear();
  values.resize(count);
}

template <typename T>
VRS_IOVECTOR_BENCHMARK_FORCE_INLINE void resizeForOverwrite(
    vrs::helpers::IOVector<T>& values,
    size_t count) {
  values.resizeDiscardingWithoutInitialization(count);
}

template <typename T>
VRS_IOVECTOR_BENCHMARK_FORCE_INLINE void resizeForOverwrite(
    HighWaterVector<T>& values,
    size_t count) {
  values.resizeForOverwrite(count);
}

template <typename T>
VRS_IOVECTOR_BENCHMARK_FORCE_INLINE void resizeForOverwrite(
    DirectIOBuffer<T>& values,
    size_t count) {
  values.resizeForOverwrite(count);
}

template <typename T>
VRS_IOVECTOR_BENCHMARK_FORCE_INLINE void resizeForOverwrite(
    OverwriteFloor<T>& /*values*/,
    size_t /*count*/) {}

template <typename Vector>
void resizePreserving(Vector& values, size_t count);

template <typename T>
VRS_IOVECTOR_BENCHMARK_FORCE_INLINE void resizePreserving(StandardVector<T>& values, size_t count) {
  values.resize(count);
}

template <typename T>
VRS_IOVECTOR_BENCHMARK_FORCE_INLINE void resizePreserving(
    vrs::helpers::IOVector<T>& values,
    size_t count) {
  values.resizePreservingWithoutInitialization(count);
}

template <typename T>
VRS_IOVECTOR_BENCHMARK_FORCE_INLINE void resizePreserving(
    HighWaterVector<T>& values,
    size_t count) {
  values.resizeForOverwrite(count);
}

template <typename T>
VRS_IOVECTOR_BENCHMARK_FORCE_INLINE void resizePreserving(DirectIOBuffer<T>& values, size_t count) {
  values.resizePreserving(count);
}

template <typename Vector>
void benchmarkResizeInitialized(Vector& values, size_t count);

template <typename T>
VRS_IOVECTOR_BENCHMARK_FORCE_INLINE void benchmarkResizeInitialized(
    StandardVector<T>& values,
    size_t count) {
  values.clear();
  values.resize(count);
}

template <typename T>
VRS_IOVECTOR_BENCHMARK_FORCE_INLINE void benchmarkResizeInitialized(
    vrs::helpers::IOVector<T>& values,
    size_t count) {
  values.clear();
  values.resizePreservingWithInitialization(count);
}

template <typename T>
VRS_IOVECTOR_BENCHMARK_FORCE_INLINE void benchmarkResizeInitialized(
    HighWaterVector<T>& values,
    size_t count) {
  values.clear();
  values.resizeWithInitialization(count);
}

template <typename T>
VRS_IOVECTOR_BENCHMARK_FORCE_INLINE void benchmarkResizeInitialized(
    DirectIOBuffer<T>& values,
    size_t count) {
  values.clear();
  values.resizeWithInitialization(count);
}

template <typename T>
VRS_IOVECTOR_BENCHMARK_FORCE_INLINE void benchmarkResizeInitialized(
    OverwriteFloor<T>& /*values*/,
    size_t /*count*/) {
  std::abort();
}

template <typename Vector>
VRS_IOVECTOR_BENCHMARK_ISOLATED Sample measureReuse(size_t byteCount, bool initialized) {
  using T = typename Vector::value_type;
  const size_t elementCount = byteCount / sizeof(T);
  const size_t iterationCount = std::max<size_t>(32, kBytesPerReuseSample / byteCount);
  Vector values;
  values.reserve(elementCount);
  Sample sample;
  sample.allocationCount = 1;
  sample.allocatedBytes = values.capacity() * sizeof(T);
  sample.peakCapacityBytes = sample.allocatedBytes;
  resizeForOverwrite(values, elementCount);
  sample.checksum = overwriteAndChecksum(values.data(), values.size() * sizeof(T), 1);
  const auto start = Clock::now();
  for (size_t iteration = 0; iteration < iterationCount; ++iteration) {
    if (initialized) {
      benchmarkResizeInitialized(values, elementCount);
    } else {
      resizeForOverwrite(values, elementCount);
    }
    sample.checksum += overwriteAndChecksum(
        values.data(), values.size() * sizeof(T), static_cast<uint8_t>(iteration + 1));
  }
  const auto stop = Clock::now();
  sample.milliseconds = std::chrono::duration<double, std::milli>(stop - start).count();
  sample.operationCount = iterationCount;
  return sample;
}

template <typename Vector>
VRS_IOVECTOR_BENCHMARK_ISOLATED Sample measureRecycled(size_t byteCount) {
  using T = typename Vector::value_type;
  const size_t elementCount = byteCount / sizeof(T);
  const size_t iterationCount = std::max<size_t>(32, kBytesPerReuseSample / byteCount);
  Sample sample;
  sample.allocationMetricsObserved = true;
  const auto start = Clock::now();
  for (size_t iteration = 0; iteration < iterationCount; ++iteration) {
    Vector values;
    auto* const oldStorage = values.data();
    resizeForOverwrite(values, elementCount);
    if (values.data() != oldStorage) {
      ++sample.allocationCount;
      sample.allocatedBytes += values.capacity() * sizeof(T);
    }
    sample.peakCapacityBytes = std::max(sample.peakCapacityBytes, values.capacity() * sizeof(T));
    sample.checksum += overwriteAndChecksum(
        values.data(), values.size() * sizeof(T), static_cast<uint8_t>(iteration + 1));
  }
  const auto stop = Clock::now();
  sample.milliseconds = std::chrono::duration<double, std::milli>(stop - start).count();
  sample.operationCount = iterationCount;
  return sample;
}

template <typename Vector>
VRS_IOVECTOR_BENCHMARK_ISOLATED void
measureOneCold(std::vector<Vector>& buffers, size_t elementCount, size_t index, Sample& sample) {
  using T = typename Vector::value_type;
  const auto start = Clock::now();
  buffers.emplace_back();
  Vector& values = buffers.back();
  auto* const oldStorage = values.data();
  resizeForOverwrite(values, elementCount);
  if (values.data() != oldStorage) {
    ++sample.allocationCount;
    sample.allocatedBytes += values.capacity() * sizeof(T);
  }
  ++sample.operationCount;
  sample.peakCapacityBytes += values.capacity() * sizeof(T);
  sample.checksum += overwriteAndChecksum(
      values.data(), values.size() * sizeof(T), static_cast<uint8_t>(index + 1));
  const auto stop = Clock::now();
  sample.milliseconds += std::chrono::duration<double, std::milli>(stop - start).count();
}

template <typename Baseline, typename Candidate>
std::pair<Sample, Sample> measureColdPair(size_t byteCount) {
  using T = typename Baseline::value_type;
  static_assert(sizeof(T) == sizeof(typename Candidate::value_type));
  const size_t elementCount = byteCount / sizeof(T);
  std::vector<Baseline> baselineBuffers;
  std::vector<Candidate> candidateBuffers;
  baselineBuffers.reserve(kColdBufferCount);
  candidateBuffers.reserve(kColdBufferCount);
  Sample baseline;
  Sample candidate;
  baseline.allocationMetricsObserved = true;
  candidate.allocationMetricsObserved = true;
  for (size_t index = 0; index < kColdBufferCount; ++index) {
    if ((index & 1) == 0) {
      measureOneCold(baselineBuffers, elementCount, index, baseline);
      measureOneCold(candidateBuffers, elementCount, index, candidate);
    } else {
      measureOneCold(candidateBuffers, elementCount, index, candidate);
      measureOneCold(baselineBuffers, elementCount, index, baseline);
    }
  }
  return {baseline, candidate};
}

template <typename Vector>
VRS_IOVECTOR_BENCHMARK_ISOLATED Sample measurePreservingGrowth(size_t finalByteCount) {
  using T = typename Vector::value_type;
  constexpr size_t kStepBytes = 4 * 1024;
  Vector values;
  Sample sample;
  sample.allocationMetricsObserved = true;
  auto* previousStorage = values.data();
  const auto start = Clock::now();
  for (size_t byteCount = kStepBytes; byteCount <= finalByteCount; byteCount += kStepBytes) {
    const size_t oldByteCount = values.size() * sizeof(T);
    const size_t elementCount = byteCount / sizeof(T);
    resizePreserving(values, elementCount);
    if (values.data() != previousStorage) {
      ++sample.allocationCount;
      sample.allocatedBytes += values.capacity() * sizeof(T);
      previousStorage = values.data();
    }
    sample.peakCapacityBytes = std::max(sample.peakCapacityBytes, values.capacity() * sizeof(T));
    sample.checksum += overwriteAndChecksum(
        static_cast<uint8_t*>(static_cast<void*>(values.data())) + oldByteCount,
        byteCount - oldByteCount,
        static_cast<uint8_t>(byteCount / kStepBytes));
  }
  const auto stop = Clock::now();
  sample.milliseconds = std::chrono::duration<double, std::milli>(stop - start).count();
  sample.operationCount = finalByteCount / kStepBytes;
  return sample;
}

template <typename Vector>
VRS_IOVECTOR_BENCHMARK_ISOLATED Sample measureShrinkRegrow(size_t byteCount) {
  using T = typename Vector::value_type;
  const size_t elementCount = byteCount / sizeof(T);
  const size_t halfCount = elementCount / 2;
  const size_t iterationCount = std::max<size_t>(32, kBytesPerReuseSample / byteCount);
  Vector values;
  values.reserve(elementCount);
  resizeForOverwrite(values, elementCount);
  Sample sample;
  sample.allocationCount = 1;
  sample.allocatedBytes = values.capacity() * sizeof(T);
  sample.peakCapacityBytes = sample.allocatedBytes;
  sample.checksum = overwriteAndChecksum(values.data(), values.size() * sizeof(T), 1);
  const auto start = Clock::now();
  for (size_t iteration = 0; iteration < iterationCount; ++iteration) {
    resizePreserving(values, halfCount);
    resizePreserving(values, elementCount);
    sample.checksum += overwriteAndChecksum(
        static_cast<uint8_t*>(static_cast<void*>(values.data())) + halfCount * sizeof(T),
        (elementCount - halfCount) * sizeof(T),
        static_cast<uint8_t>(iteration + 1));
  }
  const auto stop = Clock::now();
  sample.milliseconds = std::chrono::duration<double, std::milli>(stop - start).count();
  sample.operationCount = iterationCount * 2;
  return sample;
}

template <typename Vector>
VRS_IOVECTOR_BENCHMARK_ISOLATED Sample measureRealisticDistribution(size_t /*byteCount*/) {
  using T = typename Vector::value_type;
  const size_t maxElementCount = kLargeBufferBytes / sizeof(T);
  Vector values;
  values.reserve(maxElementCount);
  Sample sample;
  sample.allocationCount = 1;
  sample.allocatedBytes = values.capacity() * sizeof(T);
  sample.peakCapacityBytes = sample.allocatedBytes;
  size_t processedBytes = 0;
  size_t iteration = 0;
  const auto start = Clock::now();
  while (processedBytes < kBytesPerReuseSample) {
    const size_t requestedBytes = kRealisticBufferBytes[iteration % kRealisticBufferBytes.size()];
    const size_t elementCount = requestedBytes / sizeof(T);
    resizeForOverwrite(values, elementCount);
    const size_t actualBytes = values.size() * sizeof(T);
    sample.checksum +=
        overwriteAndChecksum(values.data(), actualBytes, static_cast<uint8_t>(iteration + 1));
    processedBytes += actualBytes;
    ++iteration;
  }
  const auto stop = Clock::now();
  sample.milliseconds = std::chrono::duration<double, std::milli>(stop - start).count();
  sample.operationCount = iteration;
  return sample;
}

template <typename Vector>
VRS_IOVECTOR_BENCHMARK_ISOLATED Sample measureAppendReuse(size_t byteCount) {
  using T = typename Vector::value_type;
  const size_t elementCount = byteCount / sizeof(T);
  const size_t iterationCount = std::max<size_t>(16, kElementsPerAppendSample / elementCount);
  Vector values;
  values.reserve(elementCount);
  Sample sample;
  sample.allocationCount = 1;
  sample.allocatedBytes = values.capacity() * sizeof(T);
  sample.peakCapacityBytes = sample.allocatedBytes;
  const size_t warmupIterationCount = std::max<size_t>(1, kElementsPerAppendWarmup / elementCount);
  for (size_t iteration = 0; iteration < warmupIterationCount; ++iteration) {
    values.clear();
    for (size_t index = 0; index < elementCount; ++index) {
      values.push_back(static_cast<T>(index + iteration));
    }
    (void)overwriteAndChecksum(values.data(), values.size() * sizeof(T), 1);
  }
  const auto start = Clock::now();
  for (size_t iteration = 0; iteration < iterationCount; ++iteration) {
    values.clear();
    for (size_t index = 0; index < elementCount; ++index) {
      values.push_back(static_cast<T>(index + iteration));
    }
    sample.checksum += overwriteAndChecksum(values.data(), values.size() * sizeof(T), 1);
  }
  const auto stop = Clock::now();
  sample.milliseconds = std::chrono::duration<double, std::milli>(stop - start).count();
  sample.operationCount = iterationCount * elementCount;
  return sample;
}

template <typename Vector>
VRS_IOVECTOR_BENCHMARK_ISOLATED Sample measureEmplaceReuse(size_t byteCount) {
  using T = typename Vector::value_type;
  const size_t elementCount = byteCount / sizeof(T);
  const size_t iterationCount = std::max<size_t>(16, kElementsPerAppendSample / elementCount);
  Vector values;
  values.reserve(elementCount);
  Sample sample;
  sample.allocationCount = 1;
  sample.allocatedBytes = values.capacity() * sizeof(T);
  sample.peakCapacityBytes = sample.allocatedBytes;
  const size_t warmupIterationCount = std::max<size_t>(1, kElementsPerAppendWarmup / elementCount);
  for (size_t iteration = 0; iteration < warmupIterationCount; ++iteration) {
    values.clear();
    for (size_t index = 0; index < elementCount; ++index) {
      values.emplace_back(static_cast<T>(index + iteration));
    }
    (void)overwriteAndChecksum(values.data(), values.size() * sizeof(T), 1);
  }
  const auto start = Clock::now();
  for (size_t iteration = 0; iteration < iterationCount; ++iteration) {
    values.clear();
    for (size_t index = 0; index < elementCount; ++index) {
      values.emplace_back(static_cast<T>(index + iteration));
    }
    sample.checksum += overwriteAndChecksum(values.data(), values.size() * sizeof(T), 1);
  }
  const auto stop = Clock::now();
  sample.milliseconds = std::chrono::duration<double, std::milli>(stop - start).count();
  sample.operationCount = iterationCount * elementCount;
  return sample;
}

template <typename Vector>
VRS_IOVECTOR_BENCHMARK_ISOLATED Sample measureReserveGrowth(size_t byteCount) {
  using T = typename Vector::value_type;
  const size_t elementCount = byteCount / sizeof(T);
  const size_t initialElementCount = elementCount / 2;
  std::vector<Vector> buffers;
  buffers.reserve(kReserveBufferCount);
  Sample sample;
  sample.allocationMetricsObserved = true;
  for (size_t index = 0; index < kReserveBufferCount; ++index) {
    buffers.emplace_back();
    Vector& values = buffers.back();
    auto* const oldStorage = values.data();
    resizeForOverwrite(values, initialElementCount);
    sample.checksum += overwriteAndChecksum(
        values.data(), values.size() * sizeof(T), static_cast<uint8_t>(index + 1));
    if (values.data() != oldStorage) {
      ++sample.allocationCount;
      sample.allocatedBytes += values.capacity() * sizeof(T);
    }
  }
  const auto start = Clock::now();
  for (Vector& values : buffers) {
    auto* const oldStorage = values.data();
    values.reserve(elementCount);
    sample.checksum += sampleChecksum(values.data(), values.size() * sizeof(T));
    if (values.data() != oldStorage) {
      ++sample.allocationCount;
      sample.allocatedBytes += values.capacity() * sizeof(T);
    }
    sample.peakCapacityBytes += values.capacity() * sizeof(T);
  }
  const auto stop = Clock::now();
  sample.milliseconds = std::chrono::duration<double, std::milli>(stop - start).count();
  sample.operationCount = kReserveBufferCount;
  return sample;
}

double median(std::array<double, kSampleCount> samples) {
  std::sort(samples.begin(), samples.end());
  return (samples[samples.size() / 2 - 1] + samples[samples.size() / 2]) / 2;
}

void verifySample(const Sample& sample) {
  if (sample.checksum == 0 || sample.operationCount == 0 || sample.allocationCount == 0 ||
      sample.allocatedBytes == 0 || sample.peakCapacityBytes == 0) {
    fmt::print(stderr, "benchmark invariant failed\n");
    std::abort();
  }
}

void verifyEquivalentWork(const Sample& baseline, const Sample& candidate) {
  if (baseline.checksum != candidate.checksum ||
      baseline.operationCount != candidate.operationCount) {
    fmt::print(stderr, "benchmark alternatives performed different work\n");
    std::abort();
  }
}

template <typename Baseline, typename Candidate, typename Measure>
void compare(
    const char* workload,
    const char* type,
    size_t byteCount,
    size_t run,
    Measure measure) {
  std::array<double, kSampleCount> baselineTimes{};
  std::array<double, kSampleCount> candidateTimes{};
  Sample baseline;
  Sample candidate;
  for (size_t sample = 0; sample < kSampleCount; ++sample) {
    if ((sample & 1) == 0) {
      baseline = measure.template operator()<Baseline>(byteCount);
      candidate = measure.template operator()<Candidate>(byteCount);
    } else {
      candidate = measure.template operator()<Candidate>(byteCount);
      baseline = measure.template operator()<Baseline>(byteCount);
    }
    verifySample(baseline);
    verifySample(candidate);
    verifyEquivalentWork(baseline, candidate);
    baselineTimes[sample] = baseline.milliseconds;
    candidateTimes[sample] = candidate.milliseconds;
  }
  const double baselineMedian = median(baselineTimes);
  const double candidateMedian = median(candidateTimes);
  fmt::print(
      "{},{},{},{},{},{:.3f},{:.3f},{:.3f},{},{},{},{},{},{},{},{},{},{}\n",
      sBuildLabel,
      workload,
      type,
      byteCount,
      run,
      baselineMedian,
      candidateMedian,
      baselineMedian / candidateMedian,
      candidate.operationCount,
      baseline.allocationMetricsObserved,
      candidate.allocationMetricsObserved,
      baseline.allocationCount,
      candidate.allocationCount,
      baseline.allocatedBytes,
      candidate.allocatedBytes,
      baseline.peakCapacityBytes,
      candidate.peakCapacityBytes,
      static_cast<size_t>(candidate.checksum));
}

struct MeasureOverwriteReuse {
  template <typename Vector>
  Sample operator()(size_t byteCount) const {
    return measureReuse<Vector>(byteCount, false);
  }
};

struct MeasureInitializedReuse {
  template <typename Vector>
  Sample operator()(size_t byteCount) const {
    return measureReuse<Vector>(byteCount, true);
  }
};

struct MeasureRecycled {
  template <typename Vector>
  Sample operator()(size_t byteCount) const {
    return measureRecycled<Vector>(byteCount);
  }
};

struct MeasurePreservingGrowth {
  template <typename Vector>
  Sample operator()(size_t byteCount) const {
    return measurePreservingGrowth<Vector>(byteCount);
  }
};

struct MeasureShrinkRegrow {
  template <typename Vector>
  Sample operator()(size_t byteCount) const {
    return measureShrinkRegrow<Vector>(byteCount);
  }
};

struct MeasureRealisticDistribution {
  template <typename Vector>
  Sample operator()(size_t byteCount) const {
    return measureRealisticDistribution<Vector>(byteCount);
  }
};

struct MeasureAppendReuse {
  template <typename Vector>
  Sample operator()(size_t byteCount) const {
    return measureAppendReuse<Vector>(byteCount);
  }
};

struct MeasureReserveGrowth {
  template <typename Vector>
  Sample operator()(size_t byteCount) const {
    return measureReserveGrowth<Vector>(byteCount);
  }
};

struct MeasureEmplaceReuse {
  template <typename Vector>
  Sample operator()(size_t byteCount) const {
    return measureEmplaceReuse<Vector>(byteCount);
  }
};

template <typename Baseline, typename Candidate>
void compareCold(const char* workload, const char* type, size_t byteCount, size_t run) {
  std::array<double, kSampleCount> baselineTimes{};
  std::array<double, kSampleCount> candidateTimes{};
  Sample baseline;
  Sample candidate;
  for (size_t sample = 0; sample < kSampleCount; ++sample) {
    std::tie(baseline, candidate) = measureColdPair<Baseline, Candidate>(byteCount);
    verifySample(baseline);
    verifySample(candidate);
    verifyEquivalentWork(baseline, candidate);
    baselineTimes[sample] = baseline.milliseconds;
    candidateTimes[sample] = candidate.milliseconds;
  }
  const double baselineMedian = median(baselineTimes);
  const double candidateMedian = median(candidateTimes);
  fmt::print(
      "{},{},{},{},{},{:.3f},{:.3f},{:.3f},{},{},{},{},{},{},{},{},{},{}\n",
      sBuildLabel,
      workload,
      type,
      byteCount,
      run,
      baselineMedian,
      candidateMedian,
      baselineMedian / candidateMedian,
      candidate.operationCount,
      baseline.allocationMetricsObserved,
      candidate.allocationMetricsObserved,
      baseline.allocationCount,
      candidate.allocationCount,
      baseline.allocatedBytes,
      candidate.allocatedBytes,
      baseline.peakCapacityBytes,
      candidate.peakCapacityBytes,
      candidate.checksum);
}

template <typename T>
void runType(const char* type) {
  for (size_t run = 0; run < kIndependentRunCount; ++run) {
    compare<StandardVector<T>, vrs::helpers::IOVector<T>>(
        "reuse-overwrite", type, kSmallBufferBytes, run, MeasureOverwriteReuse{});
    compare<StandardVector<T>, vrs::helpers::IOVector<T>>(
        "reuse-overwrite", type, kLargeBufferBytes, run, MeasureOverwriteReuse{});
    compare<StandardVector<T>, vrs::helpers::IOVector<T>>(
        "reuse-initialized", type, kLargeBufferBytes, run, MeasureInitializedReuse{});
  }
}

template <typename T>
void runDirectType(const char* type) {
  for (size_t run = 0; run < kIndependentRunCount; ++run) {
    compare<DirectIOBuffer<T>, vrs::helpers::IOVector<T>>(
        "direct-reuse-overwrite", type, kSmallBufferBytes, run, MeasureOverwriteReuse{});
    compare<DirectIOBuffer<T>, vrs::helpers::IOVector<T>>(
        "direct-reuse-overwrite", type, kLargeBufferBytes, run, MeasureOverwriteReuse{});
    compare<DirectIOBuffer<T>, vrs::helpers::IOVector<T>>(
        "direct-reuse-initialized", type, kLargeBufferBytes, run, MeasureInitializedReuse{});
  }
}

void runAppendComparisons() {
  for (size_t run = 0; run < kIndependentRunCount; ++run) {
    compare<DirectIOBuffer<uint32_t>, vrs::helpers::IOVector<uint32_t>>(
        "direct-append-reuse", "uint32_t", kSmallBufferBytes, run, MeasureAppendReuse{});
    compare<StandardVector<uint32_t>, vrs::helpers::IOVector<uint32_t>>(
        "append-reuse", "uint32_t", kSmallBufferBytes, run, MeasureAppendReuse{});
    compare<DirectIOBuffer<uint32_t>, vrs::helpers::IOVector<uint32_t>>(
        "direct-emplace-reuse", "uint32_t", kSmallBufferBytes, run, MeasureEmplaceReuse{});
    compare<StandardVector<uint32_t>, vrs::helpers::IOVector<uint32_t>>(
        "emplace-reuse", "uint32_t", kSmallBufferBytes, run, MeasureEmplaceReuse{});
  }
}

#pragma pack(push, 1)

struct PackedRecord {
  uint32_t timestamp;
  uint16_t stream;
};

#pragma pack(pop)

struct NaturalRecord {
  uint32_t timestamp;
  uint16_t stream;
};

struct alignas(64) OverAlignedRecord {
  uint64_t values[8];
};

} // namespace

int main(int argc, char** argv) {
  if (argc > 1) {
    sBuildLabel = argv[1];
  }
  fmt::print(
      "build_label,workload,type,bytes,run,baseline_ms,iovector_ms,speedup,operations,baseline_allocation_metrics_observed,iovector_allocation_metrics_observed,baseline_allocations,iovector_allocations,baseline_allocated_bytes,iovector_allocated_bytes,baseline_peak_capacity_bytes,iovector_peak_capacity_bytes,checksum\n");
  runAppendComparisons();
  for (size_t run = 0; run < kIndependentRunCount; ++run) {
    compare<OverwriteFloor<uint8_t>, vrs::helpers::IOVector<uint8_t>>(
        "overwrite-floor", "uint8_t", kSmallBufferBytes, run, MeasureOverwriteReuse{});
    compare<OverwriteFloor<uint8_t>, vrs::helpers::IOVector<uint8_t>>(
        "overwrite-floor", "uint8_t", kLargeBufferBytes, run, MeasureOverwriteReuse{});
    compare<HighWaterVector<uint8_t>, vrs::helpers::IOVector<uint8_t>>(
        "high-water-control", "uint8_t", kLargeBufferBytes, run, MeasureOverwriteReuse{});
  }
  runType<uint8_t>("uint8_t");
  runType<uint32_t>("uint32_t");
  runType<PackedRecord>("packed-record");
  runType<NaturalRecord>("natural-record");
  runType<OverAlignedRecord>("over-aligned-record");
  runDirectType<uint8_t>("uint8_t");
  runDirectType<uint32_t>("uint32_t");
  runDirectType<PackedRecord>("packed-record");
  runDirectType<NaturalRecord>("natural-record");
  runDirectType<OverAlignedRecord>("over-aligned-record");
  for (size_t run = 0; run < kIndependentRunCount; ++run) {
    compare<StandardVector<UninitializedByte>, vrs::helpers::IOVector<uint8_t>>(
        "existing-uninitialized-byte", "uint8_t", kSmallBufferBytes, run, MeasureOverwriteReuse{});
    compare<StandardVector<UninitializedByte>, vrs::helpers::IOVector<uint8_t>>(
        "existing-uninitialized-byte", "uint8_t", kLargeBufferBytes, run, MeasureOverwriteReuse{});
    compare<StandardVector<uint8_t>, vrs::helpers::IOVector<uint8_t>>(
        "allocator-recycled", "uint8_t", kLargeBufferBytes, run, MeasureRecycled{});
    compare<StandardVector<UninitializedByte>, vrs::helpers::IOVector<uint8_t>>(
        "existing-uninitialized-byte-recycled",
        "uint8_t",
        kLargeBufferBytes,
        run,
        MeasureRecycled{});
    compare<DirectIOBuffer<uint8_t>, vrs::helpers::IOVector<uint8_t>>(
        "direct-allocator-recycled", "uint8_t", kLargeBufferBytes, run, MeasureRecycled{});
    compareCold<StandardVector<uint8_t>, vrs::helpers::IOVector<uint8_t>>(
        "true-cold", "uint8_t", kLargeBufferBytes, run);
    compareCold<StandardVector<UninitializedByte>, vrs::helpers::IOVector<uint8_t>>(
        "existing-uninitialized-byte-cold", "uint8_t", kLargeBufferBytes, run);
    compareCold<DirectIOBuffer<uint8_t>, vrs::helpers::IOVector<uint8_t>>(
        "direct-true-cold", "uint8_t", kLargeBufferBytes, run);
    compare<DirectIOBuffer<uint8_t>, vrs::helpers::IOVector<uint8_t>>(
        "direct-preserving-growth", "uint8_t", kLargeBufferBytes, run, MeasurePreservingGrowth{});
    compare<StandardVector<uint8_t>, vrs::helpers::IOVector<uint8_t>>(
        "preserving-growth", "uint8_t", kLargeBufferBytes, run, MeasurePreservingGrowth{});
    compare<DirectIOBuffer<uint8_t>, vrs::helpers::IOVector<uint8_t>>(
        "direct-shrink-regrow", "uint8_t", kLargeBufferBytes, run, MeasureShrinkRegrow{});
    compare<StandardVector<uint8_t>, vrs::helpers::IOVector<uint8_t>>(
        "shrink-regrow", "uint8_t", kLargeBufferBytes, run, MeasureShrinkRegrow{});
    compare<DirectIOBuffer<uint8_t>, vrs::helpers::IOVector<uint8_t>>(
        "direct-realistic-distribution",
        "uint8_t",
        kLargeBufferBytes,
        run,
        MeasureRealisticDistribution{});
    compare<StandardVector<uint8_t>, vrs::helpers::IOVector<uint8_t>>(
        "realistic-distribution",
        "uint8_t",
        kLargeBufferBytes,
        run,
        MeasureRealisticDistribution{});
    compare<DirectIOBuffer<uint8_t>, vrs::helpers::IOVector<uint8_t>>(
        "direct-reserve-growth", "uint8_t", kLargeBufferBytes, run, MeasureReserveGrowth{});
    compare<StandardVector<uint8_t>, vrs::helpers::IOVector<uint8_t>>(
        "reserve-growth", "uint8_t", kLargeBufferBytes, run, MeasureReserveGrowth{});
  }
  return 0;
}

#undef VRS_IOVECTOR_BENCHMARK_FORCE_INLINE
#undef VRS_IOVECTOR_BENCHMARK_ISOLATED
