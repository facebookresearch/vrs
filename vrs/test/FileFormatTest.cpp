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

#include <chrono>

#include <gtest/gtest.h>

#include <vrs/FileFormat.h>

using namespace std::chrono;
using namespace vrs::FileFormat;

namespace {

class FileFormatTest : public testing::Test {
 protected:
  void TearDown() override {
    FileHeader::setCreationIdOffset(0);
  }
};

TEST_F(FileFormatTest, CreationIdOffsetCanBeSetAndReset) {
  constexpr uint64_t kHourNs = 3'600'000'000'000;
  constexpr uint64_t kRandomBitsMask = (uint64_t{1} << 30) - 1;
  for (uint64_t offset : {uint64_t{0}, kHourNs, 2 * kHourNs, uint64_t{0}}) {
    FileHeader::setCreationIdOffset(offset);
    const uint64_t before =
        duration_cast<nanoseconds>(system_clock::now().time_since_epoch()).count();
    FileHeader header;
    header.init();
    const uint64_t after =
        duration_cast<nanoseconds>(system_clock::now().time_since_epoch()).count();
    EXPECT_GE(header.creationId & ~kRandomBitsMask, (before + offset) & ~kRandomBitsMask);
    EXPECT_LE(header.creationId & ~kRandomBitsMask, (after + offset) & ~kRandomBitsMask);
  }
}

} // namespace
