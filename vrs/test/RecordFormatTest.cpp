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

#define DEFAULT_LOG_CHANNEL "RecordFormatTest"
#include <logging/Verify.h>

#include <vrs/DataPieces.h>
#include <vrs/Record.h>
#include <vrs/RecordFormat.h>
#include <vrs/TagConventions.h>
#include <vrs/helpers/EnumStringConverter.h>

using namespace vrs;
using vrs::RecordFormat;
using namespace std;

namespace {

struct RecordFormatTest : testing::Test {};

class FixedLayout : public AutoDataLayout {
 public:
  DataPieceValue<double> time_{"time"};
  DataPieceArray<int32_t> ints_{"ints", 10};

  AutoDataLayoutEnd end;
};

class VarLayout : public AutoDataLayout {
 public:
  DataPieceValue<double> time_{"time"};
  DataPieceArray<int32_t> ints_{"ints", 10};
  DataPieceVector<int32_t> moreInts_{"ints"};
  DataPieceVector<string> strings{"strings"};

  AutoDataLayoutEnd end;
};

class TestRecordable : public Recordable {
 public:
  TestRecordable() : Recordable(RecordableTypeId::UnitTest1) {}
  const Record* createConfigurationRecord() override {
    return nullptr;
  }
  const Record* createStateRecord() override {
    return nullptr;
  }
};

} // namespace

#define FORMAT_EQUAL(_block_format, _cstring) \
  EXPECT_STREQ((_block_format).asString().c_str(), _cstring)

bool checkImageDimensions(
    const ContentBlock& cb,
    uint32_t width,
    uint32_t height,
    uint32_t rawStride,
    uint32_t planeStride0,
    uint32_t rawStride2 = 0,
    uint32_t planeStride1 = 0,
    uint32_t planeStride2 = 0,
    uint32_t planeStride3 = 0) {
  const ImageContentBlockSpec& spec = cb.image();
  uint32_t planeCount = 0;
  for (uint32_t plane = 0; plane <= 4; ++plane) {
    if (spec.getPlaneHeight(plane) != 0 || spec.getPlaneStride(plane) != 0) {
      planeCount++;
    }
    EXPECT_EQ(spec.getPlaneHeight(plane) != 0, spec.getPlaneStride(plane) != 0);
  }
  // if dimensions aren't set, bypass the test below
  if (spec.getWidth() == 0 && spec.getHeight() == 0) {
    planeCount = spec.getPlaneCount();
  }
  return XR_VERIFY(spec.getWidth() == width) && XR_VERIFY(spec.getHeight() == height) &&
      XR_VERIFY(spec.getRawStride() == rawStride) && XR_VERIFY(spec.getStride() == planeStride0) &&
      XR_VERIFY(spec.getPlaneStride(0) == planeStride0) &&
      XR_VERIFY(spec.getRawStride2() == rawStride2) &&
      XR_VERIFY(spec.getPlaneStride(1) == planeStride1) &&
      XR_VERIFY(spec.getPlaneStride(2) == planeStride2) &&
      XR_VERIFY(spec.getPlaneStride(3) == planeStride3) && XR_VERIFY(spec.getPlaneStride(4) == 0) &&
      XR_VERIFY(spec.getPlaneCount() == planeCount) && XR_VERIFY(spec.sanityCheckStrides());
}

bool checkImageHeights(
    const ContentBlock& cb,
    uint32_t height0,
    uint32_t height1 = 0,
    uint32_t height2 = 0,
    uint32_t height3 = 0) {
  const ImageContentBlockSpec& spec = cb.image();
  return XR_VERIFY(spec.getPlaneHeight(0) == height0) &&
      XR_VERIFY(spec.getPlaneHeight(1) == height1) &&
      XR_VERIFY(spec.getPlaneHeight(2) == height2) &&
      XR_VERIFY(spec.getPlaneHeight(3) == height3) && XR_VERIFY(spec.getPlaneHeight(4) == 0);
}

