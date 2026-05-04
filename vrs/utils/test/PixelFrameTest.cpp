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

#include <cmath>
#include <memory>
#include <set>
#include <string>

#include <gtest/gtest.h>

#define DEFAULT_LOG_CHANNEL "PixelFrameTest"
#include <logging/Verify.h>

#include <TestDataDir/TestDataDir.h>

#include <vrs/RecordFileReader.h>
#include <vrs/RecordFormatStreamPlayer.h>
#include <vrs/os/Utils.h>
#include <vrs/utils/PixelFrame.h>
#include <vrs/utils/PixelFrameOptions.h>

#include <vrs/utils/PixelConversions.h>

using namespace std;
using namespace vrs;
using namespace vrs::utils;

using coretech::getTestDataDir;

struct PixelFrameTest : testing::Test {
  string kTestFilePath = os::pathJoin(getTestDataDir(), "VRS_Files/sample_raw_pixel_formats.vrs");
  string kJpegTestFilePath = os::pathJoin(getTestDataDir(), "computer_vision/eileen_128x128.jpg");
};

class ImagePlayer : public RecordFormatStreamPlayer {
  bool onImageRead(const CurrentRecord& record, size_t /*idx*/, const ContentBlock& cb) override {
    EXPECT_TRUE(PixelFrame::readFrame(frame_, record.reader, cb));
    PixelFrame::normalizeFrame(frame_, normalized_, true);
    PixelFormat format = normalized_->getPixelFormat();
    EXPECT_TRUE(
        format == PixelFormat::GREY8 || format == PixelFormat::GREY16 ||
        format == PixelFormat::RGB8 || format == PixelFormat::RGBA8);
    PixelFrame::normalizeFrame(frame_, normalized_, false);
    format = normalized_->getPixelFormat();
    EXPECT_TRUE(
        format == PixelFormat::GREY8 || format == PixelFormat::RGB8 ||
        format == PixelFormat::RGBA8);
    return true; // read next blocks, if any
  }

 private:
  shared_ptr<PixelFrame> frame_;
  shared_ptr<PixelFrame> normalized_;
};

TEST_F(PixelFrameTest, normalize) {
  RecordFileReader reader;
  ASSERT_EQ(reader.openFile(kTestFilePath), 0);
  vector<unique_ptr<StreamPlayer>> streamPlayers;
  for (auto id : reader.getStreams()) {
    streamPlayers.emplace_back(new ImagePlayer());
    reader.setStreamPlayer(id, streamPlayers.back().get());
  }
  reader.readAllRecords();
}

static bool checkNormalized(const vector<float>& floats, const vector<uint8_t>& ints) {
  PixelFrame pf(PixelFormat::DEPTH32F, floats.size(), 1);
  memcpy(pf.wdata(), floats.data(), floats.size() * sizeof(float));
  shared_ptr<PixelFrame> normalizedFrame;
  pf.normalizeFrame(normalizedFrame, false);
  vector<uint8_t> normalized(normalizedFrame->size());
  memcpy(normalized.data(), normalizedFrame->rdata(), normalizedFrame->size());
  EXPECT_EQ(normalized, ints);
  return normalized == ints;
}

TEST_F(PixelFrameTest, normalizeDepth) {
  // With < 10 valid positive samples, percentile normalization produces blank output.
  EXPECT_TRUE(checkNormalized({1, 2, 3, 4}, {0, 0, 0, 0}));
  EXPECT_TRUE(checkNormalized({-10, -100, 25, -2}, {0, 0, 0, 0}));
  EXPECT_TRUE(checkNormalized({NAN, -100, 25, -2}, {0, 0, 0, 0}));
  EXPECT_TRUE(checkNormalized({-10, -100, 25, NAN}, {0, 0, 0, 0}));
  EXPECT_TRUE(checkNormalized({NAN, NAN, 25, -2}, {0, 0, 0, 0}));
  EXPECT_TRUE(checkNormalized({NAN, NAN, NAN, NAN}, {0, 0, 0, 0}));
}

struct Triplet {
  uint8_t a, b, c;
  bool operator==(const Triplet& rhs) const {
    return a == rhs.a && b == rhs.b && c == rhs.c;
  }
};

template <>
void testing::internal::PrintTo<Triplet>(const Triplet& value, ::std::ostream* os) {
  *os << "{" << (int)value.a << ", " << (int)value.b << ", " << (int)value.c << "}";
}

struct TripletF {
  float a, b, c;
};

static bool checkNormalizedRGB32F(const vector<TripletF>& floats, const vector<Triplet>& ints) {
  if (!XR_VERIFY(floats.size() == ints.size())) {
    return false;
  }
  PixelFrame pf(PixelFormat::RGB32F, floats.size(), 1);
  memcpy(
      pf.wdata(), reinterpret_cast<const float*>(floats.data()), 3 * floats.size() * sizeof(float));
  shared_ptr<PixelFrame> normalizedFrame;
  pf.normalizeFrame(normalizedFrame, false);
  vector<Triplet> normalized(ints.size());
  const uint8_t* normalizedSrc = normalizedFrame->rdata();
  uint8_t* normalizedDst = reinterpret_cast<uint8_t*>(normalized.data());
  for (size_t k = 0; k < normalizedFrame->size(); k++) {
    normalizedDst[k] = normalizedSrc[k];
  }
  EXPECT_EQ(normalized, ints);
  return normalized == ints;
}

TEST_F(PixelFrameTest, normalizeRGB32F) {
  EXPECT_TRUE(checkNormalizedRGB32F({{1, 150, 3}, {10, 50, 100}}, {{0, 255, 0}, {255, 0, 255}}));
  EXPECT_TRUE(checkNormalizedRGB32F(
      {{1, 2, 3}, {10, -50, 100}, {5, 30, 150}}, {{0, 165, 0}, {255, 0, 168}, {113, 255, 255}}));
  EXPECT_TRUE(checkNormalizedRGB32F(
      {{1, NAN, NAN}, {10, -50, NAN}, {-5, 30, 250}, {25, NAN, 150}},
      {{51, 0, 0}, {127, 0, 0}, {0, 255, 255}, {255, 0, 0}}));
}

