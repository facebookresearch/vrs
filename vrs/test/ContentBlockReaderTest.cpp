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

#include <cstdio>

#include <gtest/gtest.h>

#include <vrs/DataLayoutConventions.h>
#include <vrs/RecordFormatStreamPlayer.h>

using namespace vrs;
using namespace vrs::datalayout_conventions;

struct ContentBlockReaderTest : testing::Test {};
namespace {

ContentBlock getImageContentBlock(
    DataLayout& layout,
    const ImageContentBlockSpec& base,
    size_t blockSize = ContentBlock::kSizeUnknown) {
  layout.collectVariableDataAndUpdateIndex(); // Enable reading staged fields
  ImageSpec officialSpec;
  officialSpec.mapLayout(layout);
  return officialSpec.getImageContentBlock(base, blockSize);
}

bool isImageSpec(
    const ImageContentBlockSpec& spec,
    DataLayout& layout,
    const ImageContentBlockSpec& base,
    size_t blockSize = ContentBlock::kSizeUnknown) {
  ContentBlock cb = getImageContentBlock(layout, base, blockSize);
  string imageBlock = cb.image().asString();
  string expected = spec.asString();
  return imageBlock == expected;
}

bool hasImageContentBlock(
    DataLayout& layout,
    const ImageContentBlockSpec& base,
    size_t blockSize = ContentBlock::kSizeUnknown) {
  ContentBlock cb = getImageContentBlock(layout, base, blockSize);
  return cb.getContentType() == ContentType::IMAGE;
}

} // namespace

TEST_F(ContentBlockReaderTest, rawImageSpecTest) {
  class MinimumSpec : public AutoDataLayout {
   public:
    DataPieceEnum<PixelFormat, ImageSpecType> pixelFormat{kImagePixelFormat};
    DataPieceValue<ImageSpecType> height{kImageHeight};
    DataPieceValue<ImageSpecType> width{kImageWidth};

    AutoDataLayoutEnd end;

    void set(PixelFormat format, uint32_t w, uint32_t h) {
      pixelFormat.set(format);
      width.set(w);
      height.set(h);
    }
  };

  MinimumSpec spec;
  EXPECT_FALSE(hasImageContentBlock(spec, ImageFormat::RAW, 123));
  spec.set(PixelFormat::GREY8, 100, 100);
  EXPECT_TRUE(isImageSpec({PixelFormat::GREY8, 100, 100}, spec, ImageFormat::RAW, 123));
}

TEST_F(ContentBlockReaderTest, customCodecImageSpecTest) {
  class Spec : public AutoDataLayout {
   public:
    DataLayout& set(PixelFormat format, uint32_t w = 0, uint32_t h = 0) {
      pixelFormat.set(format);
      width.set(w);
      height.set(h);
      codecQuality.set(ImageContentBlockSpec::kQualityUndefined);
      return *this;
    }
    DataLayout& set(
        const string& cname,
        uint32_t quality = ImageContentBlockSpec::kQualityUndefined,
        PixelFormat format = PixelFormat::UNDEFINED,
        uint32_t w = 0,
        uint32_t h = 0) {
      codecName.stage(cname);
      pixelFormat.set(format);
      width.set(w);
      height.set(h);
      codecQuality.set(quality);
      return *this;
    }

    DataPieceEnum<PixelFormat, ImageSpecType> pixelFormat{kImagePixelFormat};
    DataPieceValue<ImageSpecType> height{kImageHeight};
    DataPieceValue<ImageSpecType> width{kImageWidth};
    DataPieceString codecName{kImageCodecName};
    DataPieceValue<ImageSpecType> codecQuality{kImageCodecQuality};

    AutoDataLayoutEnd end;
  };

  // custom codec images require a codec name, but quality is optional
  EXPECT_TRUE(isImageSpec(
      {ImageFormat::CUSTOM_CODEC, "mycodec"},
      Spec().set("mycodec"),
      ImageFormat::CUSTOM_CODEC,
      123));
  EXPECT_TRUE(isImageSpec(
      {ImageFormat::CUSTOM_CODEC, "mycodec"},
      Spec().set("mycodec"),
      ImageContentBlockSpec("custom_codec/codec=mycodec2"),
      123));
  EXPECT_TRUE(isImageSpec(
      {ImageFormat::CUSTOM_CODEC,
       "mycodec",
       ImageContentBlockSpec::kQualityUndefined,
       PixelFormat::GREY8,
       100,
       100},
      Spec().set("mycodec", ImageContentBlockSpec::kQualityUndefined, PixelFormat::GREY8, 100, 100),
      ImageFormat::CUSTOM_CODEC,
      123));
  // fails because the codec name is empty
  EXPECT_FALSE(hasImageContentBlock(
      Spec().set("", 42, PixelFormat::GREY8, 100, 100), ImageFormat::CUSTOM_CODEC, 123));
}