TEST_F(RecordFormatTest, testBlockFormat) {
  ContentBlock emptyString("");
  EXPECT_EQ(emptyString.getContentType(), ContentType::CUSTOM);

  ContentBlock png("image/png");
  EXPECT_EQ(png.getContentType(), ContentType::IMAGE);
  EXPECT_EQ(png.image().getImageFormat(), ImageFormat::PNG);

  ContentBlock jpg("image/jpg");
  EXPECT_EQ(jpg.getContentType(), ContentType::IMAGE);
  EXPECT_EQ(jpg.image().getImageFormat(), ImageFormat::JPG);

  ContentBlock jxl("image/jxl");
  EXPECT_EQ(jxl.getContentType(), ContentType::IMAGE);
  EXPECT_EQ(jxl.image().getImageFormat(), ImageFormat::JXL);

  ContentBlock weird("image/weird");
  EXPECT_EQ(weird.getContentType(), ContentType::IMAGE);
  EXPECT_EQ(weird.image().getImageFormat(), ImageFormat::UNDEFINED);

  ContentBlock raw("image/raw");
  EXPECT_EQ(raw.getContentType(), ContentType::IMAGE);
  EXPECT_EQ(raw.image().getImageFormat(), ImageFormat::RAW);
  EXPECT_EQ(raw.image().getPixelFormat(), PixelFormat::UNDEFINED);
  EXPECT_EQ(raw.image().getWidth(), 0);
  EXPECT_EQ(raw.image().getHeight(), 0);
  EXPECT_EQ(raw.image().getStride(), 0);

  ContentBlock classic("image/raw/640x480/pixel=grey8/stride=648");
  EXPECT_EQ(classic.getContentType(), ContentType::IMAGE);
  EXPECT_EQ(classic.image().getImageFormat(), ImageFormat::RAW);
  EXPECT_EQ(classic.image().getPixelFormat(), PixelFormat::GREY8);
  EXPECT_TRUE(checkImageDimensions(classic, 640, 480, 648, 648));
  EXPECT_TRUE(checkImageHeights(classic, 480));
  EXPECT_EQ(classic.image().getBytesPerPixel(), 1);
  EXPECT_EQ(classic.image().getChannelCountPerPixel(), 1);

  ContentBlock classicManual(PixelFormat::GREY8, 640, 480, 648);
  EXPECT_EQ(classic, classicManual);
  EXPECT_TRUE(checkImageDimensions(classicManual, 640, 480, 648, 648));
  EXPECT_TRUE(checkImageHeights(classicManual, 480));

  EXPECT_EQ(ContentBlock("image/raw/10x20/pixel=grey8"), ContentBlock(PixelFormat::GREY8, 10, 20));
  EXPECT_EQ(
      ContentBlock("image/raw/100x120/pixel=grey8/stride=105"),
      ContentBlock(PixelFormat::GREY8, 100, 120, 105));

  EXPECT_EQ(
      ContentBlock("image/raw/10x20/pixel=depth32f"), ContentBlock(PixelFormat::DEPTH32F, 10, 20));

  ContentBlock yuvSplit("image/raw/640x480/pixel=yuv_i420_split");
  EXPECT_EQ(yuvSplit.getContentType(), ContentType::IMAGE);
  EXPECT_EQ(yuvSplit.image().getImageFormat(), ImageFormat::RAW);
  EXPECT_EQ(yuvSplit.image().getPixelFormat(), PixelFormat::YUV_I420_SPLIT);
  EXPECT_TRUE(checkImageDimensions(yuvSplit, 640, 480, 0, 640, 0, 320, 320));
  EXPECT_TRUE(checkImageHeights(yuvSplit, 480, 240, 240));
  EXPECT_EQ(yuvSplit.image().getBytesPerPixel(), ContentBlock::kSizeUnknown);
  EXPECT_EQ(yuvSplit.image().getChannelCountPerPixel(), 3);
  EXPECT_EQ(yuvSplit.image().getBlockSize(), 640 * 480 + 2 * 320 * 240);
  EXPECT_EQ(yuvSplit.image().getPlaneCount(), 3);
  EXPECT_EQ(yuvSplit.image().getPlaneHeight(0), 480);
  EXPECT_EQ(yuvSplit.image().getPlaneHeight(1), 240);
  EXPECT_EQ(yuvSplit.image().getPlaneHeight(2), 240);
  EXPECT_EQ(yuvSplit.image().getPlaneHeight(3), 0);

  // A single stride doesn't make much sense for this format, but we'll accept it anyway.
  ContentBlock yuvSplit2("image/raw/640x480/pixel=yuv_i420_split/stride=640");
  EXPECT_EQ(yuvSplit2.getContentType(), ContentType::IMAGE);
  EXPECT_EQ(yuvSplit2.image().getImageFormat(), ImageFormat::RAW);
  EXPECT_EQ(yuvSplit2.image().getPixelFormat(), PixelFormat::YUV_I420_SPLIT);
  EXPECT_TRUE(checkImageDimensions(yuvSplit2, 640, 480, 640, 640, 0, 320, 320));
  EXPECT_TRUE(checkImageHeights(yuvSplit2, 480, 240, 240));
  EXPECT_EQ(yuvSplit2.image().getBlockSize(), 640 * 480 + 2 * 320 * 240);

  // A single stride doesn't make much sense for this format, but we'll accept it anyway.
  ContentBlock yuvSplit3("image/raw/640x480/pixel=yuv_i420_split/stride=650");
  EXPECT_EQ(yuvSplit3.getContentType(), ContentType::IMAGE);
  EXPECT_EQ(yuvSplit3.image().getImageFormat(), ImageFormat::RAW);
  EXPECT_EQ(yuvSplit3.image().getPixelFormat(), PixelFormat::YUV_I420_SPLIT);
  EXPECT_TRUE(checkImageDimensions(yuvSplit3, 640, 480, 650, 650, 0, 320, 320));
  EXPECT_TRUE(checkImageHeights(yuvSplit3, 480, 240, 240));
  EXPECT_EQ(yuvSplit3.image().getBlockSize(), 650 * 480 + 2 * 320 * 240);

  ContentBlock yuvSplit4("image/raw/642x480/pixel=yuv_i420_split");
  EXPECT_EQ(yuvSplit4.getContentType(), ContentType::IMAGE);
  EXPECT_EQ(yuvSplit4.image().getImageFormat(), ImageFormat::RAW);
  EXPECT_EQ(yuvSplit4.image().getPixelFormat(), PixelFormat::YUV_I420_SPLIT);
  EXPECT_TRUE(checkImageDimensions(yuvSplit4, 642, 480, 0, 642, 0, 321, 321));
  EXPECT_TRUE(checkImageHeights(yuvSplit4, 480, 240, 240));
  EXPECT_EQ(yuvSplit4.image().getBlockSize(), 642 * 480 + 2 * 321 * 240);

  ContentBlock yuvSplit4b("image/raw/642x480/pixel=yuv_i420_split/stride_2=340");
  EXPECT_EQ(yuvSplit4b.getContentType(), ContentType::IMAGE);
  EXPECT_EQ(yuvSplit4b.image().getImageFormat(), ImageFormat::RAW);
  EXPECT_EQ(yuvSplit4b.image().getPixelFormat(), PixelFormat::YUV_I420_SPLIT);
  EXPECT_TRUE(checkImageDimensions(yuvSplit4b, 642, 480, 0, 642, 340, 340, 340));
  EXPECT_TRUE(checkImageHeights(yuvSplit4b, 480, 240, 240));
  EXPECT_EQ(yuvSplit4b.image().getBlockSize(), 642 * 480 + 2 * 340 * 240);

  ContentBlock yuvSplit4c("image/raw/642x480/pixel=yuv_i420_split/stride=660/stride_2=340");
  EXPECT_EQ(yuvSplit4c.getContentType(), ContentType::IMAGE);
  EXPECT_EQ(yuvSplit4c.image().getImageFormat(), ImageFormat::RAW);
  EXPECT_EQ(yuvSplit4c.image().getPixelFormat(), PixelFormat::YUV_I420_SPLIT);
  EXPECT_TRUE(checkImageDimensions(yuvSplit4c, 642, 480, 660, 660, 340, 340, 340));
  EXPECT_TRUE(checkImageHeights(yuvSplit4c, 480, 240, 240));
  EXPECT_EQ(yuvSplit4c.image().getBlockSize(), 660 * 480 + 2 * 340 * 240);

  ContentBlock yuvNv21("image/raw/640x480/pixel=yuv_420_nv21");
  EXPECT_EQ(yuvNv21.getContentType(), ContentType::IMAGE);
  EXPECT_EQ(yuvNv21.image().getImageFormat(), ImageFormat::RAW);
  EXPECT_EQ(yuvNv21.image().getPixelFormat(), PixelFormat::YUV_420_NV21);
  EXPECT_TRUE(checkImageDimensions(yuvNv21, 640, 480, 0, 640, 0, 640));
  EXPECT_TRUE(checkImageHeights(yuvNv21, 480, 240));
  EXPECT_EQ(yuvNv21.image().getBlockSize(), 640 * 480 + 640 * 240);
  EXPECT_EQ(yuvNv21.image().getBytesPerPixel(), ContentBlock::kSizeUnknown);
  EXPECT_EQ(yuvNv21.image().getChannelCountPerPixel(), 3);
  EXPECT_EQ(yuvNv21.image().getPlaneCount(), 2);

  ContentBlock yuvNv21b("image/raw/641x480/pixel=yuv_420_nv21");
  EXPECT_EQ(yuvNv21b.getContentType(), ContentType::IMAGE);
  EXPECT_EQ(yuvNv21b.image().getImageFormat(), ImageFormat::RAW);
  EXPECT_EQ(yuvNv21b.image().getPixelFormat(), PixelFormat::YUV_420_NV21);
  EXPECT_TRUE(checkImageDimensions(yuvNv21b, 641, 480, 0, 641, 0, 642));
  EXPECT_TRUE(checkImageHeights(yuvNv21b, 480, 240));
  EXPECT_EQ(yuvNv21b.image().getBlockSize(), 641 * 480 + 642 * 240);
  EXPECT_EQ(yuvNv21b.image().getBytesPerPixel(), ContentBlock::kSizeUnknown);
  EXPECT_EQ(yuvNv21b.image().getChannelCountPerPixel(), 3);
  EXPECT_EQ(yuvNv21b.image().getPlaneCount(), 2);

  ContentBlock yuvNv12("image/raw/640x480/pixel=yuv_420_nv12");
  EXPECT_EQ(yuvNv12.getContentType(), ContentType::IMAGE);
  EXPECT_EQ(yuvNv12.image().getImageFormat(), ImageFormat::RAW);
  EXPECT_EQ(yuvNv12.image().getPixelFormat(), PixelFormat::YUV_420_NV12);
  EXPECT_TRUE(checkImageDimensions(yuvNv12, 640, 480, 0, 640, 0, 640));
  EXPECT_TRUE(checkImageHeights(yuvNv12, 480, 240));
  EXPECT_EQ(yuvNv12.image().getBlockSize(), 640 * 480 + 640 * 240);
  EXPECT_EQ(yuvNv12.image().getBytesPerPixel(), ContentBlock::kSizeUnknown);
  EXPECT_EQ(yuvNv12.image().getChannelCountPerPixel(), 3);
  EXPECT_EQ(yuvNv12.image().getBlockSize(), 640 * 480 + 640 * 240);
  EXPECT_EQ(yuvNv12.image().getPlaneCount(), 2);

  ContentBlock yuvNv12b("image/raw/640x480/pixel=yuv_420_nv12/stride_2=680");
  EXPECT_EQ(yuvNv12b.getContentType(), ContentType::IMAGE);
  EXPECT_EQ(yuvNv12b.image().getImageFormat(), ImageFormat::RAW);
  EXPECT_EQ(yuvNv12b.image().getPixelFormat(), PixelFormat::YUV_420_NV12);
  EXPECT_TRUE(checkImageDimensions(yuvNv12b, 640, 480, 0, 640, 680, 680));
  EXPECT_TRUE(checkImageHeights(yuvNv12b, 480, 240));
  EXPECT_EQ(yuvNv12b.image().getBlockSize(), 640 * 480 + 680 * 240);
  EXPECT_EQ(yuvNv12b.image().getBytesPerPixel(), ContentBlock::kSizeUnknown);
  EXPECT_EQ(yuvNv12b.image().getChannelCountPerPixel(), 3);
  EXPECT_EQ(yuvNv12b.image().getPlaneCount(), 2);

  ContentBlock yuvNv12c("image/raw/640x480/pixel=yuv_420_nv12/stride=660/stride_2=680");
  EXPECT_EQ(yuvNv12c.getContentType(), ContentType::IMAGE);
  EXPECT_EQ(yuvNv12c.image().getImageFormat(), ImageFormat::RAW);
  EXPECT_EQ(yuvNv12c.image().getPixelFormat(), PixelFormat::YUV_420_NV12);
  EXPECT_TRUE(checkImageDimensions(yuvNv12c, 640, 480, 660, 660, 680, 680));
  EXPECT_TRUE(checkImageHeights(yuvNv12c, 480, 240));
  EXPECT_EQ(yuvNv12c.image().getBlockSize(), 660 * 480 + 680 * 240);
  EXPECT_EQ(yuvNv12c.image().getBytesPerPixel(), ContentBlock::kSizeUnknown);
  EXPECT_EQ(yuvNv12c.image().getChannelCountPerPixel(), 3);
  EXPECT_EQ(yuvNv12c.image().getPlaneCount(), 2);

  ContentBlock yuvNv12d("image/raw/639x480/pixel=yuv_420_nv12");
  EXPECT_EQ(yuvNv12d.getContentType(), ContentType::IMAGE);
  EXPECT_EQ(yuvNv12d.image().getImageFormat(), ImageFormat::RAW);
  EXPECT_EQ(yuvNv12d.image().getPixelFormat(), PixelFormat::YUV_420_NV12);
  EXPECT_TRUE(checkImageDimensions(yuvNv12d, 639, 480, 0, 639, 0, 640));
  EXPECT_TRUE(checkImageHeights(yuvNv12d, 480, 240));
  EXPECT_EQ(yuvNv12d.image().getBlockSize(), 639 * 480 + 640 * 240);
  EXPECT_EQ(yuvNv12d.image().getBytesPerPixel(), ContentBlock::kSizeUnknown);
  EXPECT_EQ(yuvNv12d.image().getChannelCountPerPixel(), 3);
  EXPECT_EQ(yuvNv12d.image().getPlaneCount(), 2);

  ContentBlock yuy2a("image/raw/642x480/pixel=yuy2");
  EXPECT_EQ(yuy2a.getContentType(), ContentType::IMAGE);
  EXPECT_EQ(yuy2a.image().getImageFormat(), ImageFormat::RAW);
  EXPECT_EQ(yuy2a.image().getPixelFormat(), PixelFormat::YUY2);
  EXPECT_TRUE(checkImageDimensions(yuy2a, 642, 480, 0, 642 * 2));
  EXPECT_TRUE(checkImageHeights(yuy2a, 480));
  EXPECT_EQ(yuy2a.image().getBlockSize(), 642 * 2 * 480);
  EXPECT_EQ(yuy2a.image().getBytesPerPixel(), ContentBlock::kSizeUnknown);
  EXPECT_EQ(yuy2a.image().getChannelCountPerPixel(), 3);

  ContentBlock yuy2b("image/raw/643x480/pixel=yuy2");
  EXPECT_EQ(yuy2b.getContentType(), ContentType::IMAGE);
  EXPECT_EQ(yuy2b.image().getImageFormat(), ImageFormat::RAW);
  EXPECT_EQ(yuy2b.image().getPixelFormat(), PixelFormat::YUY2);
  EXPECT_TRUE(checkImageDimensions(yuy2b, 643, 480, 0, 644 * 2));
  EXPECT_TRUE(checkImageHeights(yuy2b, 480));
  EXPECT_EQ(yuy2b.image().getBlockSize(), 644 * 2 * 480);

  ContentBlock raw10a("image/raw/640x480/pixel=raw10");
  EXPECT_EQ(raw10a.getContentType(), ContentType::IMAGE);
  EXPECT_EQ(raw10a.image().getImageFormat(), ImageFormat::RAW);
  EXPECT_EQ(raw10a.image().getPixelFormat(), PixelFormat::RAW10);
  EXPECT_TRUE(checkImageDimensions(raw10a, 640, 480, 0, 800));
  EXPECT_TRUE(checkImageHeights(raw10a, 480));
  EXPECT_EQ(raw10a.image().getBlockSize(), 800 * 480);
  EXPECT_EQ(raw10a.image().getBytesPerPixel(), ContentBlock::kSizeUnknown);
  EXPECT_EQ(raw10a.image().getChannelCountPerPixel(), 1);

  ContentBlock raw10b("image/raw/641x480/pixel=raw10");
  EXPECT_EQ(raw10b.getContentType(), ContentType::IMAGE);
  EXPECT_EQ(raw10b.image().getImageFormat(), ImageFormat::RAW);
  EXPECT_EQ(raw10b.image().getPixelFormat(), PixelFormat::RAW10);
  EXPECT_TRUE(checkImageDimensions(raw10b, 641, 480, 0, 805));
  EXPECT_TRUE(checkImageHeights(raw10b, 480));
  EXPECT_EQ(raw10b.image().getBlockSize(), 805 * 480);
  EXPECT_EQ(raw10b.image().getBytesPerPixel(), ContentBlock::kSizeUnknown);
  EXPECT_EQ(raw10b.image().getChannelCountPerPixel(), 1);
  EXPECT_EQ(raw10b.image().getCodecQuality(), ImageContentBlockSpec::kQualityUndefined);

  ContentBlock video("image/video/codec_quality=100");
  EXPECT_EQ(video.getContentType(), ContentType::IMAGE);
  EXPECT_EQ(video.image().getImageFormat(), ImageFormat::VIDEO);
  EXPECT_EQ(video.image().getPixelFormat(), PixelFormat::UNDEFINED);
  EXPECT_TRUE(checkImageDimensions(video, 0, 0, 0, 0));
  EXPECT_TRUE(checkImageHeights(video, 0));
  EXPECT_EQ(video.image().getBytesPerPixel(), ContentBlock::kSizeUnknown);
  EXPECT_EQ(video.image().getChannelCountPerPixel(), 0);
  EXPECT_EQ(video.image().getCodecName(), "");
  EXPECT_EQ(video.image().getCodecQuality(), 100);

  ContentBlock videoCodec("image/video/codec=H.264");
  EXPECT_EQ(videoCodec.getContentType(), ContentType::IMAGE);
  EXPECT_EQ(videoCodec.image().getImageFormat(), ImageFormat::VIDEO);
  EXPECT_EQ(videoCodec.image().getPixelFormat(), PixelFormat::UNDEFINED);
  EXPECT_TRUE(checkImageDimensions(videoCodec, 0, 0, 0, 0));
  EXPECT_TRUE(checkImageHeights(videoCodec, 0));
  EXPECT_EQ(videoCodec.image().getBytesPerPixel(), ContentBlock::kSizeUnknown);
  EXPECT_EQ(videoCodec.image().getChannelCountPerPixel(), 0);
  EXPECT_EQ(videoCodec.image().getCodecName(), "H.264");
  EXPECT_EQ(videoCodec.image().getCodecQuality(), ImageContentBlockSpec::kQualityUndefined);

  ContentBlock videoCodecQuality("image/video/codec=VP9/codec_quality=35");
  EXPECT_EQ(videoCodecQuality.getContentType(), ContentType::IMAGE);
  EXPECT_EQ(videoCodecQuality.image().getImageFormat(), ImageFormat::VIDEO);
  EXPECT_EQ(videoCodecQuality.image().getPixelFormat(), PixelFormat::UNDEFINED);
  EXPECT_TRUE(checkImageDimensions(videoCodecQuality, 0, 0, 0, 0));
  EXPECT_TRUE(checkImageHeights(videoCodecQuality, 0));
  EXPECT_EQ(videoCodecQuality.image().getBytesPerPixel(), ContentBlock::kSizeUnknown);
  EXPECT_EQ(videoCodecQuality.image().getChannelCountPerPixel(), 0);
  EXPECT_EQ(videoCodecQuality.image().getCodecName(), "VP9");
  EXPECT_EQ(videoCodecQuality.image().getCodecQuality(), 35);

  EXPECT_EQ(
      ImageContentBlockSpec("H.264 % + / \\ \" %", 0, PixelFormat::GREY8, 640, 480).asString(),
      "video/640x480/pixel=grey8/codec=H.264%20%25%20%2B%20%2F%20%5C%20%22%20%25/codec_quality=0");

  ContentBlock videoCodecEscaped(
      "image/video/640x480/stride=1300/pixel=grey12/codec=%2Bconfusing%2Fcodec%2Fbad%2Bname");
  EXPECT_EQ(videoCodecEscaped.getContentType(), ContentType::IMAGE);
  EXPECT_EQ(videoCodecEscaped.image().getImageFormat(), ImageFormat::VIDEO);
  EXPECT_EQ(videoCodecEscaped.image().getPixelFormat(), PixelFormat::GREY12);
  EXPECT_TRUE(checkImageDimensions(videoCodecEscaped, 640, 480, 1300, 1300));
  EXPECT_TRUE(checkImageHeights(videoCodecEscaped, 480));
  EXPECT_EQ(videoCodecEscaped.image().getBlockSize(), ContentBlock::kSizeUnknown);
  EXPECT_EQ(videoCodecEscaped.image().getBytesPerPixel(), 2);
  EXPECT_EQ(videoCodecEscaped.image().getChannelCountPerPixel(), 1);
  EXPECT_EQ(videoCodecEscaped.image().getCodecName(), "+confusing/codec/bad+name");
  EXPECT_EQ(videoCodecEscaped.image().getCodecQuality(), ImageContentBlockSpec::kQualityUndefined);

  ContentBlock partial("audio/pcm/uint24be/rate=32000/channels=1");
  EXPECT_EQ(partial.getContentType(), ContentType::AUDIO);
  EXPECT_EQ(partial.getBlockSize(), ContentBlock::kSizeUnknown);
  EXPECT_EQ(partial.audio().getAudioFormat(), AudioFormat::PCM);
  EXPECT_EQ(partial.audio().getSampleFormat(), AudioSampleFormat::U24_BE);
  EXPECT_EQ(partial.audio().getSampleRate(), 32000);
  EXPECT_EQ(partial.audio().getChannelCount(), 1);
  EXPECT_EQ(partial.audio().getBitsPerSample(), 24);
  EXPECT_EQ(partial.audio().isLittleEndian(), false);
  EXPECT_EQ(partial.audio().getSampleFrameStride(), 3);
  EXPECT_EQ(partial.audio().getSampleCount(), 0);

  ContentBlock full("audio/pcm/float64be/channels=2/rate=32000/samples=100/stride=16");
  EXPECT_EQ(full.getContentType(), ContentType::AUDIO);
  EXPECT_EQ(full.getBlockSize(), 100 * 8 * 2);
  EXPECT_EQ(full.audio().getAudioFormat(), AudioFormat::PCM);
  EXPECT_EQ(full.audio().getSampleFormat(), AudioSampleFormat::F64_BE);
  EXPECT_EQ(full.audio().getSampleRate(), 32000);
  EXPECT_EQ(full.audio().getChannelCount(), 2);
  EXPECT_EQ(full.audio().getBitsPerSample(), 64);
  EXPECT_EQ(full.audio().isLittleEndian(), false);
  EXPECT_EQ(full.audio().getSampleFrameStride(), 16);
  EXPECT_EQ(full.audio().getSampleCount(), 100);

  ContentBlock direct("audio/pcm/float64be/channels=2/rate=32000/samples=100/stride=0");
  EXPECT_EQ(direct.getContentType(), ContentType::AUDIO);
  EXPECT_EQ(direct.getBlockSize(), 100 * 8 * 2);
  EXPECT_EQ(direct.audio().getBitsPerSample(), 64);
  EXPECT_EQ(direct.audio().isLittleEndian(), false);
  EXPECT_EQ(direct.audio().getSampleFrameStride(), 16);
  EXPECT_EQ(
      direct.audio(),
      AudioContentBlockSpec(AudioFormat::PCM, AudioSampleFormat::F64_BE, 2, 0, 32000, 100));

  ContentBlock exotic("audio/pcm/int24be/channels=3/rate=12345");
  EXPECT_EQ(exotic.getContentType(), ContentType::AUDIO);
  EXPECT_EQ(exotic.getBlockSize(), ContentBlock::kSizeUnknown);
  EXPECT_EQ(exotic.audio().getBitsPerSample(), 24);
  EXPECT_EQ(exotic.audio().getSampleFrameStride(), 9);
  EXPECT_EQ(
      exotic.audio(),
      AudioContentBlockSpec(AudioFormat::PCM, AudioSampleFormat::S24_BE, 3, 0, 12345, 0));

  ContentBlock telco("audio/pcm/uint8mulaw/channels=1/rate=8000/samples=800");
  EXPECT_EQ(telco.getContentType(), ContentType::AUDIO);
  EXPECT_EQ(telco.getBlockSize(), 800 * 1);
  EXPECT_EQ(telco.audio().getBitsPerSample(), 8);
  EXPECT_EQ(telco.audio().getSampleFrameStride(), 1);
  EXPECT_EQ(
      telco.audio(),
      AudioContentBlockSpec(AudioFormat::PCM, AudioSampleFormat::MU_LAW, 1, 0, 8000, 800));

  FORMAT_EQUAL(ContentBlock(ContentType::AUDIO), "audio");
  FORMAT_EQUAL(
      ContentBlock(AudioFormat::PCM, AudioSampleFormat::F64_BE, 2, 16, 32000, 100),
      "audio/pcm/float64be/channels=2/rate=32000/samples=100");

  ContentBlock opus("audio/opus");
  EXPECT_EQ(opus.getContentType(), ContentType::AUDIO);
  EXPECT_EQ(opus.getBlockSize(), ContentBlock::kSizeUnknown);
  EXPECT_EQ(opus.audio().getPcmBlockSize(), ContentBlock::kSizeUnknown);
  EXPECT_EQ(opus.audio(), AudioContentBlockSpec(AudioFormat::OPUS, AudioSampleFormat::UNDEFINED));

  ContentBlock opusFull("audio/opus/float64be/channels=2/rate=32000/samples=100/stride=16");
  EXPECT_EQ(opusFull.getContentType(), ContentType::AUDIO);
  EXPECT_EQ(opusFull.getBlockSize(), ContentBlock::kSizeUnknown);
  EXPECT_EQ(opusFull.audio().getPcmBlockSize(), 100 * 8 * 2);
  EXPECT_EQ(opusFull.audio().isLittleEndian(), false);
  EXPECT_EQ(opusFull.audio().getSampleFrameStride(), 16);
  EXPECT_EQ(
      opusFull.audio(),
      AudioContentBlockSpec(AudioFormat::OPUS, AudioSampleFormat::F64_BE, 2, 0, 32000, 100));

  FORMAT_EQUAL(ContentBlock(ContentType::CUSTOM), "custom");
  FORMAT_EQUAL(ContentBlock(ContentType::CUSTOM, 20), "custom/size=20");
}