// This streamPlayer read image, writes it as png in a buffer, reads the buffer back and finally
// tests that the decoded PixelFrame is strictly identical from the raw data.
class PngImageWriteRead : public RecordFormatStreamPlayer {
  bool onImageRead(const CurrentRecord& record, size_t /*idx*/, const ContentBlock& cb) override {
    PixelFrame frame;
    EXPECT_TRUE(frame.readRawFrame(record.reader, cb.image()));
    vector<uint8_t> buffer;
    PixelFrame decoded;
    frame.writeAsPng("", &buffer);
    decoded.readPngFrame(buffer);
    EXPECT_TRUE(frame.hasSamePixels(decoded.getSpec()));
    EXPECT_EQ(frame.getBuffer(), decoded.getBuffer());
    return true; // read next blocks, if any
  }
};

TEST_F(PixelFrameTest, writeReadPng) {
  RecordFileReader reader;
  ASSERT_EQ(reader.openFile(kTestFilePath), 0);
  PngImageWriteRead streamPlayer;
  auto streamIdsPerfectlyConvertibleToPng = {
      vrs::StreamId::fromNumericName("100-1"), // grey8
      vrs::StreamId::fromNumericName("100-4"), // grey16
      vrs::StreamId::fromNumericName("214-2"), // rgb8
      vrs::StreamId::fromNumericName("214-4"), // rgba8
  };
  for (auto streamId : streamIdsPerfectlyConvertibleToPng) {
    reader.setStreamPlayer(streamId, &streamPlayer);
    reader.readAllRecords();
  }
}

using RGBColor = vrs::utils::PixelFrame::RGBColor;

inline bool equal(const RGBColor& left, const RGBColor& right) {
  return left.b == right.b && left.g == right.g && left.r == right.r;
}

TEST_F(PixelFrameTest, colorTableTest) {
  const vector<RGBColor>& colors = PixelFrame::getGetObjectIdSegmentationColors();
  ASSERT_GT(colors.size(), 0xffff);
  EXPECT_TRUE(equal(colors[0], RGBColor()));
  EXPECT_TRUE(equal(colors[0xffff], RGBColor(255, 255, 255)));
}

#if IS_VRS_FB_INTERNAL()

// Build a BAYER10_GBRG source with constant per-channel values and an explicit
// row stride. GBRG row-major layout:
//   even rows: G B G B ...
//   odd  rows: R G R G ...
// Each pixel is a uint16_t holding a 10-bit value (0..1023) in the low 10 bits.
static PixelFrame makeBayer10GbrgFrame(
    uint32_t width,
    uint32_t height,
    uint16_t green10,
    uint16_t blue10,
    uint16_t red10,
    uint32_t strideBytes = 0) {
  if (strideBytes == 0) {
    strideBytes = width * sizeof(uint16_t);
  }
  PixelFrame frame(PixelFormat::BAYER10_GBRG, width, height, strideBytes);
  uint8_t* dst = frame.wdata();
  for (uint32_t y = 0; y < height; ++y) {
    uint16_t* row = reinterpret_cast<uint16_t*>(dst + y * strideBytes);
    for (uint32_t x = 0; x < width; ++x) {
      const bool evenRow = (y % 2) == 0;
      const bool evenCol = (x % 2) == 0;
      row[x] = evenRow ? (evenCol ? green10 : blue10) : (evenCol ? red10 : green10);
    }
  }
  return frame;
}

TEST_F(PixelFrameTest, normalizeBayer10GbrgFastPath) {
  // 4x4 GBRG -> 2x2 RGB8 with speedOverPrecision=true.
  // 10-bit input is shifted right by 2 to produce 8-bit output.
  constexpr uint16_t kGreen10 = 200 << 2;
  constexpr uint16_t kBlue10 = 100 << 2;
  constexpr uint16_t kRed10 = 50 << 2;
  PixelFrame source = makeBayer10GbrgFrame(4, 4, kGreen10, kBlue10, kRed10);
  shared_ptr<PixelFrame> normalized;
  NormalizeOptions options(ImageSemantic::Camera);
  options.speedOverPrecision = true;
  ASSERT_TRUE(source.normalizeFrame(normalized, false, options));
  ASSERT_EQ(normalized->getPixelFormat(), PixelFormat::RGB8);
  ASSERT_EQ(normalized->getWidth(), 2u);
  ASSERT_EQ(normalized->getHeight(), 2u);
  const uint8_t* rgb = normalized->rdata();
  for (uint32_t i = 0; i < 4; ++i) {
    EXPECT_EQ(rgb[i * 3 + 0], 50) << "R at block " << i;
    EXPECT_EQ(rgb[i * 3 + 1], 200) << "G at block " << i;
    EXPECT_EQ(rgb[i * 3 + 2], 100) << "B at block " << i;
  }
}

TEST_F(PixelFrameTest, normalizeBayer10GbrgQualityPath) {
  // 8x8 GBRG -> 8x8 RGB8 with speedOverPrecision=false.
  // For constant per-channel input, interior bilinear-debayered pixels return
  // each channel at its constant value; border pixels are excluded from checks.
  constexpr uint16_t kGreen10 = 200 << 2;
  constexpr uint16_t kBlue10 = 100 << 2;
  constexpr uint16_t kRed10 = 50 << 2;
  PixelFrame source = makeBayer10GbrgFrame(8, 8, kGreen10, kBlue10, kRed10);
  shared_ptr<PixelFrame> normalized;
  NormalizeOptions options(ImageSemantic::Camera);
  options.speedOverPrecision = false;
  ASSERT_TRUE(source.normalizeFrame(normalized, false, options));
  ASSERT_EQ(normalized->getPixelFormat(), PixelFormat::RGB8);
  ASSERT_EQ(normalized->getWidth(), 8u);
  ASSERT_EQ(normalized->getHeight(), 8u);
  const uint8_t* rgb = normalized->rdata();
  const uint32_t stride = normalized->getStride();
  for (uint32_t y = 2; y < 6; ++y) {
    const uint8_t* row = rgb + y * stride;
    for (uint32_t x = 2; x < 6; ++x) {
      EXPECT_EQ(row[x * 3 + 0], 50) << "R at (" << x << "," << y << ")";
      EXPECT_EQ(row[x * 3 + 1], 200) << "G at (" << x << "," << y << ")";
      EXPECT_EQ(row[x * 3 + 2], 100) << "B at (" << x << "," << y << ")";
    }
  }
}

