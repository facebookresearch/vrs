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