TEST_F(RecordFormatTest, testBadStride) {
  ImageContentBlockSpec yuv_i420_split("raw/642x480/pixel=yuv_i420_split/stride=641");
  EXPECT_FALSE(yuv_i420_split.sanityCheckStrides());
  ImageContentBlockSpec rgb8("raw/5312x2988/pixel=rgb8/stride=5312");
  EXPECT_FALSE(rgb8.sanityCheckStrides());
  ImageContentBlockSpec yuv_420_nv12("raw/640x480/pixel=yuv_420_nv12/stride=660/stride_2=639");
  EXPECT_FALSE(yuv_420_nv12.sanityCheckStrides());
}

TEST_F(RecordFormatTest, testOperators) {
  FORMAT_EQUAL(RecordFormat(ContentType::CUSTOM), "custom");
  FORMAT_EQUAL(ContentBlock(ContentType::CUSTOM), "custom");
  FORMAT_EQUAL(
      (ContentBlock(ContentType::IMAGE) + ContentBlock(ContentType::CUSTOM)), "image+custom");
  FORMAT_EQUAL(
      (ContentBlock(ContentType::IMAGE) + ContentBlock(ContentType::DATA_LAYOUT) +
       ContentBlock(ContentType::CUSTOM)),
      "image+data_layout+custom");
  FORMAT_EQUAL(
      (RecordFormat(ContentType::DATA_LAYOUT) + ContentBlock(ContentType::CUSTOM, 56) +
       ContentBlock(ContentType::AUDIO, 512) + ContentBlock(ContentType::IMAGE)),
      "data_layout+custom/size=56+audio/size=512+image");
}