TEST_F(PixelFrameTest, normalizeBayer10GbrgFastPathRespectsStride) {
  // Regression test: with a padded source row stride, the handler must index
  // using getStride(), not width*sizeof(uint16_t). Padding bytes are filled
  // with sentinel values that would corrupt the output if read as pixels.
  constexpr uint32_t kWidth = 4;
  constexpr uint32_t kHeight = 4;
  constexpr uint32_t kPadPixels = 2;
  constexpr uint32_t kPaddedStride = (kWidth + kPadPixels) * sizeof(uint16_t);
  constexpr uint16_t kGreen10 = 200 << 2;
  constexpr uint16_t kBlue10 = 100 << 2;
  constexpr uint16_t kRed10 = 50 << 2;
  PixelFrame source =
      makeBayer10GbrgFrame(kWidth, kHeight, kGreen10, kBlue10, kRed10, kPaddedStride);
  // Sentinel padding values: max 10-bit value, distinct from any channel.
  uint8_t* dst = source.wdata();
  for (uint32_t y = 0; y < kHeight; ++y) {
    uint16_t* row = reinterpret_cast<uint16_t*>(dst + y * kPaddedStride);
    for (uint32_t x = kWidth; x < kWidth + kPadPixels; ++x) {
      row[x] = 0x3FF;
    }
  }
  shared_ptr<PixelFrame> normalized;
  NormalizeOptions options(ImageSemantic::Camera);
  options.speedOverPrecision = true;
  ASSERT_TRUE(source.normalizeFrame(normalized, false, options));
  ASSERT_EQ(normalized->getPixelFormat(), PixelFormat::RGB8);
  ASSERT_EQ(normalized->getWidth(), 2u);
  ASSERT_EQ(normalized->getHeight(), 2u);
  const uint8_t* rgb = normalized->rdata();
  for (uint32_t i = 0; i < 4; ++i) {
    EXPECT_EQ(rgb[i * 3 + 0], 50) << "R leaked padding at block " << i;
    EXPECT_EQ(rgb[i * 3 + 1], 200) << "G leaked padding at block " << i;
    EXPECT_EQ(rgb[i * 3 + 2], 100) << "B leaked padding at block " << i;
  }
}

TEST_F(PixelFrameTest, normalizeBayer10GbrgQualityPathRespectsStride) {
  // Regression test: quality path must also honor source stride. With padded
  // rows containing sentinel garbage, interior debayered pixels must still
  // resolve to the constant per-channel input values.
  constexpr uint32_t kWidth = 8;
  constexpr uint32_t kHeight = 8;
  constexpr uint32_t kPadPixels = 4;
  constexpr uint32_t kPaddedStride = (kWidth + kPadPixels) * sizeof(uint16_t);
  constexpr uint16_t kGreen10 = 200 << 2;
  constexpr uint16_t kBlue10 = 100 << 2;
  constexpr uint16_t kRed10 = 50 << 2;
  PixelFrame source =
      makeBayer10GbrgFrame(kWidth, kHeight, kGreen10, kBlue10, kRed10, kPaddedStride);
  uint8_t* dst = source.wdata();
  for (uint32_t y = 0; y < kHeight; ++y) {
    uint16_t* row = reinterpret_cast<uint16_t*>(dst + y * kPaddedStride);
    for (uint32_t x = kWidth; x < kWidth + kPadPixels; ++x) {
      row[x] = 0x3FF;
    }
  }
  shared_ptr<PixelFrame> normalized;
  NormalizeOptions options(ImageSemantic::Camera);
  options.speedOverPrecision = false;
  ASSERT_TRUE(source.normalizeFrame(normalized, false, options));
  ASSERT_EQ(normalized->getPixelFormat(), PixelFormat::RGB8);
  ASSERT_EQ(normalized->getWidth(), kWidth);
  ASSERT_EQ(normalized->getHeight(), kHeight);
  const uint8_t* rgb = normalized->rdata();
  const uint32_t stride = normalized->getStride();
  for (uint32_t y = 2; y < 6; ++y) {
    const uint8_t* row = rgb + y * stride;
    for (uint32_t x = 2; x < 6; ++x) {
      EXPECT_EQ(row[x * 3 + 0], 50) << "R leaked at (" << x << "," << y << ")";
      EXPECT_EQ(row[x * 3 + 1], 200) << "G leaked at (" << x << "," << y << ")";
      EXPECT_EQ(row[x * 3 + 2], 100) << "B leaked at (" << x << "," << y << ")";
    }
  }
}

TEST_F(PixelFrameTest, jpegTest) {
  PixelFrame frame;
  ASSERT_TRUE(frame.readJpegFrameFromFile(kJpegTestFilePath, true));
  EXPECT_EQ(frame.size(), 49152);
  EXPECT_EQ(frame.data<uint64_t>()[0], 4198000490383812399);
  EXPECT_EQ(frame.data<uint64_t>()[2500], 8221608522182776091);
  EXPECT_EQ(frame.data<uint64_t>()[49152 / 8 - 1], 1099511693312);
  PixelFrame frame2;
  ASSERT_TRUE(frame2.readJpegFrameFromFile(kJpegTestFilePath, false));
  EXPECT_EQ(frame2.size(), frame.size());
  ASSERT_EQ(frame.getSpec(), frame2.getSpec());
  frame.blankFrame();
  EXPECT_EQ(frame.getBuffer(), frame2.getBuffer());
}
#endif

