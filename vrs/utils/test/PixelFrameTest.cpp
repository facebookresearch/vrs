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

using namespace std;
using namespace vrs;
using namespace vrs::utils;

using coretech::getTestDataDir;

struct PixelFrameTest : testing::Test {
  string kTestFilePath = os::pathJoin(getTestDataDir(), "VRS_Files/sample_raw_pixel_formats.vrs");
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
