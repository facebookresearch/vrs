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

#include <vector>

#include <gtest/gtest.h>

#include <vrs/utils/converters/Grey10PackedConverters.h>

using namespace std;
using namespace vrs::utils;

static void checkRevert(
    const vector<uint16_t>& grey10,
    const vector<uint8_t>& packed10,
    uint32_t width,
    uint32_t height) {
  // verify packed10 to grey16 goes back to starting point
  vector<uint16_t> grey16(width * height);
  EXPECT_TRUE(convertGrey10PackedToGrey16(
      grey16.data(),
      grey16.size() * 2,
      width * 2,
      packed10.data(),
      packed10.size(),
      width,
      height,
      width * 5 / 4));
  for (size_t k = 0; k < grey16.size(); ++k) {
    EXPECT_EQ(grey16[k] >> 6, grey10[k]);
  }

  // verify convertGrey10PackedToGrey8 behavior
  vector<uint8_t> grey8(width * height);
  EXPECT_TRUE(convertGrey10PackedToGrey8(
      grey8.data(),
      grey8.size(),
      width,
      packed10.data(),
      packed10.size(),
      width,
      height,
      width * 5 / 4));
  EXPECT_EQ(grey16.size(), grey8.size());

  for (size_t k = 0; k < grey16.size(); ++k) {
    EXPECT_EQ(static_cast<uint8_t>(grey16[k] >> 8), grey8[k]);
  }
}

TEST(Grey10PackedConvertersTest, allBitsSet) {
  uint32_t width = 4;
  uint32_t height = 2;
  vector<uint16_t> src(width * height, (1 << 10) - 1);
  vector<uint8_t> dst(height * width * 5 / 4);

  EXPECT_TRUE(convertGrey10ToGrey10Packed(
      dst.data(), dst.size(), 5, src.data(), src.size() * 2, width, height, width * 2));
  vector<uint8_t> dstExpected = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
  EXPECT_EQ(dst, dstExpected);

  checkRevert(src, dst, width, height);
}

TEST(Grey10PackedConvertersTest, lowBits) {
  uint32_t width = 4;
  uint32_t height = 2;
  vector<uint16_t> src(width * height, 0x03); // lower 2 bits clear for all pixels
  vector<uint8_t> dst(height * width * 5 / 4); // for each 4 input pixels, one more byte

  EXPECT_TRUE(convertGrey10ToGrey10Packed(
      dst.data(), dst.size(), 5, src.data(), src.size() * 2, width, height, width * 2));
  vector<uint8_t> dstExpected = {
      0b11, 0b1100, 0b110000, 0b11000000, 0, 0b11, 0b1100, 0b110000, 0b11000000, 0};
  EXPECT_EQ(dst, dstExpected);

  checkRevert(src, dst, width, height);
}

TEST(Grey10PackedConvertersTest, highBits) {
  uint32_t width = 4;
  uint32_t height = 2;
  vector<uint16_t> src(width * height, 0xff << 2); // 2 lower bits clear, 8 next set
  vector<uint8_t> dst(height * width * 5 / 4); // for each 4 input pixels, one more byte

  EXPECT_TRUE(convertGrey10ToGrey10Packed(
      dst.data(), dst.size(), 5, src.data(), src.size() * 2, width, height, width * 2));
  vector<uint8_t> dstExpected = {
      0b11111100,
      0b11110011,
      0b11001111,
      0b00111111,
      0b11111111,
      0b11111100,
      0b11110011,
      0b11001111,
      0b00111111,
      0b11111111,
  };
  EXPECT_EQ(dst, dstExpected);

  checkRevert(src, dst, width, height);
}

TEST(Grey10PackedConvertersTest, invertedBits) {
  uint32_t width = 4;
  uint32_t height = 2;
  vector<uint16_t> src{
      0b1010101010,
      0b0101010101,
      0b1010101010,
      0b0101010101,
      0b1010101010,
      0b0101010101,
      0b1010101010,
      0b0101010101,
  };
  vector<uint8_t> dst(height * width * 5 / 4); // for each 4 input pixels, one more byte

  EXPECT_TRUE(convertGrey10ToGrey10Packed(
      dst.data(), dst.size(), 5, src.data(), src.size() * 2, width, height, width * 2));
  vector<uint8_t> dstExpected = {
      0b10101010,
      0b01010110,
      0b10100101,
      0b01101010,
      0b01010101,
      0b10101010,
      0b01010110,
      0b10100101,
      0b01101010,
      0b01010101,
  };
  EXPECT_EQ(dst, dstExpected);

  checkRevert(src, dst, width, height);
}