TEST_F(RecordFormatTest, testFormatToString) {
  FORMAT_EQUAL(RecordFormat(ContentType::CUSTOM), "custom");
  FORMAT_EQUAL(RecordFormat(ContentType::CUSTOM, 20), "custom/size=20");
  FORMAT_EQUAL(RecordFormat(ContentType::EMPTY), "empty");
  FORMAT_EQUAL(RecordFormat(ContentType::IMAGE), "image");
  FORMAT_EQUAL(RecordFormat(ContentType::AUDIO), "audio");

  FORMAT_EQUAL(RecordFormat(ImageFormat::JPG), "image/jpg");
  FORMAT_EQUAL(RecordFormat(ContentBlock(ImageFormat::JPG, 10, 20)), "image/jpg/10x20");
  FORMAT_EQUAL(RecordFormat(ImageFormat::PNG), "image/png");
  FORMAT_EQUAL(RecordFormat(ContentBlock(ImageFormat::PNG, 1, 2)), "image/png/1x2");
}

TEST_F(RecordFormatTest, testUsedBlockCount) {
  EXPECT_EQ(RecordFormat("custom").getUsedBlocksCount(), 1);
  EXPECT_EQ(RecordFormat("custom/size=20").getUsedBlocksCount(), 1);
  EXPECT_EQ(RecordFormat("empty").getUsedBlocksCount(), 0);
  EXPECT_EQ(RecordFormat("image").getUsedBlocksCount(), 1);
  EXPECT_EQ(RecordFormat("audio").getUsedBlocksCount(), 1);
  EXPECT_EQ(RecordFormat("json").getUsedBlocksCount(), 1);
  EXPECT_EQ(RecordFormat("text").getUsedBlocksCount(), 1);
  EXPECT_EQ(RecordFormat("image/jpg").getUsedBlocksCount(), 1);
  EXPECT_EQ(RecordFormat("image/jpg/10x20").getUsedBlocksCount(), 1);
  EXPECT_EQ(RecordFormat("image/png").getUsedBlocksCount(), 1);
  EXPECT_EQ(RecordFormat("image/png/1x2").getUsedBlocksCount(), 1);
  EXPECT_EQ(RecordFormat("custom+image/png/1x2").getUsedBlocksCount(), 2);
  EXPECT_EQ(RecordFormat("image+image/png/1x2").getUsedBlocksCount(), 2);
  EXPECT_EQ(RecordFormat("empty+image/png/1x2").getUsedBlocksCount(), 2); // weird, but expected
  EXPECT_EQ(RecordFormat("custom/size=70+image/raw/20x30/pixel=bgr8").getUsedBlocksCount(), 2);
}