/// Helper: create an RGBA8 PixelFrame filled with a known pattern.
/// Each pixel is {R=pixelIndex*3+1, G=pixelIndex*3+2, B=pixelIndex*3+3, A=0xFF}.
static PixelFrame makeRgbaFrame(uint32_t width, uint32_t height) {
  PixelFrame frame(PixelFormat::RGBA8, width, height);
  uint8_t* dst = frame.wdata();
  uint32_t stride = frame.getStride();
  uint8_t counter = 1;
  for (uint32_t h = 0; h < height; ++h) {
    uint8_t* row = dst + h * stride;
    for (uint32_t w = 0; w < width; ++w) {
      row[w * 4 + 0] = counter++;
      row[w * 4 + 1] = counter++;
      row[w * 4 + 2] = counter++;
      row[w * 4 + 3] = 0xFF; // alpha
    }
  }
  return frame;
}

/// Verify that an RGB8 frame contains the expected R,G,B values from makeRgbaFrame's pattern,
/// i.e., the alpha channel was dropped and the RGB values are preserved.
static void
verifyRgbFrame(const PixelFrame& rgbFrame, uint32_t expectedWidth, uint32_t expectedHeight) {
  ASSERT_EQ(rgbFrame.getPixelFormat(), PixelFormat::RGB8);
  ASSERT_EQ(rgbFrame.getWidth(), expectedWidth);
  ASSERT_EQ(rgbFrame.getHeight(), expectedHeight);
  const uint8_t* src = rgbFrame.rdata();
  uint32_t stride = rgbFrame.getStride();
  uint8_t counter = 1;
  for (uint32_t h = 0; h < expectedHeight; ++h) {
    const uint8_t* row = src + h * stride;
    for (uint32_t w = 0; w < expectedWidth; ++w) {
      EXPECT_EQ(row[w * 3 + 0], counter++) << "R mismatch at pixel (" << w << ", " << h << ")";
      EXPECT_EQ(row[w * 3 + 1], counter++) << "G mismatch at pixel (" << w << ", " << h << ")";
      EXPECT_EQ(row[w * 3 + 2], counter++) << "B mismatch at pixel (" << w << ", " << h << ")";
    }
  }
}

TEST_F(PixelFrameTest, inplaceRgbaToRgbBasic) {
  constexpr uint32_t kWidth = 16;
  constexpr uint32_t kHeight = 4;
  PixelFrame frame = makeRgbaFrame(kWidth, kHeight);
  ASSERT_EQ(frame.getPixelFormat(), PixelFormat::RGBA8);
  EXPECT_TRUE(frame.inplaceRgbaToRgb());
  verifyRgbFrame(frame, kWidth, kHeight);
}

TEST_F(PixelFrameTest, convertRgbaToRgbBasic) {
  constexpr uint32_t kWidth = 16;
  constexpr uint32_t kHeight = 4;
  PixelFrame frame = makeRgbaFrame(kWidth, kHeight);
  shared_ptr<PixelFrame> rgbFrame;
  EXPECT_TRUE(frame.convertRgbaToRgb(rgbFrame));
  ASSERT_NE(rgbFrame, nullptr);
  verifyRgbFrame(*rgbFrame, kWidth, kHeight);
  // Source frame is unchanged
  EXPECT_EQ(frame.getPixelFormat(), PixelFormat::RGBA8);
}

TEST_F(PixelFrameTest, rgbaToRgbRejectsNonRgba) {
  PixelFrame grey(PixelFormat::GREY8, 4, 4);
  EXPECT_FALSE(grey.inplaceRgbaToRgb());
  PixelFrame rgb(PixelFormat::RGB8, 4, 4);
  shared_ptr<PixelFrame> out;
  EXPECT_FALSE(rgb.convertRgbaToRgb(out));
}

// Test widths that exercise all code paths in convertRgbaToRgbRows:
// - 8-pixel vectorized loop
// - 2-pixel scalar cleanup
// - 1-pixel trailing cleanup
TEST_F(PixelFrameTest, rgbaToRgbOddWidths) {
  for (uint32_t width : {1, 2, 3, 7, 9, 15, 17}) {
    PixelFrame inplace = makeRgbaFrame(width, 2);
    EXPECT_TRUE(inplace.inplaceRgbaToRgb()) << "width=" << width;
    verifyRgbFrame(inplace, width, 2);
    PixelFrame source = makeRgbaFrame(width, 2);
    shared_ptr<PixelFrame> converted;
    EXPECT_TRUE(source.convertRgbaToRgb(converted)) << "width=" << width;
    verifyRgbFrame(*converted, width, 2);
  }
}

TEST_F(PixelFrameTest, rgbaToRgbInplaceAndConvertMatch) {
  constexpr uint32_t kWidth = 19;
  constexpr uint32_t kHeight = 5;
  PixelFrame inplace = makeRgbaFrame(kWidth, kHeight);
  PixelFrame source = makeRgbaFrame(kWidth, kHeight);
  ASSERT_TRUE(inplace.inplaceRgbaToRgb());
  shared_ptr<PixelFrame> converted;
  ASSERT_TRUE(source.convertRgbaToRgb(converted));
  ASSERT_EQ(inplace.getSpec(), converted->getSpec());
  EXPECT_EQ(inplace.getBuffer(), converted->getBuffer());
}

