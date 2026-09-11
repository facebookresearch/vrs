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

#include <algorithm>
#include <cstddef>
#include <cstring>
#include <limits>
#include <memory>
#include <new>
#include <type_traits>
#include <utility>

#include <logging/Checks.h>

#if defined(_MSC_VER)
#define VRS_IOVECTOR_FORCE_INLINE __forceinline
#define VRS_IOVECTOR_LIKELY(condition) (condition)
#elif defined(__GNUC__)
#define VRS_IOVECTOR_FORCE_INLINE inline __attribute__((always_inline))
#define VRS_IOVECTOR_LIKELY(condition) __builtin_expect(static_cast<bool>(condition), true)
#else
#define VRS_IOVECTOR_FORCE_INLINE inline
#define VRS_IOVECTOR_LIKELY(condition) (condition)
#endif

namespace vrs::helpers {

/// True when T can safely use IOVector's no-value-initialization operations.
template <typename T>
inline constexpr bool isIOVectorCompatible =
    std::is_same_v<T, std::remove_cv_t<T>> && !std::is_pointer_v<T> && !std::is_array_v<T> &&
    std::is_standard_layout_v<T> && std::is_trivially_default_constructible_v<T> &&
    std::is_trivially_copyable_v<T> && std::is_trivially_destructible_v<T>;

/// A vector-shaped container optimized for storage and compression I/O.
///
/// IOVector provides explicit resizing operations so callers choose whether newly exposed elements
/// are value-initialized or left unwritten for an immediate I/O overwrite. Both preserving resize
/// operations leave every existing element in [0, min(old size, new size)) unchanged, including
/// when growing reallocates the storage. An element left unwritten by either no-initialization
/// resize must be overwritten before an operation evaluates its value. Bytewise relocation may
/// preserve unwritten object representations without evaluating them.
/// Size-taking constructors are replaced by factories that require an explicit initialization
/// choice, and an ambiguous resize() operation is omitted. Allocation honors alignof(T).
/// Thread-safety is the same as std::vector<T>: concurrent access follows the standard container
/// data-race rules, and callers must synchronize operations that modify shared state. Because T is
/// required to be trivially destructible, shrink, clear, release, and container destruction perform
/// no element destructor work.
template <typename T>
class IOVector final {
  static_assert(
      isIOVectorCompatible<T>,
      "IOVector requires an unqualified, non-pointer, non-array, standard-layout, trivially "
      "default-constructible, trivially copyable, and trivially destructible type");

 public:
  using value_type = T;
  using size_type = size_t;
  using difference_type = ptrdiff_t;
  using reference = T&;
  using const_reference = const T&;
  using pointer = T*;
  using const_pointer = const T*;

  IOVector() = default;

  ~IOVector() = default;

  /// Creates a vector containing count value-initialized elements, equivalent to default
  /// construction followed by resizePreservingWithInitialization(count).
  /// @param count: initial logical element count and capacity
  [[nodiscard]] static VRS_IOVECTOR_FORCE_INLINE IOVector newInitialized(size_type count) {
    IOVector result;
    result.resizePreservingWithInitialization(count);
    return result;
  }

  /// Creates a vector containing count uninitialized elements, equivalent to default construction
  /// followed by resizeDiscardingWithoutInitialization(count).
  /// Every element must be overwritten before an operation evaluates its value.
  /// @param count: initial logical element count and capacity
  [[nodiscard]] static VRS_IOVECTOR_FORCE_INLINE IOVector newUninitialized(size_type count) {
    IOVector result;
    result.resizeDiscardingWithoutInitialization(count);
    return result;
  }

  IOVector(const IOVector& other) {
    const size_type otherSize = other.size();
    if (otherSize > 0) {
      data_ = allocate(otherSize);
      copyValues(data_.get(), other.data_.get(), otherSize);
      end_ = data_.get() + otherSize;
      capacityEnd_ = end_;
    }
  }

  IOVector& operator=(const IOVector& other) {
    if (this != &other) {
      const size_type otherSize = other.size();
      if (otherSize > capacity()) {
        Storage newData = allocate(otherSize);
        copyValues(newData.get(), other.data_.get(), otherSize);
        data_ = std::move(newData);
        capacityEnd_ = data_.get() + otherSize;
      } else {
        copyValues(data_.get(), other.data_.get(), otherSize);
      }
      setSize(otherSize);
    }
    return *this;
  }

  IOVector(IOVector&& other) noexcept
      : data_{std::move(other.data_)},
        end_{std::exchange(other.end_, nullptr)},
        capacityEnd_{std::exchange(other.capacityEnd_, nullptr)} {}

