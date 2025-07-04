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

#include <TestDataDir/TestDataDir.h>

#include <vrs/os/Utils.h>
#include <vrs/utils/ImageIndexer.h>
#include <vrs/utils/ImageLoader.h>
#include <vrs/utils/xxhash/xxhash.h>

#if IS_VRS_FB_INTERNAL()

using namespace std;
using namespace vrs;
using namespace vrs::utils;

struct ImageIndexerLoaderTest : testing::Test {
  string kRgbFile = os::pathJoin(coretech::getTestDataDir(), "VRS_Files/rgb8.vrs");
  string kJpgFile = os::pathJoin(coretech::getTestDataDir(), "VRS_Files/jpg.vrs");
};

// Test the loadImage API reading from a file
static int loadFrameFromFile(
    FileHandler& file,
    const DirectImageReference& image,
    const string& format,
    uint64_t hash) {
  PixelFrame frame;
  if (!loadImage(file, frame, image)) {
    return 2;
  }
  EXPECT_EQ(frame.getSpec().asString(), format);
  XXH64Digester digester;
  digester.ingest(frame.getBuffer());
  EXPECT_EQ(digester.digest(), hash);
  return 0;
}

// Test the loadImage API reading from a memory buffer
static int loadFrameFromMemory(
    FileHandler& file,
    const DirectImageReference& image,
    const string& format,
    uint64_t hash) {
  vector<char> buffer(image.dataSize);
  file.setPos(image.dataOffset);
  EXPECT_EQ(file.read(buffer.data(), buffer.size()), 0);
  PixelFrame frame;
  if (!loadImage(buffer, frame, image)) {
    return 2;
  }
  EXPECT_EQ(frame.getSpec().asString(), format);
  XXH64Digester digester;
  digester.ingest(frame.getBuffer());
  EXPECT_EQ(digester.digest(), hash);
  return 0;
}

TEST_F(ImageIndexerLoaderTest, ImageIndexerLoaderTest) {
  vector<DirectImageReferencePlus> readImages;

  ASSERT_EQ(indexImages(kRgbFile, readImages), 0);
  const string format = "raw/1224x1024/pixel=rgb8/stride=3672";
  vector<DirectImageReferencePlus> expectedImages = {
      {{RecordableTypeId::EyeTrackingCamera, 1},
       0,
       2251,
       2105916,
       format,
       CompressionType::Zstd,
       52,
       3760128},
      {{RecordableTypeId::EyeTrackingCamera, 1},
       1,
       2108199,
       2106944,
       format,
       CompressionType::Zstd,
       52,
       3760128},
      {{RecordableTypeId::EyeTrackingCamera, 1},
       2,
       4215175,
       2106022,
       format,
       CompressionType::Zstd,
       52,
       3760128},
  };
  ASSERT_EQ(readImages, expectedImages);
  unique_ptr<FileHandler> file = FileHandler::makeOpen(kRgbFile);
  ASSERT_NE(file, nullptr);
  EXPECT_EQ(loadFrameFromFile(*file, readImages[0], format, 4114475262886596638ULL), 0);
  EXPECT_EQ(loadFrameFromFile(*file, readImages[1], format, 16026781315276957005ULL), 0);
  EXPECT_EQ(loadFrameFromFile(*file, readImages[2], format, 8098506684566711634ULL), 0);
  EXPECT_EQ(loadFrameFromMemory(*file, readImages[1], format, 16026781315276957005ULL), 0);

  ASSERT_EQ(indexImages(kJpgFile, readImages), 0);
  expectedImages = {
      {{RecordableTypeId::RgbCameraRecordableClass, 1},
       0,
       6046,
       1985655,
       "jpg",
       CompressionType::None,
       0,
       0},
  };
  EXPECT_EQ(readImages, expectedImages);
  ASSERT_NE(file = FileHandler::makeOpen(kJpgFile), nullptr);
  EXPECT_EQ(loadFrameFromFile(*file, readImages[0], "jpg", 10323177114171200117ULL), 0);
  EXPECT_EQ(loadFrameFromMemory(*file, readImages[0], "jpg", 10323177114171200117ULL), 0);
}

#endif