TEST_F(RecordFormatTest, testBlocksOfFormatCount) {
  EXPECT_EQ(RecordFormat("custom").getBlocksOfTypeCount(ContentType::CUSTOM), 1);
  EXPECT_EQ(RecordFormat("custom/size=20").getBlocksOfTypeCount(ContentType::CUSTOM), 1);
  EXPECT_EQ(RecordFormat("empty").getBlocksOfTypeCount(ContentType::CUSTOM), 0);
  EXPECT_EQ(RecordFormat("image").getBlocksOfTypeCount(ContentType::IMAGE), 1);
  EXPECT_EQ(RecordFormat("image").getBlocksOfTypeCount(ContentType::CUSTOM), 0);
  EXPECT_EQ(RecordFormat("audio").getBlocksOfTypeCount(ContentType::IMAGE), 0);
  EXPECT_EQ(RecordFormat("image/jpg").getBlocksOfTypeCount(ContentType::IMAGE), 1);
  EXPECT_EQ(RecordFormat("image/jpg/10x20").getBlocksOfTypeCount(ContentType::IMAGE), 1);
  EXPECT_EQ(RecordFormat("image/png").getBlocksOfTypeCount(ContentType::IMAGE), 1);
  EXPECT_EQ(RecordFormat("image/png").getBlocksOfTypeCount(ContentType::CUSTOM), 0);
  EXPECT_EQ(RecordFormat("image/png/1x2").getBlocksOfTypeCount(ContentType::IMAGE), 1);
  EXPECT_EQ(RecordFormat("custom+image/png/1x2").getBlocksOfTypeCount(ContentType::IMAGE), 1);
  EXPECT_EQ(RecordFormat("custom+image/png/1x2").getBlocksOfTypeCount(ContentType::CUSTOM), 1);
  EXPECT_EQ(RecordFormat("custom+image/png/1x2").getBlocksOfTypeCount(ContentType::AUDIO), 0);
  EXPECT_EQ(RecordFormat("image+image/png/1x2").getBlocksOfTypeCount(ContentType::IMAGE), 2);
  EXPECT_EQ(RecordFormat("empty+image/png/1x2").getBlocksOfTypeCount(ContentType::IMAGE), 1);
  EXPECT_EQ(
      RecordFormat("custom/size=70+image/raw/20x30/pixel=bgr8")
          .getBlocksOfTypeCount(ContentType::IMAGE),
      1);
}

