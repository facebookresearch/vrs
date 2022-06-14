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
#include <vrs/utils/FilteredFileReader.h>
#include <vrs/utils/FrameRateEstimator.h>

using namespace std;
using namespace vrs;
using namespace vrs::utils;

struct FrameRateEstimatorTest : testing::Test {};

inline uint32_t intFps(const vector<IndexRecord::RecordInfo>& index, vrs::StreamId id) {
  return static_cast<uint32_t>(frameRateEstimationFps(index, id) + 0.5);
}

TEST_F(FrameRateEstimatorTest, frameRateEstimatorTest) {
  utils::FilteredFileReader filteredReaderTestOne;
  utils::FilteredFileReader filteredReaderTestTwo;
  filteredReaderTestOne.setSource(
      os::pathJoin(coretech::getTestDataDir(), "VRS_Files/sample_file.vrs"));
  filteredReaderTestTwo.setSource(
      os::pathJoin(coretech::getTestDataDir(), "VRS_Files/VRSTestRecording.vrs"));

  utils::RecordFilterParams filterParams;
  int statusOne = filteredReaderTestOne.openFile(filterParams);
  ASSERT_EQ(0, statusOne);
  const auto& index = filteredReaderTestOne.reader.getIndex();

  EXPECT_EQ(intFps(index, {RecordableTypeId::AudioStream, 1}), 90);
  EXPECT_EQ(intFps(index, {RecordableTypeId::ForwardCameraRecordableClass, 1}), 90);
  EXPECT_EQ(intFps(index, {RecordableTypeId::MotionRecordableClass, 1}), 90);

  int statusTwo = filteredReaderTestTwo.openFile(filterParams);
  ASSERT_EQ(0, statusTwo);
  const auto& index2 = filteredReaderTestTwo.reader.getIndex();

  EXPECT_EQ(intFps(index2, {RecordableTypeId::Proto0IMUHAL, 1}), 501);
  EXPECT_EQ(intFps(index2, {RecordableTypeId::Proto0ControllerSyncPulseHAL, 1}), 30);
  EXPECT_EQ(intFps(index2, {RecordableTypeId::Proto0CameraHALSlam, 1}), 30);
  EXPECT_EQ(intFps(index2, {RecordableTypeId::Proto0CameraHALSlam, 2}), 30);
  EXPECT_EQ(intFps(index2, {RecordableTypeId::Proto0CameraHALSlam, 3}), 30);
  EXPECT_EQ(intFps(index2, {RecordableTypeId::Proto0CameraHALSlam, 4}), 30);
}
