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

#define DEFAULT_LOG_CHANNEL "ThrottlerTest"
#include <logging/Log.h>

#include <vrs/helpers/Throttler.h>

struct ThrottlerTest : testing::Test {};

using namespace std;
using namespace vrs;
using namespace vrs::utils;

TEST_F(ThrottlerTest, frequencyTest) {
  EXPECT_EQ(Throttler::reportFrequency(0), 1);
  EXPECT_EQ(Throttler::reportFrequency(1), 1);
  EXPECT_EQ(Throttler::reportFrequency(10), 1);
  EXPECT_EQ(Throttler::reportFrequency(11), 10);
  EXPECT_EQ(Throttler::reportFrequency(100), 10);
  EXPECT_EQ(Throttler::reportFrequency(101), 100);
  EXPECT_EQ(Throttler::reportFrequency(1000), 100);
  EXPECT_EQ(Throttler::reportFrequency(1001), 1000);
  EXPECT_EQ(Throttler::reportFrequency(10000), 1000);
  EXPECT_EQ(Throttler::reportFrequency(10001), 10000);
  EXPECT_EQ(Throttler::reportFrequency(100000), 10000);
  EXPECT_EQ(Throttler::reportFrequency(100001), 100000);
}

TEST_F(ThrottlerTest, throttleTest) {
  Throttler throttler;
  int counter = 0;
  for (int k = 0; k < 100000; k++) {
    if (throttler.report(__LINE__)) {
      XR_LOGW("Condition failed report #{}", ++counter);
    }
  }
  EXPECT_EQ(counter, 55);
}