TEST_F(PixelFrameTest, resizeOptionsResizeFormats) {
  constexpr uint32_t kSourceWidth = 32;
  constexpr uint32_t kSourceHeight = 24;

  ResizeOptions resizeOptions = ResizeOptions::withRatio(0.5f);

  // Iterate over all pixel formats
  for (uint8_t i = 0; i < static_cast<uint8_t>(PixelFormat::COUNT); ++i) {
    PixelFormat format = static_cast<PixelFormat>(i);
    if (!ResizeOptions::canResize(format)) {
      continue;
    }

    PixelFrame sourceFrame(format, kSourceWidth, kSourceHeight);

    unique_ptr<PixelFrame> resizedFrame = resizeOptions.resize(sourceFrame);
    ASSERT_NE(resizedFrame, nullptr) << "resize() should return a valid PixelFrame";
    EXPECT_EQ(resizedFrame->getSpec().getWidth(), kSourceWidth / 2);
    EXPECT_EQ(resizedFrame->getSpec().getHeight(), kSourceHeight / 2);
    EXPECT_EQ(resizedFrame->getSpec().getPixelFormat(), format);
  }
}

// --- PixelConversions unit tests ---

TEST(PixelConversionsTest, convertBgr8ToRgb8_basic) {
  // 10 pixels: exercises 8-pixel unrolled path + 2-pixel remainder
  constexpr uint32_t kPixelCount = 10;
  vector<uint8_t> bgr(kPixelCount * 3);
  for (uint32_t i = 0; i < kPixelCount; ++i) {
    bgr[i * 3 + 0] = static_cast<uint8_t>(i * 3 + 1); // B
    bgr[i * 3 + 1] = static_cast<uint8_t>(i * 3 + 2); // G
    bgr[i * 3 + 2] = static_cast<uint8_t>(i * 3 + 3); // R
  }
  vector<uint8_t> rgb(kPixelCount * 3, 0);
  pixel_conversions::convertBgr8ToRgb8(bgr.data(), rgb.data(), kPixelCount);
  for (uint32_t i = 0; i < kPixelCount; ++i) {
    EXPECT_EQ(rgb[i * 3 + 0], bgr[i * 3 + 2]) << "R mismatch at pixel " << i;
    EXPECT_EQ(rgb[i * 3 + 1], bgr[i * 3 + 1]) << "G mismatch at pixel " << i;
    EXPECT_EQ(rgb[i * 3 + 2], bgr[i * 3 + 0]) << "B mismatch at pixel " << i;
  }
}

TEST(PixelConversionsTest, convertBgr8ToRgb8_oddCounts) {
  // Test all counts 0..9 to exercise both unrolled and remainder paths
  for (uint32_t count = 0; count <= 9; ++count) {
    vector<uint8_t> bgr(count * 3);
    for (uint32_t i = 0; i < count * 3; ++i) {
      bgr[i] = static_cast<uint8_t>(i + 1);
    }
    vector<uint8_t> rgb(count * 3, 0xFF);
    pixel_conversions::convertBgr8ToRgb8(bgr.data(), rgb.data(), count);
    for (uint32_t i = 0; i < count; ++i) {
      EXPECT_EQ(rgb[i * 3 + 0], bgr[i * 3 + 2]) << "count=" << count << " pixel=" << i;
      EXPECT_EQ(rgb[i * 3 + 1], bgr[i * 3 + 1]) << "count=" << count << " pixel=" << i;
      EXPECT_EQ(rgb[i * 3 + 2], bgr[i * 3 + 0]) << "count=" << count << " pixel=" << i;
    }
  }
}

TEST(PixelConversionsTest, convertRaw10ToGrey8_multipleOf4) {
  // 8 pixels = 2 groups of 4: 10 source bytes → 8 output bytes
  constexpr uint32_t kWidth = 8;
  constexpr uint32_t kHeight = 1;
  // RAW10 stride: 5 bytes per 4 pixels = ceil(8/4)*5 = 10
  constexpr size_t kSrcStride = 10;
  vector<uint8_t> raw10(kSrcStride * kHeight);
  // Group 0: MSB bytes 10,20,30,40; LSB byte ignored
  raw10[0] = 10;
  raw10[1] = 20;
  raw10[2] = 30;
  raw10[3] = 40;
  raw10[4] = 0xAA; // LSB byte — should be dropped
  // Group 1: MSB bytes 50,60,70,80
  raw10[5] = 50;
  raw10[6] = 60;
  raw10[7] = 70;
  raw10[8] = 80;
  raw10[9] = 0xBB; // LSB byte — should be dropped
  vector<uint8_t> grey(kWidth, 0);
  pixel_conversions::convertRaw10ToGrey8(
      raw10.data(), kSrcStride, grey.data(), kWidth, kWidth, kHeight);
  const vector<uint8_t> expected = {10, 20, 30, 40, 50, 60, 70, 80};
  EXPECT_EQ(grey, expected);
}

TEST(PixelConversionsTest, convertRaw10ToGrey8_nonMultipleOf4) {
  // 6 pixels = 1 full group (4) + 2 remainder
  constexpr uint32_t kWidth = 6;
  constexpr uint32_t kHeight = 1;
  // 1 full group (5 bytes) + 2 remainder bytes
  constexpr size_t kSrcStride = 7;
  vector<uint8_t> raw10(kSrcStride);
  raw10[0] = 11;
  raw10[1] = 22;
  raw10[2] = 33;
  raw10[3] = 44;
  raw10[4] = 0xFF; // LSB — dropped
  raw10[5] = 55; // remainder pixel 0
  raw10[6] = 66; // remainder pixel 1
  vector<uint8_t> grey(kWidth, 0);
  pixel_conversions::convertRaw10ToGrey8(
      raw10.data(), kSrcStride, grey.data(), kWidth, kWidth, kHeight);
  const vector<uint8_t> expected = {11, 22, 33, 44, 55, 66};
  EXPECT_EQ(grey, expected);
}