TEST_F(RecordFormatTest, testFormatFromString) {
  EXPECT_EQ(RecordFormat("custom").getFirstContentBlock().getContentType(), ContentType::CUSTOM);
  EXPECT_EQ(
      RecordFormat("data_layout").getFirstContentBlock().getContentType(),
      ContentType::DATA_LAYOUT);
  EXPECT_EQ(RecordFormat("empty").getFirstContentBlock().getContentType(), ContentType::EMPTY);
  EXPECT_EQ(RecordFormat("image").getFirstContentBlock().getContentType(), ContentType::IMAGE);
  EXPECT_EQ(RecordFormat("whatever").getFirstContentBlock().getContentType(), ContentType::CUSTOM);
}

TEST_F(RecordFormatTest, testGetDataLayoutTagName) {
  EXPECT_STREQ(RecordFormat::getDataLayoutTagName(Record::Type::DATA, 1, 2).c_str(), "DL:Data:1:2");
  EXPECT_STREQ(
      RecordFormat::getDataLayoutTagName(Record::Type::STATE, 10, 1256).c_str(),
      "DL:State:10:1256");
  EXPECT_STREQ(
      RecordFormat::getDataLayoutTagName(Record::Type::CONFIGURATION, 2, 0).c_str(),
      "DL:Configuration:2:0");
}

TEST_F(RecordFormatTest, testFormat) {
  const char* formats[] = {
      "custom",
      "audio",
      "audio/pcm",
      "audio/size=100/pcm/channels=5",
      "audio/pcm/int16le/channels=2",
      "audio/pcm/float32le/rate=48000",
      "audio/pcm/uint24be/channels=2/rate=48000/stride=4",
      "audio/pcm/uint8mulaw/channels=2/rate=48000",
      "audio/pcm/uint8alaw/channels=2/rate=48000",
      "data_layout",
      "image/png",
      "image/jpg",
      "image/jpg/100x200",
      "image/png/12x60",
      "image/raw/100x200/pixel=bgr8",
      "image/raw/10x20/pixel=grey8",
      "image/raw/102x200/pixel=depth32f",
      "image/raw/102x200/pixel=yuv_i420_split",
      "image/raw/102x200/pixel=yuy2",
      "image/raw/102x200/pixel=yuy2/stride=204",
      "image/raw/102x200/pixel=yuv_420_nv12/stride=120/stride_2=104",
      "image/raw/102x200/pixel=yuv_420_nv12/stride_2=104",
      "image/video/1024x800/pixel=raw10/codec=H.264",
      "image/video/1024x800/pixel=raw10/codec=VP9/codec_quality=53",
      "image/video/640x480/pixel=grey8/codec_quality=0",
      "image/video/1920x1080/pixel=rgb8/codec_quality=100",
      "image/video/640x480/pixel=grey8/codec=H.264%20%25%20%2B%20%2F%20%5C%20%22%20",
      "image/video/640x480/pixel=grey8/codec=H.254/keyframe_timestamp=2.251009123/keyframe_index=5",
      "data_layout+image/raw/102x200/pixel=depth32f",
      "custom/size=70+image/raw/20x30/pixel=bgr8/stride=24"};
  for (size_t k = 0; k < COUNT_OF(formats); ++k) {
    FORMAT_EQUAL(RecordFormat(formats[k]), formats[k]);
  }
}

TEST_F(RecordFormatTest, testGetRecordFormatTagName) {
  EXPECT_STREQ(RecordFormat::getRecordFormatTagName(Record::Type::DATA, 1).c_str(), "RF:Data:1");
  EXPECT_STREQ(
      RecordFormat::getRecordFormatTagName(Record::Type::STATE, 10).c_str(), "RF:State:10");
  EXPECT_STREQ(
      RecordFormat::getRecordFormatTagName(Record::Type::CONFIGURATION, 42).c_str(),
      "RF:Configuration:42");
}

#define TEST_RECORD_FORMAT_NAME(FORMAT_NAME, RECORD_TYPE, FORMAT_VERSION)                      \
  EXPECT_TRUE(RecordFormat::parseRecordFormatTagName(FORMAT_NAME, recordType, formatVersion)); \
  EXPECT_EQ(recordType, RECORD_TYPE);                                                          \
  EXPECT_EQ(formatVersion, FORMAT_VERSION);

#define TEST_BAD_RECORD_FORMAT_NAME(FORMAT_NAME) \
  EXPECT_FALSE(RecordFormat::parseRecordFormatTagName(FORMAT_NAME, recordType, formatVersion));

