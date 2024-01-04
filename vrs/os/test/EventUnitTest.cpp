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
#include <mutex>
#include <thread>
#include <utility>

#include <gtest/gtest.h>

#include <vrs/os/Event.h>
#include <vrs/os/Platform.h>

using namespace std;

using vrs::os::EventChannel;

void dispatchEvents(void* data, bool synchronous);
void waitOnEvents(void* data, const vector<double>& waitIntervals);

struct EventTest : public testing::Test {
  void TearDown() override {
    if (dispatchThread && dispatchThread->joinable()) {
      dispatchThread->join();
    }
    dispatchThread.reset();
    for (auto& thread : waitThreads) {
      if (thread->joinable()) {
        thread->join();
      }
    }
    waitThreads.clear();
    eventParams.clear();
    launched = false;
    launchReady = 0;
    launchTarget = 0;
  }

  void setupEvent(EventChannel::NotificationMode mode) {
    TearDown();

    testEventChannel.reset(new EventChannel("TestEventChannel", mode));
    EXPECT_NE(nullptr, testEventChannel);
  }

  void addEventInstance(double a_sleep_time_sec, void* data) {
    eventParams.emplace_back(a_sleep_time_sec, data);
  }

  void startDispatchThread(bool synchronous = false) {
    dispatchThread = make_unique<thread>(dispatchEvents, this, synchronous);
  }

  void startWaitThread(const vector<double>& waitIntervals) {
    waitThreads.push_back(make_unique<thread>(waitOnEvents, this, waitIntervals));
  }

  void waitForLaunch() {
    unique_lock<mutex> lock(launchLock);
    if (++launchReady == launchTarget && launchTarget > 0) {
      launched = true;
      launchCond.notify_all();
    } else {
      launchCond.wait(lock, [this] { return this->launched; });
    }
  }

  void launch() {
    unique_lock<mutex> lock(launchLock);
    launchTarget = (dispatchThread ? 1 : 0) + waitThreads.size();
    if (launchReady == launchTarget) {
      launched = true;
      launchCond.notify_all();
    } else {
      launchCond.wait(lock, [this] { return this->launched; });
    }
  }

  void runWaitAndDispatch(EventChannel::NotificationMode mode);
  void runDispatchAndWait(EventChannel::NotificationMode mode);
  void runDispatchAndWaitWithLookback(EventChannel::NotificationMode mode);
  void runNumPastEvents(EventChannel::NotificationMode mode);
  void runSpuriousWakeup(EventChannel::NotificationMode mode);

  const double lookback_time_sec{0.2}; // Look-back 200 ms.
  const double wait_time_sec{
      0.05}; // Assume 50 ms after threaded started, we are already waiting on event.
  unique_ptr<EventChannel> testEventChannel;
  mutex launchLock;
  condition_variable launchCond;
  uint32_t launchReady{0};
  uint32_t launchTarget{0};
  bool launched{false};
  unique_ptr<thread> dispatchThread;
  vector<unique_ptr<thread>> waitThreads;
  vector<pair<double, void*>> eventParams;
  EventChannel::Event event;
};

void dispatchEvents(void* data, bool synchronous) {
  EventTest* test = static_cast<EventTest*>(data);
  test->waitForLaunch();
  for (const auto& i : test->eventParams) {
    this_thread::sleep_for(chrono::duration<double>(i.first));
    if (synchronous) {
      // waiting for an event has the effect of completing any previous wakeups
      EventChannel::Event e;
      EXPECT_EQ(EventChannel::Status::TIMEOUT, test->testEventChannel->waitForEvent(e, 0.0, 0.0));
    }
    test->testEventChannel->dispatchEvent(i.second);
  }
}

void waitOnEvents(void* data, const vector<double>& waitIntervals) {
  EventTest* test = static_cast<EventTest*>(data);
  test->waitForLaunch();
  EventChannel::Event event;
  for (auto interval : waitIntervals) {
    this_thread::sleep_for(chrono::duration<double>(interval));
    EventChannel::Status status =
        test->testEventChannel->waitForEvent(event, EventChannel::kInfiniteTimeout, 0);
    EXPECT_EQ(EventChannel::Status::SUCCESS, status);
  }
}

void EventTest::runWaitAndDispatch(EventChannel::NotificationMode mode) {
  int a = 0;
  setupEvent(mode);
  addEventInstance(wait_time_sec, &a);
  startDispatchThread();
  launch();

  EXPECT_EQ(
      EventChannel::Status::SUCCESS,
      testEventChannel->waitForEvent(event, EventChannel::kInfiniteTimeout, 0.0));
  EXPECT_EQ(&a, event.pointer);
}

TEST_F(EventTest, WaitAndDispatchUnicast) {
  runWaitAndDispatch(EventChannel::NotificationMode::UNICAST);
}

TEST_F(EventTest, WaitAndDispatchBroadcast) {
  runWaitAndDispatch(EventChannel::NotificationMode::BROADCAST);
}