TEST(PixelConversionsTest, convertGreyToRgb8_basic) {
  constexpr uint32_t kWidth = 6; // exercises 4-pixel batch + 2 remainder
  constexpr uint32_t kHeight = 2;
  vector<uint8_t> grey(kWidth * kHeight);
  for (uint32_t i = 0; i < grey.size(); ++i) {
    grey[i] = static_cast<uint8_t>(i * 10);
  }
  vector<uint8_t> rgb(kWidth * 3 * kHeight, 0);
  pixel_conversions::convertGreyToRgb8(
      grey.data(), kWidth, rgb.data(), kWidth * 3, kWidth, kHeight);
  for (uint32_t h = 0; h < kHeight; ++h) {
    for (uint32_t w = 0; w < kWidth; ++w) {
      uint8_t g = grey[h * kWidth + w];
      size_t off = h * kWidth * 3 + w * 3;
      EXPECT_EQ(rgb[off + 0], g) << "R at (" << w << "," << h << ")";
      EXPECT_EQ(rgb[off + 1], g) << "G at (" << w << "," << h << ")";
      EXPECT_EQ(rgb[off + 2], g) << "B at (" << w << "," << h << ")";
    }
  }
}

TEST(PixelConversionsTest, convertYuy2ToRgb8_knownValues) {
  // YUY2 macro-pixel: Y0=128, U=128, Y1=128, V=128 → neutral grey
  // With the BT.601 formula used: c=128-16=112, d=0, e=0
  // R = clip((298*112 + 409*0 + 128) >> 8) = clip(130.5) = 130
  // G = clip((298*112 - 100*0 - 208*0 + 128) >> 8) = clip(130.5) = 130
  // B = clip((298*112 + 516*0 + 128) >> 8) = clip(130.5) = 130
  constexpr uint32_t kWidth = 2;
  constexpr uint32_t kHeight = 1;
  const uint8_t yuy2[] = {128, 128, 128, 128}; // Y0 U Y1 V
  uint8_t rgb[6] = {};
  pixel_conversions::convertYuy2ToRgb8(yuy2, 4, rgb, 6, kWidth, kHeight);
  EXPECT_EQ(rgb[0], 130); // R0
  EXPECT_EQ(rgb[1], 130); // G0
  EXPECT_EQ(rgb[2], 130); // B0
  EXPECT_EQ(rgb[3], 130); // R1
  EXPECT_EQ(rgb[4], 130); // G1
  EXPECT_EQ(rgb[5], 130); // B1
}

TEST(PixelConversionsTest, convertYuy2ToRgb8_unaligned) {
  // Same input as knownValues, but force a misaligned source pointer to exercise
  // the byte-read fallback path (non-uint32_t reads).
  constexpr uint32_t kWidth = 2;
  constexpr uint32_t kHeight = 1;
  alignas(4) uint8_t storage[8]; // force 4-byte alignment of the base
  uint8_t* yuy2 = storage + 1; // guaranteed misaligned (4n+1)
  yuy2[0] = 128; // Y0
  yuy2[1] = 128; // U
  yuy2[2] = 128; // Y1
  yuy2[3] = 128; // V
  uint8_t rgb[6] = {};
  pixel_conversions::convertYuy2ToRgb8(yuy2, 4, rgb, 6, kWidth, kHeight);
  EXPECT_EQ(rgb[0], 130);
  EXPECT_EQ(rgb[1], 130);
  EXPECT_EQ(rgb[2], 130);
  EXPECT_EQ(rgb[3], 130);
  EXPECT_EQ(rgb[4], 130);
  EXPECT_EQ(rgb[5], 130);
}

TEST(PixelConversionsTest, convertYuy2ToRgb8_clamping) {
  // Y0=0, U=0, Y1=255, V=255 → tests both low and high clamping
  constexpr uint32_t kWidth = 2;
  constexpr uint32_t kHeight = 1;
  const uint8_t yuy2[] = {0, 0, 255, 255};
  uint8_t rgb[6] = {};
  pixel_conversions::convertYuy2ToRgb8(yuy2, 4, rgb, 6, kWidth, kHeight);
  // Pixel 0: Y=0, U=0, V=255 → c=-16, d=-128, e=127
  // R = clip((298*(-16) + 409*127 + 128) >> 8) = clip((47215)>>8) = clip(184) = 184
  // G = clip((298*(-16) - 100*(-128) - 208*127 + 128) >> 8) = clip((-30960)>>8) = 0
  // B = clip((298*(-16) + 516*(-128) + 128) >> 8) = clip((-70784)>>8) = 0
  EXPECT_EQ(rgb[0], 184); // R0 - red from high V
  EXPECT_EQ(rgb[1], 0); // G0 - clamped low
  EXPECT_EQ(rgb[2], 0); // B0 - clamped low from low U
  // Pixel 1: Y=255, U=0, V=255 → c=239, d=-128, e=127
  // R = clip((298*239 + 409*127 + 128) >> 8) = clip(123293>>8) = 255
  // G = clip((298*239 - 100*(-128) - 208*127 + 128) >> 8) = clip(57734>>8) = 225
  // B = clip((298*239 + 516*(-128) + 128) >> 8) = clip(5302>>8) = 20
  EXPECT_EQ(rgb[3], 255); // R1 - clamped high
  EXPECT_EQ(rgb[4], 225); // G1
  EXPECT_EQ(rgb[5], 20); // B1
}

TEST(PixelConversionsTest, upscalePixels16) {
  // Upscale 10-bit (max 1023) to 16-bit by shifting left 6
  const vector<uint16_t> src = {0, 1, 512, 1023, 100};
  vector<uint16_t> dst(src.size(), 0);
  pixel_conversions::upscalePixels16(src.data(), dst.data(), static_cast<uint32_t>(src.size()), 6);
  for (size_t i = 0; i < src.size(); ++i) {
    EXPECT_EQ(dst[i], static_cast<uint16_t>(src[i] << 6)) << "index=" << i;
  }
}

TEST(PixelConversionsTest, downscalePixels16To8) {
  // Downscale 16-bit to 8-bit by shifting right 8
  const vector<uint16_t> src = {0, 255, 256, 65535, 32768, 1, 128};
  vector<uint8_t> dst(src.size(), 0);
  pixel_conversions::downscalePixels16To8(
      src.data(), dst.data(), static_cast<uint32_t>(src.size()), 8);
  for (size_t i = 0; i < src.size(); ++i) {
    EXPECT_EQ(dst[i], static_cast<uint8_t>((src[i] >> 8) & 0xFF)) << "index=" << i;
  }
}

