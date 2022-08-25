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

#include <cstring>

#include <gtest/gtest.h>

#include <vrs/helpers/MemBuffer.h>

struct MemBufferTester : testing::Test, public ::testing::WithParamInterface<size_t> {};

using namespace std;
using namespace vrs;
using namespace vrs::helpers;

#define ADD(MB, PTR, SIZE) \
  MB.addData(PTR, SIZE);   \
  PTR += SIZE

#define ALLOCATE_ADD(MB, PTR, SIZE_ALLOC, SIZE_COPY)           \
  {                                                            \
    uint8_t* ptr_;                                             \
    size_t allocatedSize = MB.allocateSpace(ptr_, SIZE_ALLOC); \
    EXPECT_GE(allocatedSize, SIZE_ALLOC);                      \
    memcpy(ptr_, PTR, SIZE_COPY);                              \
    MB.addAllocatedSpace(SIZE_COPY);                           \
    PTR += SIZE_COPY;                                          \
  }

namespace {

const char kInput[] =
    "this is just some test buffer to play with so we can write it out to the"
    "buffer, then compare it with the results";
size_t kSize = sizeof(kInput);

} // namespace

TEST_P(MemBufferTester, memBufferTestP) {
  MemBuffer mb(GetParam());
  const char* ptr = kInput;
  ADD(mb, ptr, 5);
  ADD(mb, ptr, 2);
  ALLOCATE_ADD(mb, ptr, 2, 1);

  ASSERT_EQ(8, mb.getSize());

  ADD(mb, ptr, 7);
  ASSERT_EQ(15, mb.getSize());
  ALLOCATE_ADD(mb, ptr, 10, 3);
  ASSERT_EQ(18, mb.getSize());
  ALLOCATE_ADD(mb, ptr, 3, 2);
  ADD(mb, ptr, 47);
  ADD(mb, ptr, 1);
  ALLOCATE_ADD(mb, ptr, 5, 4);
  ALLOCATE_ADD(mb, ptr, 9, 9);
  ALLOCATE_ADD(mb, ptr, 8, 8);
  ALLOCATE_ADD(mb, ptr, 7, 7);

  // add whatever is left
  ADD(mb, ptr, kSize - (ptr - kInput));

  ASSERT_EQ(kSize, mb.getSize());

  // validate that the collected data is the same as the input
  vector<uint8_t> data;
  mb.getData(data);
  ASSERT_EQ(kSize, data.size());
  EXPECT_EQ(memcmp(data.data(), kInput, kSize), 0);
}

INSTANTIATE_TEST_SUITE_P(
    memBufferTest,
    MemBufferTester,
    ::testing::Values(1, 2, 3, 5, 7, 10, 15, 97, 200));