void EventTest::runDispatchAndWait(EventChannel::NotificationMode mode) {
  setupEvent(mode);
  addEventInstance(0.0, nullptr);
  startDispatchThread();
  launch();
  this_thread::sleep_for(chrono::duration<double>(wait_time_sec));

  EXPECT_EQ(EventChannel::Status::TIMEOUT, testEventChannel->waitForEvent(event, 0, 0.0));
  EXPECT_EQ(0, testEventChannel->getNumEventsSinceLastWait());
}

TEST_F(EventTest, DispatchAndWaitUnicast) {
  runDispatchAndWait(EventChannel::NotificationMode::UNICAST);
}

TEST_F(EventTest, DispatchAndWaitBroadcast) {
  runDispatchAndWait(EventChannel::NotificationMode::BROADCAST);
}

void EventTest::runDispatchAndWaitWithLookback(EventChannel::NotificationMode mode) {
  setupEvent(mode);
  addEventInstance(0.0, nullptr);
  EXPECT_EQ(0, testEventChannel->getNumEventsSinceLastWait());
  startDispatchThread();
  launch();
  this_thread::sleep_for(chrono::duration<double>(wait_time_sec));

  EXPECT_EQ(1, testEventChannel->getNumEventsSinceLastWait());
  EXPECT_EQ(
      EventChannel::Status::SUCCESS, testEventChannel->waitForEvent(event, 0, lookback_time_sec));
  EXPECT_EQ(0, testEventChannel->getNumEventsSinceLastWait());
  EXPECT_EQ(0, event.numMissedEvents);
}

#if !(IS_VRS_OSS_CODE() && IS_MAC_PLATFORM())
TEST_F(EventTest, DispatchAndWaitWithLookbackUnicast) {
  runDispatchAndWaitWithLookback(EventChannel::NotificationMode::UNICAST);
}

TEST_F(EventTest, DispatchAndWaitWithLookbackBroadcast) {
  runDispatchAndWaitWithLookback(EventChannel::NotificationMode::BROADCAST);
}

void EventTest::runNumPastEvents(EventChannel::NotificationMode mode) {
  setupEvent(mode);
  EXPECT_EQ(0, testEventChannel->getNumEventsSinceLastWait());

  addEventInstance(0.0, nullptr); // we will miss this event
  addEventInstance(0.0, nullptr); // we can get this event with look-back long enough
  startDispatchThread();
  launch();
  this_thread::sleep_for(chrono::duration<double>(wait_time_sec));

  EXPECT_EQ(2, testEventChannel->getNumEventsSinceLastWait());
  EXPECT_EQ(
      EventChannel::Status::SUCCESS, testEventChannel->waitForEvent(event, 0, lookback_time_sec));
  EXPECT_EQ(1, event.numMissedEvents);
  EXPECT_EQ(0, testEventChannel->getNumEventsSinceLastWait());
}

TEST_F(EventTest, NumPastEventsUnicast) {
  runNumPastEvents(EventChannel::NotificationMode::UNICAST);
}

TEST_F(EventTest, NumPastEventsBroadcast) {
  runNumPastEvents(EventChannel::NotificationMode::BROADCAST);
}
#endif

void EventTest::runSpuriousWakeup(EventChannel::NotificationMode mode) {
  setupEvent(mode);
  for (auto i = 0; i < 100; i++) {
    EXPECT_EQ(0, testEventChannel->getNumEventsSinceLastWait());
    EXPECT_EQ(EventChannel::Status::TIMEOUT, testEventChannel->waitForEvent(event, 0.02, 0));
  }
}

TEST_F(EventTest, SpuriousWakeupUnicast) {
  runSpuriousWakeup(EventChannel::NotificationMode::UNICAST);
}

TEST_F(EventTest, SpuriousWakeupBroadcast) {
  runSpuriousWakeup(EventChannel::NotificationMode::BROADCAST);
}

TEST_F(EventTest, MultipleListenersUnicast) {
  setupEvent(EventChannel::NotificationMode::UNICAST);

  size_t numWaiters = 30;

  addEventInstance(0.5, nullptr); // In 0.5 seconds, all the listeners should be waiting already.
  for (auto i = 0; i < numWaiters; ++i) {
    addEventInstance(0.0, nullptr); // Each waiter needs their own event (plus this thread's wait)
  }
  startDispatchThread(true);

  vector<double> waitIntervals(1, 0.0);
  for (auto i = 0; i < numWaiters; i++) {
    startWaitThread(waitIntervals);
  }
  launch();

  EventChannel::Event event;
  EventChannel::Status status =
      testEventChannel->waitForEvent(event, EventChannel::kInfiniteTimeout);
  EXPECT_EQ(EventChannel::Status::SUCCESS, status);
}

TEST_F(EventTest, MultipleListenersBroadcast) {
  setupEvent(EventChannel::NotificationMode::BROADCAST);

  addEventInstance(0.5, nullptr); // In 0.5 seconds, all the listeners should be waiting already.
  startDispatchThread();

  vector<double> waitIntervals(1, 0.0);
  for (auto i = 0; i < 30; i++) {
    startWaitThread(waitIntervals);
  }
  launch();

  EventChannel::Event event;
  EventChannel::Status status =
      testEventChannel->waitForEvent(event, EventChannel::kInfiniteTimeout);
  EXPECT_EQ(EventChannel::Status::SUCCESS, status);
}