TEST_F(ContentBlockReaderTest, videoImageSpecTest) {
  class VideoSpec : public AutoDataLayout {
   public:
    DataPieceEnum<PixelFormat, ImageSpecType> pixelFormat{kImagePixelFormat};
    DataPieceString codecName{kImageCodecName};
    DataPieceValue<ImageSpecType> height{kImageHeight};
    DataPieceValue<ImageSpecType> codecQuality{kImageCodecQuality};
    DataPieceValue<ImageSpecType> width{kImageWidth};

    AutoDataLayoutEnd end;

    void set(PixelFormat format, uint32_t w, uint32_t h) {
      pixelFormat.set(format);
      width.set(w);
      height.set(h);
    }
  };

  VideoSpec spec;
  EXPECT_FALSE(hasImageContentBlock(spec, ImageFormat::RAW, 123));
  spec.set(PixelFormat::GREY8, 100, 100);
  spec.codecQuality.set(ImageContentBlockSpec::kQualityUndefined);
  EXPECT_TRUE(isImageSpec({PixelFormat::GREY8, 100, 100}, spec, ImageFormat::RAW, 123));
  EXPECT_FALSE(isImageSpec(
      {ImageFormat::VIDEO, PixelFormat::GREY8, 100, 100}, spec, ImageFormat::VIDEO, 123));
  spec.codecName.stage("H.264");
  EXPECT_TRUE(isImageSpec(
      {ImageFormat::VIDEO, PixelFormat::GREY8, 100, 100, 0, 0, "H.264"},
      spec,
      ImageFormat::VIDEO,
      123));
  spec.codecQuality.set(23);
  EXPECT_TRUE(isImageSpec(
      {ImageFormat::VIDEO, PixelFormat::GREY8, 100, 100, 0, 0, "H.264", 23},
      spec,
      ImageFormat::VIDEO,
      123));
  spec.codecQuality.set(0);
  EXPECT_TRUE(isImageSpec(
      {ImageFormat::VIDEO, PixelFormat::GREY8, 100, 100, 0, 0, "H.264", 0},
      spec,
      ImageFormat::VIDEO,
      123));
  spec.codecQuality.set(101);
  EXPECT_TRUE(isImageSpec(
      {ImageFormat::VIDEO,
       PixelFormat::GREY8,
       100,
       100,
       0,
       0,
       "H.264",
       ImageContentBlockSpec::kQualityUndefined},
      spec,
      ImageFormat::VIDEO,
      123));
  spec.codecName.stage({});
  spec.codecQuality.set(5);
  EXPECT_FALSE(isImageSpec(
      {ImageFormat::VIDEO, PixelFormat::GREY8, 100, 100, 0, 0, {}, 5},
      spec,
      ImageFormat::VIDEO,
      123));
}

TEST_F(ContentBlockReaderTest, legacySpecTest) {
  class LegacySpec : public AutoDataLayout {
   public:
    DataPieceValue<ImageSpecType> bytesPerPixels{kImageBytesPerPixel};
    DataPieceValue<ImageSpecType> height{kImageHeight};
    DataPieceValue<ImageSpecType> width{kImageWidth};

    AutoDataLayoutEnd end;

    void set(uint8_t bpp, uint32_t w, uint32_t h) {
      bytesPerPixels.set(bpp);
      width.set(w);
      height.set(h);
    }
  };

  LegacySpec spec;
  spec.set(1, 100, 100);
  EXPECT_TRUE(isImageSpec({PixelFormat::GREY8, 100, 100}, spec, ImageFormat::RAW, 123));
  spec.set(3, 100, 100);
  EXPECT_TRUE(isImageSpec({PixelFormat::RGB8, 100, 100}, spec, ImageFormat::RAW, 123));
  spec.set(4, 100, 100);
  EXPECT_TRUE(isImageSpec({PixelFormat::DEPTH32F, 100, 100}, spec, ImageFormat::RAW, 123));
  spec.set(8, 100, 100);
  EXPECT_TRUE(isImageSpec({PixelFormat::SCALAR64F, 100, 100}, spec, ImageFormat::RAW, 123));

  class LegacySpec8 : public AutoDataLayout {
   public:
    DataPieceValue<ImageSpecType> height{kImageHeight};
    DataPieceValue<uint8_t> bytesPerPixels{kImageBytesPerPixel};
    DataPieceValue<ImageSpecType> width{kImageWidth};

    AutoDataLayoutEnd end;

    void set(uint8_t bpp, uint32_t w, uint32_t h) {
      bytesPerPixels.set(bpp);
      width.set(w);
      height.set(h);
    }
  };

  LegacySpec8 spec8;
  spec8.set(1, 100, 100);
  EXPECT_TRUE(isImageSpec({PixelFormat::GREY8, 100, 100}, spec8, ImageFormat::RAW, 123));
  spec8.set(3, 100, 100);
  EXPECT_TRUE(isImageSpec({PixelFormat::RGB8, 100, 100}, spec8, ImageFormat::RAW, 123));
  spec8.set(4, 100, 100);
  EXPECT_TRUE(isImageSpec({PixelFormat::DEPTH32F, 100, 100}, spec8, ImageFormat::RAW, 123));
  spec8.set(8, 100, 100);
  EXPECT_TRUE(isImageSpec({PixelFormat::SCALAR64F, 100, 100}, spec8, ImageFormat::RAW, 123));
}