TEST(PixelConversionsTest, downscalePixels16To8_variousShifts) {
  // Test different shift amounts (2, 4, 8) used for GREY10, GREY12, GREY16
  for (uint16_t shift : {2, 4, 8}) {
    const vector<uint16_t> src = {0, 1000, 4095, 65535};
    vector<uint8_t> dst(src.size(), 0xFF);
    pixel_conversions::downscalePixels16To8(
        src.data(), dst.data(), static_cast<uint32_t>(src.size()), shift);
    for (size_t i = 0; i < src.size(); ++i) {
      EXPECT_EQ(dst[i], static_cast<uint8_t>((src[i] >> shift) & 0xFF))
          << "shift=" << shift << " index=" << i;
    }
  }
}

TEST(R2Sampler2DTest, sampleCountClamped) {
  // maxSamples > pixelCount → clamped to pixelCount
  pixel_conversions::R2Sampler2D sampler(10, 10, 5000);
  EXPECT_EQ(sampler.sampleCount(), 100u);
}

TEST(R2Sampler2DTest, sampleCountRespected) {
  // maxSamples < pixelCount → uses maxSamples
  pixel_conversions::R2Sampler2D sampler(100, 100, 500);
  EXPECT_EQ(sampler.sampleCount(), 500u);
}

TEST(R2Sampler2DTest, indicesInBounds) {
  // All generated indices must be within [0, width*height)
  constexpr uint32_t W = 640, H = 480;
  for (uint32_t idx : pixel_conversions::R2Sampler2D(W, H, 5000)) {
    EXPECT_LT(idx, W * H);
  }
}

TEST(R2Sampler2DTest, indicesInBoundsWithStride) {
  // With stride > width, indices must be within [0, stride*height)
  constexpr uint32_t W = 100, H = 50, STRIDE = 128;
  for (uint32_t idx : pixel_conversions::R2Sampler2D(W, H, 2000, STRIDE)) {
    uint32_t row = idx / STRIDE;
    uint32_t col = idx % STRIDE;
    EXPECT_LT(row, H);
    EXPECT_LT(col, W);
  }
}

TEST(R2Sampler2DTest, deterministic) {
  // Same parameters always produce the same sequence
  constexpr uint32_t W = 320, H = 240, N = 1000;
  std::vector<uint32_t> run1, run2;
  for (uint32_t idx : pixel_conversions::R2Sampler2D(W, H, N)) {
    run1.push_back(idx);
  }
  for (uint32_t idx : pixel_conversions::R2Sampler2D(W, H, N)) {
    run2.push_back(idx);
  }
  EXPECT_EQ(run1, run2);
}

TEST(R2Sampler2DTest, coverage2D) {
  // For a moderately-sized 2D image, the R2 sequence should cover a good fraction of unique pixels
  constexpr uint32_t W = 50, H = 40;
  std::set<uint32_t> unique;
  for (uint32_t idx : pixel_conversions::R2Sampler2D(W, H, W * H)) {
    unique.insert(idx);
  }
  // Expect at least 70% unique coverage (R2 on 50x40 typically achieves ~80%)
  EXPECT_GT(unique.size(), static_cast<size_t>(W * H * 7 / 10));
}

TEST(R2Sampler2DTest, singlePixel) {
  pixel_conversions::R2Sampler2D sampler(1, 1, 100);
  EXPECT_EQ(sampler.sampleCount(), 1u);
  uint32_t count = 0;
  for (uint32_t idx : sampler) {
    EXPECT_EQ(idx, 0u);
    ++count;
  }
  EXPECT_EQ(count, 1u);
}

TEST(R2Sampler2DTest, strideDefaultsToWidth) {
  // Without explicit stride, indices should be the same as stride=width
  constexpr uint32_t W = 40, H = 30, N = 500;
  std::vector<uint32_t> defaultStride, explicitStride;
  for (uint32_t idx : pixel_conversions::R2Sampler2D(W, H, N)) {
    defaultStride.push_back(idx);
  }
  for (uint32_t idx : pixel_conversions::R2Sampler2D(W, H, N, W)) {
    explicitStride.push_back(idx);
  }
  EXPECT_EQ(defaultStride, explicitStride);
}

TEST(PixelConversionsTest, normalizeBuffer_floatRange) {
  // With < 10 valid positive samples, normalizeBuffer returns blank output.
  const float src[] = {0.0f, 0.5f, 1.0f, nanf("")};
  uint8_t dst[4] = {};
  pixel_conversions::normalizeBuffer<float>(reinterpret_cast<const uint8_t*>(src), dst, 4, 1);
  EXPECT_EQ(dst[0], 0);
  EXPECT_EQ(dst[1], 0);
  EXPECT_EQ(dst[2], 0);
  EXPECT_EQ(dst[3], 0);
}

TEST(PixelConversionsTest, normalizeBuffer_percentile) {
  // 20 positive values (4x5 image): percentile-based normalization with p5/p95 range.
  // R2 quasi-random sampling covers 16/20 unique values for this size.
  // p5idx=1 → p5=2, p95idx=19 → p95=19. factor = 255 / (19 - 2) = 15.
  constexpr uint32_t W = 4;
  constexpr uint32_t H = 5;
  constexpr uint32_t N = W * H;
  float src[N];
  for (uint32_t i = 0; i < N; ++i) {
    src[i] = static_cast<float>(i + 1);
  }
  uint8_t dst[N] = {};
  pixel_conversions::normalizeBuffer<float>(reinterpret_cast<const uint8_t*>(src), dst, W, H);
  EXPECT_EQ(dst[0], 0); // 1 <= p5(2) → clamped to 0
  EXPECT_EQ(dst[1], 0); // 2 <= p5(2) → clamped to 0
  EXPECT_EQ(dst[N - 1], 255); // 20 >= p95(19) → clamped to 255
  EXPECT_EQ(dst[N - 2], 255); // 19 >= p95(19) → clamped to 255
  // Values between p5 and p95 should be in (0, 255) exclusive.
  for (uint32_t i = 2; i < N - 2; ++i) {
    EXPECT_GT(dst[i], 0);
    EXPECT_LT(dst[i], 255);
  }
  // Monotonically increasing input → monotonically increasing output.
  for (uint32_t i = 2; i < N - 3; ++i) {
    EXPECT_LE(dst[i], dst[i + 1]);
  }
}

