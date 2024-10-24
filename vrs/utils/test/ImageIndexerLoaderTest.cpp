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

using namespace std;
using namespace vrs;
using namespace vrs::utils;

struct ImageIndexerLoaderTest : testing::Test {
  string kRgbFile = os::pathJoin(coretech::getTestDataDir(), "VRS_Files/rgb8.vrs");
  string kJpgFile = os::pathJoin(coretech::getTestDataDir(), "VRS_Files/jpg.vrs");
};

TEST_F(ImageIndexerLoaderTest, ImageIndexerLoaderTest) {
  vector<DirectImageReference> readImages;

  ASSERT_EQ(indexImages(kRgbFile, readImages), 0);
  const string format = "raw/1224x1024/pixel=rgb8/stride=3672";
  vector<DirectImageReference> expectedImages = {
      {2251, 2105916, format, CompressionType::Zstd, 52, 3760128},
      {2108199, 2106944, format, CompressionType::Zstd, 52, 3760128},
      {4215175, 2106022, format, CompressionType::Zstd, 52, 3760128},
  };
  EXPECT_EQ(readImages, expectedImages);

  ASSERT_EQ(indexImages(kJpgFile, readImages), 0);
  expectedImages = {
      {6046, 1985655, "jpg", CompressionType::None, 0, 0},
  };
  EXPECT_EQ(readImages, expectedImages);
}
