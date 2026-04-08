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

#include <memory>
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
  EXPECT_TRUE(checkNormalized({1, 2, 3, 4}, {0, 85, 170, 255}));
  EXPECT_TRUE(checkNormalized({-10, -100, 25, -2}, {183, 0, 255, 199}));
  EXPECT_TRUE(checkNormalized({NAN, -100, 25, -2}, {0, 0, 255, 199}));
  EXPECT_TRUE(checkNormalized({-10, -100, 25, NAN}, {183, 0, 255, 0}));
  EXPECT_TRUE(checkNormalized({NAN, NAN, 25, -2}, {0, 0, 255, 0}));
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