TEST_F(RecordFormatTest, testGetFormatVersionFromTagName) {
  const Record::Type CONFIGURATION = Record::Type::CONFIGURATION;
  const Record::Type STATE = Record::Type::STATE;
  const Record::Type DATA = Record::Type::DATA;

  Record::Type recordType{};
  uint32_t formatVersion = 0;
  TEST_RECORD_FORMAT_NAME("RF:Data:0", DATA, 0);
  TEST_RECORD_FORMAT_NAME("RF:Data:00", DATA, 0);
  TEST_RECORD_FORMAT_NAME("RF:Data:1", DATA, 1);
  TEST_RECORD_FORMAT_NAME("RF:Data:10", DATA, 10);
  TEST_RECORD_FORMAT_NAME("RF:Data:0236950285", DATA, 236950285);
  TEST_RECORD_FORMAT_NAME("RF:State:1", STATE, 1);
  TEST_RECORD_FORMAT_NAME("RF:Configuration:1", CONFIGURATION, 1);
  for (Record::Type t : {DATA, STATE, CONFIGURATION}) {
    for (uint32_t format = 0; format < 15; format++) {
      string tagName = RecordFormat::getRecordFormatTagName(t, format);
      TEST_RECORD_FORMAT_NAME(tagName, t, format);
    }
  }
  TEST_BAD_RECORD_FORMAT_NAME("RF:Data:a");
  TEST_BAD_RECORD_FORMAT_NAME("RF:Data:");
  TEST_BAD_RECORD_FORMAT_NAME("RF:Data:x1");
  TEST_BAD_RECORD_FORMAT_NAME("RF:Data:1x");
  TEST_BAD_RECORD_FORMAT_NAME("RF:Data:100.");
  TEST_BAD_RECORD_FORMAT_NAME("RF:Data:-100");
}

TEST_F(RecordFormatTest, testFormatSizes) {
  RecordFormat recordFormat("");
  EXPECT_EQ(recordFormat.getUsedBlocksCount(), 1);
  EXPECT_EQ(recordFormat.getBlocksOfTypeCount(ContentType::CUSTOM), 1);
  EXPECT_EQ(recordFormat.getRecordSize(), ContentBlock::kSizeUnknown); // that's custom
  EXPECT_EQ(recordFormat.getBlockSize(0, 200), 200);

  recordFormat.set("custom");
  EXPECT_EQ(recordFormat.getUsedBlocksCount(), 1);
  EXPECT_EQ(recordFormat.getBlocksOfTypeCount(ContentType::CUSTOM), 1);
  EXPECT_EQ(recordFormat.getRecordSize(), ContentBlock::kSizeUnknown);
  EXPECT_EQ(recordFormat.getBlockSize(0, 150), 150);

  recordFormat.set("custom/size=20");
  EXPECT_EQ(recordFormat.getUsedBlocksCount(), 1);
  EXPECT_EQ(recordFormat.getBlocksOfTypeCount(ContentType::CUSTOM), 1);
  EXPECT_EQ(recordFormat.getRecordSize(), 20);
  EXPECT_EQ(recordFormat.getBlockSize(0, 200), 20);

  recordFormat.set("empty");
  EXPECT_EQ(recordFormat.getUsedBlocksCount(), 0);
  EXPECT_EQ(recordFormat.getBlocksOfTypeCount(ContentType::EMPTY), 1);
  EXPECT_EQ(recordFormat.getRecordSize(), 0);
  EXPECT_EQ(recordFormat.getBlockSize(0, 200), 0);

  recordFormat.set("audio");
  EXPECT_EQ(recordFormat.getUsedBlocksCount(), 1);
  EXPECT_EQ(recordFormat.getBlocksOfTypeCount(ContentType::AUDIO), 1);
  EXPECT_EQ(recordFormat.getRecordSize(), ContentBlock::kSizeUnknown);
  EXPECT_EQ(recordFormat.getBlockSize(0, 200), 200);

  recordFormat.set("audio/size=512");
  EXPECT_EQ(recordFormat.getUsedBlocksCount(), 1);
  EXPECT_EQ(recordFormat.getBlocksOfTypeCount(ContentType::AUDIO), 1);
  EXPECT_EQ(recordFormat.getRecordSize(), 512);
  EXPECT_EQ(recordFormat.getBlockSize(0, 512), 512);
  EXPECT_EQ(recordFormat.getBlockSize(0, 511), ContentBlock::kSizeUnknown); // too small: error

  recordFormat.set("image");
  EXPECT_EQ(recordFormat.getUsedBlocksCount(), 1);
  EXPECT_EQ(recordFormat.getBlocksOfTypeCount(ContentType::IMAGE), 1);
  EXPECT_EQ(recordFormat.getRecordSize(), ContentBlock::kSizeUnknown);
  EXPECT_EQ(recordFormat.getBlockSize(0, 512), 512);

  recordFormat.set("image/jpg");
  EXPECT_EQ(recordFormat.getUsedBlocksCount(), 1);
  EXPECT_EQ(recordFormat.getBlocksOfTypeCount(ContentType::IMAGE), 1);
  EXPECT_EQ(recordFormat.getRecordSize(), ContentBlock::kSizeUnknown);
  EXPECT_EQ(recordFormat.getBlockSize(0, 512), 512);

  recordFormat.set("image/png");
  EXPECT_EQ(recordFormat.getUsedBlocksCount(), 1);
  EXPECT_EQ(recordFormat.getBlocksOfTypeCount(ContentType::IMAGE), 1);
  EXPECT_EQ(recordFormat.getRecordSize(), ContentBlock::kSizeUnknown);
  EXPECT_EQ(recordFormat.getBlockSize(0, 512), 512);

  recordFormat.set("image/png/200x100");
  EXPECT_EQ(recordFormat.getUsedBlocksCount(), 1);
  EXPECT_EQ(recordFormat.getBlocksOfTypeCount(ContentType::IMAGE), 1);
  EXPECT_EQ(recordFormat.getRecordSize(), ContentBlock::kSizeUnknown);
  EXPECT_EQ(recordFormat.getBlockSize(0, 512), 512);

  recordFormat.set("image/raw/10x20/pixel=grey8");
  EXPECT_EQ(recordFormat.getUsedBlocksCount(), 1);
  EXPECT_EQ(recordFormat.getBlocksOfTypeCount(ContentType::IMAGE), 1);
  EXPECT_EQ(recordFormat.getRecordSize(), 200);
  EXPECT_EQ(recordFormat.getBlockSize(0, 200), 200);
  EXPECT_EQ(recordFormat.getBlockSize(0, 199), ContentBlock::kSizeUnknown);

  recordFormat.set("image/raw/10x20/pixel=bgr8");
  EXPECT_EQ(recordFormat.getRecordSize(), 600);

  recordFormat.set("image/raw/10x20/pixel=depth32f");
  EXPECT_EQ(recordFormat.getRecordSize(), 800);

  recordFormat.set("custom+image/raw/10x20/pixel=depth32f");
  EXPECT_EQ(recordFormat.getRecordSize(), ContentBlock::kSizeUnknown);
  EXPECT_EQ(recordFormat.getContentBlock(1).image().getBlockSize(), 800);
  EXPECT_EQ(recordFormat.getContentBlock(1).image().getRawImageSize(), 800);
  EXPECT_EQ(recordFormat.getBlockSize(0, 799), ContentBlock::kSizeUnknown);
  EXPECT_EQ(recordFormat.getBlockSize(0, 800), 0);
  EXPECT_EQ(recordFormat.getBlockSize(0, 821), 21);

  recordFormat.set("image/raw/10x20");
  EXPECT_EQ(recordFormat.getRecordSize(), ContentBlock::kSizeUnknown);

  recordFormat.set("custom+image/raw/10x20/pixel=grey8+audio/size=1024");
  EXPECT_EQ(recordFormat.getRecordSize(), ContentBlock::kSizeUnknown);
  EXPECT_EQ(recordFormat.getRemainingBlocksSize(1), 1224);

  recordFormat.set("image/raw/10x20+custom");
  EXPECT_EQ(recordFormat.getRecordSize(), ContentBlock::kSizeUnknown);
}

