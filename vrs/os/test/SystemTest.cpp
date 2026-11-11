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

#include <thread>

#include <gtest/gtest.h>

#include <vrs/os/System.h>

using namespace std;
using namespace vrs;

TEST(SystemTest, availableConcurrency) {
  uint32_t concurrency = os::getAvailableConcurrency();
  EXPECT_GE(concurrency, 1);
  EXPECT_LE(concurrency, max<uint32_t>(thread::hardware_concurrency(), 1));
  EXPECT_EQ(os::getAvailableConcurrency(), concurrency); // cached
}
