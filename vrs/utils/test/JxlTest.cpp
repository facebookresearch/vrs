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

#include <gtest/gtest.h>

#define _USE_MATH_DEFINES // needed for M_PI definition on Windows
#include <cmath>
#include <vector>

#include <TestDataDir/TestDataDir.h>
#include <gtest/gtest.h>

#include <vrs/utils/PixelFrame.h>

/// This is a basic functionality test for JpegXL compression/decompression
/// It checks that Jpeg-XL compression and decompression are working correctly
/// by performing a round trip decompress(compress(uncompressed)) of a test image and
/// verifying that average pixel error is not too large.
///
///  Run using:
///   buck run @arvr/mode/XXX/opt //arvr/libraries/vrs/utils:unit_test_vrs_utils -- \
///     --gtest_filter=JxlPixelFormatTest/JxlVerificationTest.checkRoundTrip*

using namespace std;
using namespace vrs;
using namespace vrs::utils;

static constexpr int kImageWidth = 1280;
static constexpr int kImageHeight = 720;
static constexpr int kQuality = 95;

void fill_in_row_jahne_rgb8(
    uint8_t* row,
    int width,
    float center_col_index,
    float row_sq,
    float frequency_coefficient) {
  constexpr float kShift = 1.0f;
  constexpr float kMultiplier = 127.0f;
  uint8_t gray_value = 0;
  float centered_col = 0.0f;

  uint8_t* data = row;
  for (int col_index = 0; col_index < width; col_index++) {
    centered_col = col_index - center_col_index;
    gray_value = static_cast<uint8_t>(round(
        (sinf(frequency_coefficient * (row_sq + centered_col * centered_col)) + kShift) *
        kMultiplier));
    data[0] = gray_value;
    data[1] = gray_value;
    data[2] = gray_value;

    data += 3;
  }
}

void fill_in_row_jahne_grey8(
    uint8_t* row,
    int width,
    float center_col_index,
    float row_sq,
    float frequency_coefficient) {
  constexpr float kShift = 1.0f;
  constexpr float kMultiplier = 127.0f;
  uint8_t gray_value = 0;
  float centered_col = 0.0f;
  for (int col_index = 0; col_index < width; col_index++) {
    centered_col = col_index - center_col_index;
    gray_value = static_cast<uint8_t>(round(
        (sinf(frequency_coefficient * (row_sq + centered_col * centered_col)) + kShift) *
        kMultiplier));
    row[col_index] = gray_value;
  }
}

void fill_in_row_jahne_grey16(
    uint16_t* row,
    int width,
    float center_col_index,
    float row_sq,
    float frequency_coefficient) {
  constexpr float kShift = 1.0f;
  constexpr float kMultiplier = 32767.0f;
  uint16_t gray_value = 0;
  float centered_col = 0.0f;
  for (int col_index = 0; col_index < width; col_index++) {
    centered_col = col_index - center_col_index;
    gray_value = static_cast<uint16_t>(round(
        (sinf(frequency_coefficient * (row_sq + centered_col * centered_col)) + kShift) *
        kMultiplier));
    row[col_index] = gray_value;
  }
}

void FillInJahneTestPattern(PixelFrame& f) {
  float frequency_coefficient = (float)(M_PI) / (2.0f * f.getWidth());
  float center_row_index = f.getHeight() / 2.0f;
  float center_col_index = f.getWidth() / 2.0f;
  float centered_row = 0.0f;
  float row_sq = 0.0f;
  for (size_t r = 0; r < f.getHeight(); r++) {
    uint8_t* row = f.getLine(r);
    centered_row = r - center_row_index;
    row_sq = centered_row * centered_row;

    switch (f.getPixelFormat()) {
      case PixelFormat::RGB8:
        fill_in_row_jahne_rgb8(row, f.getWidth(), center_col_index, row_sq, frequency_coefficient);
        break;
      case PixelFormat::GREY8:
        fill_in_row_jahne_grey8(row, f.getWidth(), center_col_index, row_sq, frequency_coefficient);
        break;
      case PixelFormat::GREY16:
        fill_in_row_jahne_grey16(
            reinterpret_cast<uint16_t*>(row),
            f.getWidth(),
            center_col_index,
            row_sq,
            frequency_coefficient);
        break;
      default:
        FAIL() << "fillInTestData(): Unexpected pixel format";
    }
  }
}