TEST_F(RecordFormatTest, testDataLayout) {
  FixedLayout fixedLayout;
  FORMAT_EQUAL(RecordFormat(fixedLayout.getContentBlock()), "data_layout/size=48");
  ContentBlock greyImage640x480(PixelFormat::GREY8, 640, 480);
  FORMAT_EQUAL(
      RecordFormat(fixedLayout.getContentBlock() + greyImage640x480),
      "data_layout/size=48+image/raw/640x480/pixel=grey8");

  VarLayout varLayout;
  FORMAT_EQUAL(RecordFormat(varLayout.getContentBlock()), "data_layout");
  FORMAT_EQUAL(
      RecordFormat(varLayout.getContentBlock() + greyImage640x480),
      "data_layout+image/raw/640x480/pixel=grey8");
}

TEST_F(RecordFormatTest, testTagSetHelpers) {
  vector<string> tags, readTags;
  string jsonTags = tag_conventions::makeTagSet(tags);
  EXPECT_EQ(strcmp(jsonTags.c_str(), "{}"), 0);
  EXPECT_TRUE(tag_conventions::parseTagSet(jsonTags, readTags));
  EXPECT_EQ(tags, readTags);

  tags.emplace_back("hello");
  jsonTags = tag_conventions::makeTagSet(tags);
  EXPECT_EQ(strcmp(jsonTags.c_str(), "{\"tags\":[\"hello\"]}"), 0);
  EXPECT_TRUE(tag_conventions::parseTagSet(jsonTags, readTags));
  EXPECT_EQ(tags, readTags);

  tags.emplace_back(jsonTags);
  jsonTags = tag_conventions::makeTagSet(tags);
  EXPECT_TRUE(tag_conventions::parseTagSet(jsonTags, readTags));
  EXPECT_EQ(tags, readTags);

  // Stress parsing a bit
  EXPECT_FALSE(tag_conventions::parseTagSet("", readTags));
  EXPECT_EQ(readTags.size(), 0);
  readTags.resize(2);
  EXPECT_FALSE(tag_conventions::parseTagSet("hello", readTags));
  EXPECT_EQ(readTags.size(), 0);
  EXPECT_FALSE(tag_conventions::parseTagSet("{", readTags));
  EXPECT_FALSE(tag_conventions::parseTagSet("{bad}", readTags));
}

TEST_F(RecordFormatTest, testAddRecordFormatChecks) {
  TestRecordable recordable;
  FixedLayout fixedLayout;
  VarLayout varLayout;
  // proper definition
  EXPECT_TRUE(recordable.addRecordFormat(
      Record::Type::DATA, 0, fixedLayout.getContentBlock(), {&fixedLayout}));
  // missing datalayout
  EXPECT_FALSE(
      recordable.addRecordFormat(Record::Type::DATA, 1, fixedLayout.getContentBlock(), {}));
  // extra datalayout
  EXPECT_FALSE(recordable.addRecordFormat(
      Record::Type::DATA, 2, fixedLayout.getContentBlock(), {&fixedLayout, &varLayout}));
  // wrong place
  EXPECT_FALSE(recordable.addRecordFormat(
      Record::Type::DATA,
      3,
      ContentBlock(ContentType::IMAGE) + fixedLayout.getContentBlock(),
      {&fixedLayout}));
}

TEST_F(RecordFormatTest, testCompare) {
  const uint8_t kQUndefined = ImageContentBlockSpec::kQualityUndefined; // shorter to use...

  ImageContentBlockSpec def;
  EXPECT_EQ(
      def,
      ImageContentBlockSpec(
          ImageFormat::UNDEFINED, PixelFormat::UNDEFINED, 0, 0, 0, 0, {}, kQUndefined));

  ImageContentBlockSpec copy{def};
  EXPECT_EQ(def, copy);

  ImageContentBlockSpec exp(ImageFormat::VIDEO, PixelFormat::GREY10, 10, 20, 25, 0, "test", 12);
  EXPECT_NE(def, exp);
  EXPECT_EQ(exp.getImageFormat(), ImageFormat::VIDEO);
  EXPECT_EQ(exp.getPixelFormat(), PixelFormat::GREY10);
  EXPECT_EQ(exp.getWidth(), 10);
  EXPECT_EQ(exp.getHeight(), 20);
  EXPECT_EQ(exp.getStride(), 25);
  EXPECT_EQ(exp.getRawImageSize(), 500);
  EXPECT_EQ(exp.getRawStride2(), 0);
  EXPECT_EQ(exp.getCodecName(), "test");
  EXPECT_EQ(exp.getCodecQuality(), 12);

  ImageContentBlockSpec exp2(
      ImageFormat::VIDEO, PixelFormat::GREY12, 10, 20, 25, 0, "test", 12, 1.2, 5);
  EXPECT_NE(def, exp2);
  EXPECT_EQ(exp2.getImageFormat(), ImageFormat::VIDEO);
  EXPECT_EQ(exp2.getPixelFormat(), PixelFormat::GREY12);
  EXPECT_EQ(exp2.getWidth(), 10);
  EXPECT_EQ(exp2.getHeight(), 20);
  EXPECT_EQ(exp2.getStride(), 25);
  EXPECT_EQ(exp2.getRawImageSize(), 500);
  EXPECT_EQ(exp.getRawStride2(), 0);
  EXPECT_EQ(exp2.getCodecName(), "test");
  EXPECT_EQ(exp2.getCodecQuality(), 12);
  EXPECT_NEAR(exp2.getKeyFrameTimestamp(), 1.2, 1e-9);
  EXPECT_EQ(exp2.getKeyFrameIndex(), 5);

  ImageContentBlockSpec raw(PixelFormat::GREY8, 1, 2, 3);
  EXPECT_EQ(
      raw,
      ImageContentBlockSpec(ImageFormat::RAW, PixelFormat::GREY8, 1, 2, 3, 0, {}, kQUndefined));

  ImageContentBlockSpec video("H.264", 0, PixelFormat::GREY8, 2, 3, 4);
  EXPECT_EQ(
      video, ImageContentBlockSpec(ImageFormat::VIDEO, PixelFormat::GREY8, 2, 3, 4, 0, "H.264", 0));

  ImageContentBlockSpec video2(video, 1.250, 34);
  EXPECT_EQ(
      video2,
      ImageContentBlockSpec(
          ImageFormat::VIDEO, PixelFormat::GREY8, 2, 3, 4, 0, "H.264", 0, 1.250, 34));

  ImageContentBlockSpec jpg(ImageFormat::JPG, 5, 6);
  EXPECT_EQ(
      jpg,
      ImageContentBlockSpec(ImageFormat::JPG, PixelFormat::UNDEFINED, 5, 6, 0, 0, {}, kQUndefined));
}

TEST_F(RecordFormatTest, testPixelFormat) {
  for (uint8_t p = 1; p < static_cast<uint8_t>(PixelFormat::COUNT); p++) {
    PixelFormat pf = static_cast<PixelFormat>(p);
    ImageContentBlockSpec spec(pf, 100, 100, 0);
    for (uint32_t plane = 0; plane < spec.getPlaneCount(); plane++) {
      EXPECT_NE(spec.getPlaneStride(plane), 0);
    }
  }
}
