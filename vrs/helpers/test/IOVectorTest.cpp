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
#include <cstddef>
#include <cstdint>
#include <limits>
#include <type_traits>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include <vrs/helpers/IOVector.h>

namespace vrs::helpers {
namespace {

#pragma pack(push, 1)

struct PackedRecord {
  uint32_t first;
  uint16_t second;
};

#pragma pack(pop)

struct NaturallyAlignedRecord {
  uint32_t first;
  uint16_t second;
};

struct alignas(64) OverAlignedRecord {
  uint64_t values[8];
};

struct NonTrivialDefault {
  uint32_t value{};
};

struct NonStandardLayoutBase {
  uint8_t first;
};

struct NonStandardLayout : NonStandardLayoutBase {
  uint8_t second;
};

template <typename T>
void copyAssign(T& destination, const T& source) {
  destination = source;
}

template <typename T, typename = void>
struct HasSingleArgumentResize : std::false_type {};

template <typename T>
struct HasSingleArgumentResize<
    T,
    std::void_t<decltype(std::declval<T&>().resize(std::declval<typename T::size_type>()))>>
    : std::true_type {};

template <typename T, typename = void>
struct HasValueResize : std::false_type {};

template <typename T>
struct HasValueResize<
    T,
    std::void_t<decltype(std::declval<T&>().resize(
        std::declval<typename T::size_type>(),
        std::declval<const typename T::value_type&>()))>> : std::true_type {};

template <typename T, typename = void>
struct HasGetAllocator : std::false_type {};

template <typename T>
struct HasGetAllocator<T, std::void_t<decltype(std::declval<T&>().get_allocator())>>
    : std::true_type {};

template <typename T, typename = void>
struct HasBegin : std::false_type {};

template <typename T>
struct HasBegin<T, std::void_t<decltype(std::declval<T&>().begin())>> : std::true_type {};

template <typename T, typename = void>
struct HasMemberSwap : std::false_type {};

template <typename T>
struct HasMemberSwap<T, std::void_t<decltype(std::declval<T&>().swap(std::declval<T&>()))>>
    : std::true_type {};

static_assert(isIOVectorCompatible<char>);
static_assert(isIOVectorCompatible<uint8_t>);
static_assert(isIOVectorCompatible<uint32_t>);
static_assert(isIOVectorCompatible<PackedRecord>);
static_assert(isIOVectorCompatible<NaturallyAlignedRecord>);
static_assert(isIOVectorCompatible<OverAlignedRecord>);
static_assert(!isIOVectorCompatible<const uint8_t>);
static_assert(!isIOVectorCompatible<volatile uint8_t>);
static_assert(!isIOVectorCompatible<uint8_t*>);
static_assert(!isIOVectorCompatible<uint8_t[4]>);
static_assert(!isIOVectorCompatible<NonTrivialDefault>);
static_assert(!isIOVectorCompatible<NonStandardLayout>);
static_assert(std::is_nothrow_move_assignable_v<IOVector<uint32_t>>);
static_assert(!std::is_base_of_v<std::vector<uint32_t>, IOVector<uint32_t>>);
static_assert(!std::is_constructible_v<IOVector<uint32_t>, size_t>);
static_assert(!std::is_constructible_v<IOVector<uint32_t>, size_t, uint32_t>);
static_assert(!HasSingleArgumentResize<IOVector<uint32_t>>::value);
static_assert(!HasValueResize<IOVector<uint32_t>>::value);
static_assert(!HasGetAllocator<IOVector<uint32_t>>::value);
static_assert(!HasBegin<IOVector<uint32_t>>::value);
static_assert(!HasMemberSwap<IOVector<uint32_t>>::value);

TEST(IOVectorTest, NewInitializedCreatesValueInitializedStorage) {
  auto values = IOVector<uint32_t>::newInitialized(3);
  EXPECT_EQ(values.size(), 3);
  EXPECT_EQ(values.capacity(), 3);
  const std::vector<uint32_t> expected{0, 0, 0};
  const std::vector<uint32_t> actual{values.data(), values.data() + values.size()};
  EXPECT_EQ(actual, expected);
}

TEST(IOVectorTest, NewUninitializedCreatesWritableStorage) {
  auto values = IOVector<uint32_t>::newUninitialized(3);
  EXPECT_EQ(values.size(), 3);
  EXPECT_EQ(values.capacity(), 3);
  const std::vector<uint32_t> expected{10, 11, 12};
  std::copy(expected.begin(), expected.end(), values.data());
  const std::vector<uint32_t> actual{values.data(), values.data() + values.size()};
  EXPECT_EQ(actual, expected);
}

TEST(IOVectorTest, ResizePreservingWithInitializationPreservesAndValueInitializes) {
  IOVector<uint32_t> values;
  values.emplace_back(42);
  auto* const oldStorage = values.data();
  const size_t newSize = values.capacity() + 3;
  values.resizePreservingWithInitialization(newSize);
  ASSERT_EQ(values.size(), newSize);
  EXPECT_NE(values.data(), oldStorage);
  EXPECT_EQ(values[0], 42);
  EXPECT_TRUE(std::all_of(values.data() + 1, values.data() + values.size(), [](uint32_t value) {
    return value == 0;
  }));
  values.resizePreservingWithInitialization(1);
  ASSERT_EQ(values.size(), 1);
  EXPECT_EQ(values[0], 42);
}

TEST(IOVectorTest, ZeroArgumentEmplacementRetainsVectorInitialization) {
  IOVector<uint32_t> values;
  uint32_t& value = values.emplace_back();
  EXPECT_EQ(value, 0);
  EXPECT_EQ(values.size(), 1);
}

TEST(IOVectorTest, ArgumentBearingEmplacementRetainsVectorInitialization) {
  IOVector<uint32_t> values;
  uint32_t& value = values.emplace_back(42);
  EXPECT_EQ(value, 42);
  EXPECT_EQ(values.size(), 1);
}

TEST(IOVectorTest, PushBackSupportsAValueFromTheSameAllocation) {
  IOVector<uint32_t> values;
  values.reserve(1);
  values.push_back(42);
  values.push_back(values[0]);
  ASSERT_EQ(values.size(), 2);
  EXPECT_EQ(values[0], 42);
  EXPECT_EQ(values[1], 42);
}

TEST(IOVectorTest, ValueInitializesStructuredElements) {
  IOVector<NaturallyAlignedRecord> values;
  values.resizePreservingWithInitialization(2);
  EXPECT_EQ(values[0].first, 0);
  EXPECT_EQ(values[0].second, 0);
  EXPECT_EQ(values[1].first, 0);
  EXPECT_EQ(values[1].second, 0);
}

TEST(IOVectorTest, DiscardResizeReusesCapacity) {
  IOVector<uint32_t> values;
  values.reserve(16);
  auto* const storage = values.data();
  values.resizePreservingWithInitialization(4);
  values.resizeDiscardingWithoutInitialization(12);
  EXPECT_EQ(values.data(), storage);
  EXPECT_EQ(values.size(), 12);
  EXPECT_GE(values.capacity(), 16);
  for (size_t index = 0; index < values.size(); ++index) {
    values[index] = static_cast<uint32_t>(index);
  }
}

TEST(IOVectorTest, DiscardResizeCanGrowWithoutRetainingOldStorage) {
  IOVector<uint32_t> values;
  values.resizePreservingWithInitialization(4);
  auto* const oldStorage = values.data();
  const size_t newSize = values.capacity() + 1;
  values.resizeDiscardingWithoutInitialization(newSize);
  EXPECT_NE(values.data(), oldStorage);
  EXPECT_EQ(values.size(), newSize);
  EXPECT_EQ(values.capacity(), newSize);
  for (size_t index = 0; index < values.size(); ++index) {
    values[index] = static_cast<uint32_t>(index);
  }
}

TEST(IOVectorTest, DiscardResizeHandlesEqualAndSmallerSizesWithoutReallocation) {
  IOVector<uint32_t> values;
  values.reserve(16);
  values.resizePreservingWithInitialization(8);
  auto* const storage = values.data();
  const size_t capacity = values.capacity();
  values.resizeDiscardingWithoutInitialization(8);
  EXPECT_EQ(values.data(), storage);
  EXPECT_EQ(values.size(), 8);
  EXPECT_EQ(values.capacity(), capacity);
  std::fill_n(values.data(), values.size(), 1);
  values.resizeDiscardingWithoutInitialization(3);
  EXPECT_EQ(values.data(), storage);
  EXPECT_EQ(values.size(), 3);
  EXPECT_EQ(values.capacity(), capacity);
  std::fill_n(values.data(), values.size(), 2);
}

TEST(IOVectorTest, ClearRetainsCapacityAndReleaseFreesIt) {
  IOVector<uint32_t> values;
  values.resizePreservingWithInitialization(16);
  const size_t capacity = values.capacity();
  values.clear();
  EXPECT_EQ(values.size(), 0);
  EXPECT_EQ(values.capacity(), capacity);
  values.release();
  EXPECT_EQ(values.size(), 0);
  EXPECT_EQ(values.capacity(), 0);
  EXPECT_EQ(values.data(), nullptr);
}

TEST(IOVectorDeathTest, SubscriptChecksBounds) {
  IOVector<uint32_t> values;
  EXPECT_DEATH((void)values[0], "");
}

TEST(IOVectorDeathTest, AtChecksBounds) {
  IOVector<uint32_t> values;
  EXPECT_DEATH((void)values.at(0), "");
}

TEST(IOVectorTest, ReservePreservesSizeAndValuesAcrossReallocation) {
  IOVector<uint32_t> values;
  values.push_back(41);
  values.push_back(42);
  auto* const oldStorage = values.data();
  const size_t newCapacity = values.capacity() + 16;
  values.reserve(newCapacity);
  ASSERT_EQ(values.size(), 2);
  EXPECT_GE(values.capacity(), newCapacity);
  EXPECT_NE(values.data(), oldStorage);
  EXPECT_EQ(values[0], 41);
  EXPECT_EQ(values[1], 42);
}

TEST(IOVectorTest, GrowthWithoutInitializationPreservesExistingValues) {
  IOVector<PackedRecord> values;
  values.resizePreservingWithoutInitialization(2);
  values[0] = PackedRecord{10, 11};
  values[1] = PackedRecord{20, 21};
  const size_t oldCapacity = values.capacity();
  values.resizePreservingWithoutInitialization(oldCapacity + 1);
  const uint32_t firstValue = values[0].first;
  const uint16_t firstTag = values[0].second;
  const uint32_t secondValue = values[1].first;
  const uint16_t secondTag = values[1].second;
  EXPECT_EQ(firstValue, 10);
  EXPECT_EQ(firstTag, 11);
  EXPECT_EQ(secondValue, 20);
  EXPECT_EQ(secondTag, 21);
}

TEST(IOVectorTest, HonorsElementAlignmentAcrossGrowth) {
  IOVector<OverAlignedRecord> values;
  values.resizePreservingWithoutInitialization(1);
  EXPECT_EQ(reinterpret_cast<uintptr_t>(values.data()) % alignof(OverAlignedRecord), 0);
  values[0].values[0] = 42;
  values.resizePreservingWithoutInitialization(1000);
  EXPECT_EQ(reinterpret_cast<uintptr_t>(values.data()) % alignof(OverAlignedRecord), 0);
  EXPECT_EQ(values[0].values[0], 42);
  IOVector<OverAlignedRecord> moved;
  auto* data = values.data();
  moved = std::move(values);
  EXPECT_EQ(moved.data(), data);
  EXPECT_EQ(reinterpret_cast<uintptr_t>(moved.data()) % alignof(OverAlignedRecord), 0);
  moved.resizePreservingWithoutInitialization(1);
  moved.shrink_to_fit();
  EXPECT_EQ(reinterpret_cast<uintptr_t>(moved.data()) % alignof(OverAlignedRecord), 0);
  EXPECT_EQ(moved[0].values[0], 42);
}

TEST(IOVectorTest, CopyConstructionCopiesLogicalElementsWithoutSpareCapacity) {
  IOVector<uint32_t> source;
  source.resizePreservingWithoutInitialization(3);
  source[0] = 10;
  source[1] = 11;
  source[2] = 12;
  IOVector<uint32_t> copy{source};
  EXPECT_NE(copy.data(), source.data());
  EXPECT_EQ(copy.capacity(), copy.size());
  const std::vector<uint32_t> expected{10, 11, 12};
  const std::vector<uint32_t> actual{copy.data(), copy.data() + copy.size()};
  EXPECT_EQ(actual, expected);
}

TEST(IOVectorTest, CopyAssignmentReplacesInsufficientStorage) {
  IOVector<uint32_t> source;
  source.resizePreservingWithoutInitialization(3);
  source[0] = 10;
  source[1] = 11;
  source[2] = 12;
  IOVector<uint32_t> assigned;
  assigned = source;
  EXPECT_NE(assigned.data(), source.data());
  const std::vector<uint32_t> expected{10, 11, 12};
  const std::vector<uint32_t> actual{assigned.data(), assigned.data() + assigned.size()};
  EXPECT_EQ(actual, expected);
}

TEST(IOVectorTest, CopyAssignmentReusesSufficientStorage) {
  IOVector<uint32_t> source;
  source.resizePreservingWithoutInitialization(3);
  source[0] = 10;
  source[1] = 11;
  source[2] = 12;
  IOVector<uint32_t> assigned;
  assigned.reserve(10);
  const uintptr_t assignedAddress = reinterpret_cast<uintptr_t>(assigned.data());
  assigned = source;
  EXPECT_EQ(reinterpret_cast<uintptr_t>(assigned.data()), assignedAddress);
  EXPECT_NE(assigned.data(), source.data());
  EXPECT_EQ(assigned.capacity(), 10);
  const std::vector<uint32_t> expected{10, 11, 12};
  const std::vector<uint32_t> actual{assigned.data(), assigned.data() + assigned.size()};
  EXPECT_EQ(actual, expected);
}

TEST(IOVectorTest, SelfCopyAssignmentPreservesElements) {
  IOVector<uint32_t> source;
  source.resizePreservingWithoutInitialization(3);
  source[0] = 10;
  source[1] = 11;
  source[2] = 12;
  copyAssign(source, source);
  const std::vector<uint32_t> expected{10, 11, 12};
  const std::vector<uint32_t> actual{source.data(), source.data() + source.size()};
  EXPECT_EQ(actual, expected);
}

TEST(IOVectorTest, MoveConstructionTransfersStorageAndResetsSource) {
  IOVector<uint32_t> source;
  source.resizePreservingWithoutInitialization(3);
  source[0] = 10;
  source[1] = 11;
  source[2] = 12;
  auto* const sourceData = source.data();
  IOVector<uint32_t> moved{std::move(source)};
  EXPECT_EQ(moved.data(), sourceData);
  const std::vector<uint32_t> expected{10, 11, 12};
  const std::vector<uint32_t> actual{moved.data(), moved.data() + moved.size()};
  EXPECT_EQ(actual, expected);
  EXPECT_EQ(source.size(), 0); // NOLINT(bugprone-use-after-move)
  EXPECT_EQ(source.capacity(), 0); // NOLINT(bugprone-use-after-move)
}

TEST(IOVectorTest, MoveAssignmentTransfersStorageAndResetsSource) {
  IOVector<uint32_t> source;
  source.resizePreservingWithInitialization(3);
  source[0] = 10;
  source[1] = 11;
  source[2] = 12;
  auto* const sourceData = source.data();
  IOVector<uint32_t> destination;
  destination.resizePreservingWithInitialization(1);
  destination[0] = 20;
  destination = std::move(source);
  EXPECT_EQ(destination.data(), sourceData);
  const std::vector<uint32_t> expected{10, 11, 12};
  const std::vector<uint32_t> actual{destination.data(), destination.data() + destination.size()};
  EXPECT_EQ(actual, expected);
  EXPECT_EQ(source.size(), 0); // NOLINT(bugprone-use-after-move)
  EXPECT_EQ(source.capacity(), 0); // NOLINT(bugprone-use-after-move)
}

TEST(IOVectorDeathTest, RejectsElementCountOverflow) {
  IOVector<OverAlignedRecord> values;
  EXPECT_DEATH(values.reserve(values.max_size() + 1), "");
}

TEST(IOVectorDeathTest, RejectsAllocationFailure) {
  IOVector<uint8_t> values;
  EXPECT_DEATH(values.reserve(values.max_size()), "");
}

} // namespace
} // namespace vrs::helpers
