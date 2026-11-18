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

#include <atomic>
#include <chrono>
#include <thread>

#include <vrs/helpers/WaitableCounter.h>

using namespace std;
using namespace vrs;

namespace {
constexpr double kLongTimeoutSec = 10;
constexpr auto kWaiterHeadStart = chrono::milliseconds(20);
} // namespace

TEST(WaitableCounterTest, addSubReset) {
  WaitableCounter counter;
  EXPECT_EQ(counter.get(), 0);
  counter.add(100);
  counter.sub(30);
  EXPECT_EQ(counter.get(), 70);
  counter.reset();
  EXPECT_EQ(counter.get(), 0);
}

TEST(WaitableCounterTest, returnsAtOrBelowThreshold) {
  WaitableCounter counter;
  counter.add(100);
  counter.waitForAtMost(100);
  EXPECT_TRUE(counter.waitForAtMost(100, 0));
  EXPECT_TRUE(counter.waitForAtMost(200, 0));
}

TEST(WaitableCounterTest, timesOutAboveThreshold) {
  WaitableCounter counter;
  counter.add(100);
  EXPECT_FALSE(counter.waitForAtMost(99, 0.01));
}

TEST(WaitableCounterTest, subWakesWaiterOnceAtThreshold) {
  WaitableCounter counter;
  counter.add(100);
  atomic<bool> done{false};
  thread waiter([&] {
    counter.waitForAtMost(50);
    done = true;
  });
  // Let the waiter block, so the notification is what wakes it.
  // NOLINTNEXTLINE(facebook-hte-BadCall-sleep_for)
  this_thread::sleep_for(kWaiterHeadStart);
  counter.sub(30);
  // Give a wrongly woken waiter the time to set done. A slow thread can only hide a bug, not fail.
  // NOLINTNEXTLINE(facebook-hte-BadCall-sleep_for)
  this_thread::sleep_for(kWaiterHeadStart);
  EXPECT_FALSE(done);
  counter.sub(20);
  waiter.join();
  EXPECT_TRUE(done);
}

TEST(WaitableCounterTest, resetWakesTimedWaiterBeforeTimeout) {
  WaitableCounter counter;
  counter.add(100);
  bool result = false;
  chrono::duration<double> waitTime{};
  thread waiter([&] {
    const auto start = chrono::steady_clock::now();
    result = counter.waitForAtMost(0, kLongTimeoutSec);
    waitTime = chrono::steady_clock::now() - start;
  });
  // Let the waiter block, so the notification is what wakes it.
  // NOLINTNEXTLINE(facebook-hte-BadCall-sleep_for)
  this_thread::sleep_for(kWaiterHeadStart);
  counter.reset();
  waiter.join();
  EXPECT_TRUE(result);
  EXPECT_LT(waitTime.count(), kLongTimeoutSec);
}