  IOVector& operator=(IOVector&& other) noexcept {
    if (this != &other) {
      data_ = std::move(other.data_);
      end_ = std::exchange(other.end_, nullptr);
      capacityEnd_ = std::exchange(other.capacityEnd_, nullptr);
    }
    return *this;
  }

  /// Ensures capacity for at least count elements without changing size or values.
  /// @param count: minimum element capacity
  VRS_IOVECTOR_FORCE_INLINE void reserve(size_type count) {
    if (count > capacity()) {
      reallocatePreserving(count);
    }
  }

  /// Reduces capacity to size without changing size or values.
  VRS_IOVECTOR_FORCE_INLINE void shrink_to_fit() {
    const size_type currentSize = size();
    if (currentSize == 0) {
      release();
    } else if (currentSize < capacity()) {
      reallocatePreserving(currentSize);
    }
  }

  /// Changes size while leaving the existing prefix unchanged.
  /// Let oldSize be size() before the call. Every element in [0, min(oldSize, count)) retains its
  /// value and object representation, even if growing reallocates the storage. When count exceeds
  /// oldSize, each element in [oldSize, count) is assigned a value-initialized T.
  /// @param count: requested logical element count
  VRS_IOVECTOR_FORCE_INLINE void resizePreservingWithInitialization(size_type count) {
    const size_type oldSize = size();
    if (count <= oldSize) {
      setSize(count);
      return;
    }
    if (count > capacity()) {
      reallocatePreserving(growthCapacity(count));
    }
    std::fill_n(data_.get() + oldSize, count - oldSize, T{});
    setSize(count);
  }

  /// Changes size while leaving the existing prefix unchanged.
  /// Let oldSize be size() before the call. Every element in [0, min(oldSize, count)) retains its
  /// value and object representation, even if growing reallocates the storage. When count exceeds
  /// oldSize, elements in [oldSize, count) are left uninitialized.
  /// @param count: requested logical element count
  VRS_IOVECTOR_FORCE_INLINE void resizePreservingWithoutInitialization(size_type count) {
    if (count > capacity()) {
      reallocatePreserving(growthCapacity(count));
    }
    setSize(count);
  }

  /// Changes size without preserving previous values or initializing the new logical range.
  /// When allocation is required, capacity becomes exactly count rather than growing geometrically.
  /// @param count: requested logical element count
  VRS_IOVECTOR_FORCE_INLINE void resizeDiscardingWithoutInitialization(size_type count) {
    if (count > capacity()) {
      data_ = allocate(count);
      capacityEnd_ = data_.get() + count;
    }
    setSize(count);
  }

  /// Returns the contiguous element storage.
  VRS_IOVECTOR_FORCE_INLINE pointer data() noexcept {
    return data_.get();
  }

  /// Returns the contiguous element storage.
  VRS_IOVECTOR_FORCE_INLINE const_pointer data() const noexcept {
    return data_.get();
  }

  /// Returns the logical element count.
  VRS_IOVECTOR_FORCE_INLINE size_type size() const noexcept {
    return data_ == nullptr ? 0 : static_cast<size_type>(end_ - data_.get());
  }

  /// Returns the allocated element count.
  VRS_IOVECTOR_FORCE_INLINE size_type capacity() const noexcept {
    return data_ == nullptr ? 0 : static_cast<size_type>(capacityEnd_ - data_.get());
  }

  /// Returns whether the logical range is empty.
  VRS_IOVECTOR_FORCE_INLINE bool empty() const noexcept {
    return end_ == data_.get();
  }

  /// Returns the largest representable element count.
  size_type max_size() const noexcept {
    return maxElementCount();
  }

  /// Appends a copy of value.
  /// @param value: value to append; it may refer to an element of this IOVector
  VRS_IOVECTOR_FORCE_INLINE void push_back(const T& value) {
    (void)appendValue(value);
  }

  /// Appends a moved value.
  /// @param value: value to append; it may refer to an element of this IOVector
  VRS_IOVECTOR_FORCE_INLINE void push_back(T&& value) {
    (void)appendValue(std::move(value));
  }

  /// Constructs and appends an element.
  /// @param args: constructor arguments; no arguments value-initialize the element
  /// @return reference to the appended element
  template <typename... Args>
  VRS_IOVECTOR_FORCE_INLINE reference emplace_back(Args&&... args) {
    static_assert(noexcept(T(std::forward<Args>(args)...)), "IOVector emplacement must not throw");
    return appendValue(std::forward<Args>(args)...);
  }