TEST(PixelConversionsTest, normalizeBuffer_constantInput) {
  // All same value → blank output
  const float src[] = {5.0f, 5.0f, 5.0f};
  uint8_t dst[3] = {0xFF, 0xFF, 0xFF};
  pixel_conversions::normalizeBuffer<float>(reinterpret_cast<const uint8_t*>(src), dst, 3, 1);
  EXPECT_EQ(dst[0], 0);
  EXPECT_EQ(dst[1], 0);
  EXPECT_EQ(dst[2], 0);
}

TEST(PixelConversionsTest, normalizeBufferWithRange_clamping) {
  const float src[] = {-1.0f, 0.0f, 0.5f, 1.0f, 2.0f, nanf("")};
  uint8_t dst[6] = {};
  pixel_conversions::normalizeBufferWithRange(
      reinterpret_cast<const uint8_t*>(src), dst, 6, 0.0f, 1.0f);
  EXPECT_EQ(dst[0], 0); // below min → 0
  EXPECT_EQ(dst[1], 0); // at min → 0
  EXPECT_EQ(dst[2], 127); // midpoint
  EXPECT_EQ(dst[3], 255); // at max → 255
  EXPECT_EQ(dst[4], 255); // above max → 255
  EXPECT_EQ(dst[5], 0); // NaN → 0
}

TEST(PixelConversionsTest, normalizeRGBXfloatToRGB8_uniform) {
  // 2 RGB32F pixels with uniform per-channel ranges: R=[0,1], G=[0,2], B=[0,4]
  const float src[] = {0.0f, 0.0f, 0.0f, 1.0f, 2.0f, 4.0f};
  uint8_t dst[6] = {};
  pixel_conversions::normalizeRGBXfloatToRGB8(reinterpret_cast<const uint8_t*>(src), dst, 2, 3);
  // Pixel 0: all at min → 0
  EXPECT_EQ(dst[0], 0);
  EXPECT_EQ(dst[1], 0);
  EXPECT_EQ(dst[2], 0);
  // Pixel 1: all at max → 255
  EXPECT_EQ(dst[3], 255);
  EXPECT_EQ(dst[4], 255);
  EXPECT_EQ(dst[5], 255);
}

TEST(PixelConversionsTest, normalizeBuffer_double) {
  // With < 10 valid positive samples, normalizeBuffer returns blank output.
  const double src[] = {-10.0, 0.0, 10.0};
  uint8_t dst[3] = {};
  pixel_conversions::normalizeBuffer<double>(reinterpret_cast<const uint8_t*>(src), dst, 3, 1);
  EXPECT_EQ(dst[0], 0);
  EXPECT_EQ(dst[1], 0);
  EXPECT_EQ(dst[2], 0);
}

TEST(PixelConversionsTest, normalizeBuffer_allNaN) {
  const float src[] = {nanf(""), nanf(""), nanf("")};
  uint8_t dst[3] = {0xFF, 0xFF, 0xFF};
  pixel_conversions::normalizeBuffer<float>(reinterpret_cast<const uint8_t*>(src), dst, 3, 1);
  // All NaN → min >= max (both 0), so memset to 0
  EXPECT_EQ(dst[0], 0);
  EXPECT_EQ(dst[1], 0);
  EXPECT_EQ(dst[2], 0);
}

TEST(PixelConversionsTest, normalizeRGBXfloatToRGB8_withNaN) {
  // 3 pixels with NaN in some channels. Need ≥2 distinct non-NaN values per channel for range.
  const float src[] = {
      0.0f,
      0.0f,
      0.0f, // pixel 0: all at min
      1.0f,
      2.0f,
      4.0f, // pixel 1: all at max
      nanf(""),
      nanf(""),
      nanf(""), // pixel 2: all NaN
  };
  uint8_t dst[9] = {};
  pixel_conversions::normalizeRGBXfloatToRGB8(reinterpret_cast<const uint8_t*>(src), dst, 3, 3);
  EXPECT_EQ(dst[0], 0); // R min
  EXPECT_EQ(dst[1], 0); // G min
  EXPECT_EQ(dst[2], 0); // B min
  EXPECT_EQ(dst[3], 255); // R max
  EXPECT_EQ(dst[4], 255); // G max
  EXPECT_EQ(dst[5], 255); // B max
  EXPECT_EQ(dst[6], 0); // R NaN → 0
  EXPECT_EQ(dst[7], 0); // G NaN → 0
  EXPECT_EQ(dst[8], 0); // B NaN → 0
}

TEST(PixelConversionsTest, normalizeRGBXfloatToRGB8_rgba32f) {
  // 2 RGBA32F pixels (channelCount=4), alpha channel should be skipped
  const float src[] = {0.0f, 0.0f, 0.0f, 99.0f, 1.0f, 2.0f, 4.0f, 99.0f};
  uint8_t dst[6] = {};
  pixel_conversions::normalizeRGBXfloatToRGB8(reinterpret_cast<const uint8_t*>(src), dst, 2, 4);
  EXPECT_EQ(dst[0], 0);
  EXPECT_EQ(dst[1], 0);
  EXPECT_EQ(dst[2], 0);
  EXPECT_EQ(dst[3], 255);
  EXPECT_EQ(dst[4], 255);
  EXPECT_EQ(dst[5], 255);
}