struct SumRowAbsDiffNormRGB8 {
  static constexpr double Normalizer = 255 * 3.0;
  using ElementalType = uint8_t;
};
struct SumRowAbsDiffNormGREY8 {
  static constexpr double Normalizer = 255.0;
  using ElementalType = uint8_t;
};
struct SumRowAbsDiffNormGREY16 {
  static constexpr double Normalizer = 65535.0;
  using ElementalType = uint16_t;
};

template <typename T>
double sum_row_abs_diff_normalized(
    typename T::ElementalType* row_a,
    typename T::ElementalType* row_b,
    int width) {
  double res = 0;
  for (int c = 0; c < width; c++) {
    res += (abs((double)row_a[c] - row_b[c])) / T::Normalizer;
  }
  return res;
}

// Write into result pointer rather than returning a value
// because GoogleTest prevents you from failing in a function
// that doesn't return void
void ComputeMeanNoramlizedAbsDiff(double* result, PixelFrame& a, PixelFrame& b) {
  double sum = 0;
  for (size_t row = 0; row < b.getHeight(); row++) {
    uint8_t* a_row = a.getLine(row);
    uint8_t* b_row = b.getLine(row);

    switch (a.getPixelFormat()) {
      case PixelFormat::RGB8:
        sum += sum_row_abs_diff_normalized<SumRowAbsDiffNormRGB8>(a_row, b_row, 3 * a.getWidth());
        break;
      case PixelFormat::GREY8:
        sum += sum_row_abs_diff_normalized<SumRowAbsDiffNormGREY8>(a_row, b_row, a.getWidth());
        break;
      case PixelFormat::GREY16:
        sum += sum_row_abs_diff_normalized<SumRowAbsDiffNormGREY16>(
            reinterpret_cast<uint16_t*>(a_row), reinterpret_cast<uint16_t*>(b_row), a.getWidth());
        break;
      default:
        FAIL() << "fillInTestData(): Unexpected pixel format";
    }
  }

  *result = sum / (a.getWidth() * a.getHeight());
}

struct JxlVerificationTest : testing::Test, public ::testing::WithParamInterface<PixelFormat> {};

#ifdef JXL_IS_AVAILABLE
//#define WRITE_TEST_PATTERN
TEST_P(JxlVerificationTest, checkRoundTrip) {
  PixelFormat format = GetParam();

  PixelFrame uncompressed(format, kImageWidth, kImageHeight);

  FillInJahneTestPattern(uncompressed);
#ifdef WRITE_TEST_PATTERN
  uncompressed.writeAsPng("test_pattern.png");
#endif

  // Compress
  std::vector<uint8_t> jxl_bytes;
  EXPECT_TRUE(uncompressed.jxlCompress(jxl_bytes, kQuality));

  // Now decompress
  PixelFrame decompressed;
  EXPECT_TRUE(decompressed.readJxlFrame(jxl_bytes));
#ifdef WRITE_TEST_PATTERN
  decompressed.writeAsPng("test_pattern_decompressed.png");
#endif

  // Now compare
  EXPECT_EQ(decompressed.getHeight(), uncompressed.getHeight());
  EXPECT_EQ(decompressed.getWidth(), uncompressed.getWidth());
  EXPECT_EQ(decompressed.getPixelFormat(), uncompressed.getPixelFormat());

  double mean_norm_abs_diff = 0.0;
  ComputeMeanNoramlizedAbsDiff(&mean_norm_abs_diff, uncompressed, decompressed);
  EXPECT_LE(mean_norm_abs_diff, 0.05);
}

INSTANTIATE_TEST_SUITE_P(
    JxlPixelFormatTest,
    JxlVerificationTest,
    ::testing::Values<PixelFormat>(PixelFormat::RGB8, PixelFormat::GREY8, PixelFormat::GREY16));
#endif