  /// Removes all elements from the logical range while retaining capacity.
  VRS_IOVECTOR_FORCE_INLINE void clear() noexcept {
    end_ = data_.get();
  }

  /// Releases all allocated storage and resets size and capacity.
  void release() noexcept {
    data_.reset();
    end_ = nullptr;
    capacityEnd_ = nullptr;
  }

  /// Returns the element at index with a development-build bounds check.
  reference operator[](size_type index) noexcept {
    XR_DEV_CHECK_LT(index, size());
    return data_[index];
  }

  /// Returns the element at index with a development-build bounds check.
  const_reference operator[](size_type index) const noexcept {
    XR_DEV_CHECK_LT(index, size());
    return data_[index];
  }

  /// Returns the element at index, aborting through XR_CHECK when out of range.
  reference at(size_type index) noexcept {
    XR_CHECK_LT(index, size());
    return data_[index];
  }

  /// Returns the element at index, aborting through XR_CHECK when out of range.
  const_reference at(size_type index) const noexcept {
    XR_CHECK_LT(index, size());
    return data_[index];
  }

 private:
  using Storage = std::unique_ptr<T[]>;

  static constexpr size_type maxElementCount() noexcept {
    constexpr size_type kSizeLimit = std::numeric_limits<size_type>::max() / sizeof(T);
    constexpr size_type kDifferenceLimit =
        static_cast<size_type>(std::numeric_limits<difference_type>::max()) / sizeof(T);
    return kSizeLimit < kDifferenceLimit ? kSizeLimit : kDifferenceLimit;
  }

  size_type growthCapacity(size_type minimum) const {
    const size_type currentSize = size();
    XR_DEV_CHECK_LT(currentSize, minimum);
    XR_CHECK_LE(minimum, maxElementCount());
    const size_type addedSize = minimum - currentSize;
    const size_type growth = currentSize > addedSize ? currentSize : addedSize;
    if (currentSize > maxElementCount() - growth) {
      return maxElementCount();
    }
    return currentSize + growth;
  }

  static Storage allocate(size_type count) {
    XR_CHECK_LE(count, maxElementCount());
    if (count == 0) {
      return {};
    }
    // The absence of () begins T lifetimes without writing values.
    Storage storage{new (std::nothrow) T[count]};
    XR_CHECK_NOTNULL(storage.get());
    return storage;
  }

  static void copyValues(T* destination, const T* source, size_type count) noexcept {
    if (count > 0) {
      // A byte copy preserves object representations and never evaluates unwritten element values.
      std::memcpy(destination, source, count * sizeof(T));
    }
  }

  template <typename... Args>
  VRS_IOVECTOR_FORCE_INLINE reference appendValue(Args&&... args) {
    if (VRS_IOVECTOR_LIKELY(end_ != capacityEnd_)) {
      pointer insertion = end_;
      T* result = ::new (static_cast<void*>(insertion)) T(std::forward<Args>(args)...);
      end_ = insertion + 1;
      return *result;
    }
    return appendValueReallocating(std::forward<Args>(args)...);
  }

  template <typename... Args>
  VRS_IOVECTOR_FORCE_INLINE reference appendValueReallocating(Args&&... args) {
    const size_type oldSize = size();
    XR_CHECK_LT(oldSize, maxElementCount());
    const size_type newCapacity = growthCapacity(oldSize + 1);
    Storage newData = allocate(newCapacity);
    T* result = ::new (static_cast<void*>(newData.get() + oldSize)) T(std::forward<Args>(args)...);
    copyValues(newData.get(), data_.get(), oldSize);
    data_ = std::move(newData);
    end_ = data_.get() + oldSize + 1;
    capacityEnd_ = data_.get() + newCapacity;
    return *result;
  }

  VRS_IOVECTOR_FORCE_INLINE void setSize(size_type count) noexcept {
    end_ = count == 0 ? data_.get() : data_.get() + count;
  }

  VRS_IOVECTOR_FORCE_INLINE void reallocatePreserving(size_type newCapacity) {
    const size_type currentSize = size();
    Storage newData = allocate(newCapacity);
    copyValues(newData.get(), data_.get(), currentSize);
    data_ = std::move(newData);
    end_ = data_.get() + currentSize;
    capacityEnd_ = data_.get() + newCapacity;
  }

  Storage data_;
  pointer end_{};
  pointer capacityEnd_{};
};

} // namespace vrs::helpers

#undef VRS_IOVECTOR_FORCE_INLINE
#undef VRS_IOVECTOR_LIKELY
