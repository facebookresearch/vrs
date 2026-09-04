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

#include <vrs/AsyncDiskFileChunk.h>

#if VRS_ASYNC_DISKFILE_SUPPORTED()

#include <cstdint>
#include <cstring>
#include <memory>
#include <vector>

#include <gtest/gtest.h>

using namespace std;
using namespace vrs;

namespace {

constexpr size_t kMemAlign = 4 * 1024;
constexpr size_t kLenAlign = 512;
constexpr size_t kCapacity = 8 * 1024;

vector<uint8_t> makePayload(size_t size, uint8_t seed = 0) {
  vector<uint8_t> payload(size);
  for (size_t i = 0; i < size; ++i) {
    payload[i] = static_cast<uint8_t>((i * 31 + (i >> 8) * 17 + 7 + seed * 101) & 0xff);
  }
  return payload;
}

} // namespace

TEST(AlignedBufferTest, MakeReturnsEmptyBufferWithAlignedStorage) {
  const unique_ptr<AlignedBuffer> buffer = AlignedBuffer::make(kCapacity, kMemAlign, kLenAlign);
  ASSERT_NE(buffer, nullptr);
  EXPECT_EQ(buffer->capacity(), kCapacity);
  EXPECT_EQ(buffer->size(), size_t{0});
  EXPECT_TRUE(buffer->empty());
  EXPECT_FALSE(buffer->full());
  ASSERT_NE(buffer->data(), nullptr);
  EXPECT_EQ(reinterpret_cast<uintptr_t>(buffer->data()) % kMemAlign, uintptr_t{0});
}

TEST(AlignedBufferTest, AddCopiesPayloadAndAdvancesSize) {
  const unique_ptr<AlignedBuffer> buffer = AlignedBuffer::make(kCapacity, kMemAlign, kLenAlign);
  ASSERT_NE(buffer, nullptr);
  const vector<uint8_t> payload = makePayload(1000);
  size_t copied = 0;
  EXPECT_TRUE(buffer->add(payload.data(), payload.size(), copied));
  EXPECT_EQ(copied, size_t{1000});
  EXPECT_EQ(buffer->size(), size_t{1000});
  EXPECT_FALSE(buffer->full());
  EXPECT_EQ(memcmp(buffer->data(), payload.data(), payload.size()), 0);
}

TEST(AlignedBufferTest, AddClampsToRemainingCapacity) {
  const unique_ptr<AlignedBuffer> buffer = AlignedBuffer::make(kCapacity, kMemAlign, kLenAlign);
  ASSERT_NE(buffer, nullptr);
  const vector<uint8_t> payload = makePayload(kCapacity + 1000);
  size_t copied = 0;
  EXPECT_TRUE(buffer->add(payload.data(), payload.size(), copied));
  EXPECT_EQ(copied, kCapacity);
  EXPECT_EQ(buffer->size(), kCapacity);
  EXPECT_TRUE(buffer->full());
  EXPECT_EQ(memcmp(buffer->data(), payload.data(), kCapacity), 0);
}

TEST(AlignedBufferTest, AddAppendsAtTheCurrentSizeAndClampsToWhatIsLeft) {
  const unique_ptr<AlignedBuffer> buffer = AlignedBuffer::make(kCapacity, kMemAlign, kLenAlign);
  ASSERT_NE(buffer, nullptr);
  const vector<uint8_t> head = makePayload(1000, 1);
  size_t copied = 0;
  ASSERT_TRUE(buffer->add(head.data(), head.size(), copied));
  ASSERT_EQ(copied, size_t{1000});
  const vector<uint8_t> tail = makePayload(kCapacity, 2);
  EXPECT_TRUE(buffer->add(tail.data(), tail.size(), copied));
  EXPECT_EQ(copied, kCapacity - 1000);
  EXPECT_EQ(buffer->size(), kCapacity);
  EXPECT_EQ(memcmp(buffer->data(), head.data(), head.size()), 0);
  EXPECT_EQ(memcmp(buffer->bdata() + head.size(), tail.data(), kCapacity - head.size()), 0);
}

TEST(AlignedBufferTest, AddOnFullBufferCopiesNothing) {
  const unique_ptr<AlignedBuffer> buffer = AlignedBuffer::make(kCapacity, kMemAlign, kLenAlign);
  ASSERT_NE(buffer, nullptr);
  const vector<uint8_t> payload = makePayload(kCapacity);
  size_t copied = 0;
  ASSERT_TRUE(buffer->add(payload.data(), payload.size(), copied));
  ASSERT_TRUE(buffer->full());
  copied = 12345;
  EXPECT_TRUE(buffer->add(payload.data(), 1, copied));
  EXPECT_EQ(copied, size_t{0});
  EXPECT_EQ(buffer->size(), kCapacity);
}

TEST(AlignedBufferTest, AddOfNothingIsANoOp) {
  const unique_ptr<AlignedBuffer> buffer = AlignedBuffer::make(kCapacity, kMemAlign, kLenAlign);
  ASSERT_NE(buffer, nullptr);
  const vector<uint8_t> payload = makePayload(16);
  size_t copied = 12345;
  EXPECT_TRUE(buffer->add(payload.data(), 0, copied));
  EXPECT_EQ(copied, size_t{0});
  EXPECT_EQ(buffer->size(), size_t{0});
}

TEST(AlignedBufferTest, AddWithoutStorageFails) {
  const unique_ptr<AlignedBuffer> buffer = AlignedBuffer::make(kCapacity, kMemAlign, kLenAlign);
  ASSERT_NE(buffer, nullptr);
  buffer->free();
  ASSERT_EQ(buffer->capacity(), size_t{0});
  const vector<uint8_t> payload = makePayload(16);
  size_t copied = 12345;
  EXPECT_FALSE(buffer->add(payload.data(), payload.size(), copied));
  EXPECT_EQ(copied, size_t{0});
}

TEST(AlignedBufferTest, ClearMakesBufferReusable) {
  const unique_ptr<AlignedBuffer> buffer = AlignedBuffer::make(kCapacity, kMemAlign, kLenAlign);
  ASSERT_NE(buffer, nullptr);
  const vector<uint8_t> payload = makePayload(kCapacity);
  size_t copied = 0;
  ASSERT_TRUE(buffer->add(payload.data(), payload.size(), copied));
  buffer->clear();
  EXPECT_EQ(buffer->size(), size_t{0});
  EXPECT_TRUE(buffer->empty());
  EXPECT_EQ(buffer->capacity(), kCapacity);
  EXPECT_TRUE(buffer->add(payload.data(), payload.size(), copied));
  EXPECT_EQ(copied, kCapacity);
}

TEST(AlignedBufferDeathTest, CapacityNotMultipleOfLenAlignAborts) {
  EXPECT_DEATH(
      (void)AlignedBuffer::make(kCapacity + 1, kMemAlign, kLenAlign), "not a multiple of lenalign");
}

TEST(AlignedBufferTest, ZeroLenAlignSkipsTheCapacityConstraint) {
  const unique_ptr<AlignedBuffer> buffer = AlignedBuffer::make(kCapacity + 1, kMemAlign, 0);
  ASSERT_NE(buffer, nullptr);
  EXPECT_EQ(buffer->capacity(), kCapacity + 1);
}

#endif // VRS_ASYNC_DISKFILE_SUPPORTED()
