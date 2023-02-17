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

#include <gtest/gtest.h>

#include <vrs/os/Time.h>

struct TimeTest : testing::Test {};

TEST_F(TimeTest, getCurrentTimeSecTest) {
  // naive monotony test. Values should always grow.
  double lastTime = vrs::os::getTimestampSec();
  size_t negativeGapCount = 0; // should never ever happen!
  size_t positiveGapCount = 0;
  double minNonZeroGap = 1;
  double maxNonZeroGap = 0;
  const double kTestDurationSec = 1; // 1 sec
  double end = lastTime + kTestDurationSec;
  while (lastTime < end) {
    double now = vrs::os::getTimestampSec();
    double gap = now - lastTime;
    if (gap < 0) {
      negativeGapCount++;
    } else if (gap > 0) {
      positiveGapCount++;
      if (gap < minNonZeroGap) {
        minNonZeroGap = gap;
      }
      if (gap > maxNonZeroGap) {
        maxNonZeroGap = gap;
      }
    }
    lastTime = now;
  }
  EXPECT_EQ(negativeGapCount, 0); // never OK to have a negative gap
  EXPECT_GT(positiveGapCount, 100);
  const double kRequiredMinNonZeroGapSec = 0.001; // 1 ms
  EXPECT_GT(kRequiredMinNonZeroGapSec, minNonZeroGap);
}

TEST_F(TimeTest, getCurrentTimeSecSinceEpoch) {
  // merely make sure that the implementation isn't completely busted...
  int64_t now = vrs::os::getCurrentTimeSecSinceEpoch();
  const int64_t jan1_2023 = 1672560000;
  const int64_t jan1_2040 = 2209017600;
  EXPECT_GT(now, jan1_2023);
  EXPECT_GT(jan1_2040, now);
}
