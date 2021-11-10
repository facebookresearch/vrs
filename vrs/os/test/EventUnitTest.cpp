// Copyright (c) Facebook, Inc. and its affiliates. Confidential and proprietary.

#include <thread>
#include <utility>

#include <gtest/gtest.h>

#include <vrs/os/Event.h>

using namespace std;

using vrs::os::EventChannel;

void* dispatchEvents(void* data, bool synchronous);
void waitOnEvents(void* data, vector<double> waitIntervals);

struct EventTest : public testing::Test {
  virtual void TearDown() {
    if (dispatchThread.get() != nullptr && dispatchThread->joinable()) {
      dispatchThread->join();
    }
    for (auto&& thread : waitThreads) {
      if (thread->joinable()) {
        thread->join();
      }
    }
  }

  EventTest()
      : lookback_time_sec(0.1),
        wait_time_sec(0.05),
        launchReady(0),
        launchTarget(0),
        launched(false) {}

  void setupEvent(EventChannel::NotificationMode mode) {
    testEventChannel.reset(new EventChannel("TestEventChannel", mode));
    EXPECT_NE(nullptr, testEventChannel);
  }

  void addEventInstance(double a_sleep_time_sec, void* data) {
    eventParams.push_back(pair<double, void*>(a_sleep_time_sec, data));
  }

  void startDispatchThread(bool synchronous = false) {
    dispatchThread = std::make_unique<std::thread>(dispatchEvents, this, synchronous);
  }

  void startWaitThread(vector<double>& waitIntervals) {
    waitThreads.push_back(std::make_unique<std::thread>(waitOnEvents, this, waitIntervals));
  }

  void waitForLaunch() {
    std::unique_lock<std::mutex> lock(launchLock);
    ++launchReady;
    if (launchReady == launchTarget) {
      launched = true;
      launchCond.notify_all();
    } else {
      launchCond.wait(lock, [=] { return launched; });
    }
  }

  void launch() {
    std::unique_lock<std::mutex> lock(launchLock);
    launchTarget = (dispatchThread ? 1 : 0) + waitThreads.size();
    if (launchReady == launchTarget) {
      launched = true;
      launchCond.notify_all();
    } else {
      launchCond.wait(lock, [=] { return launched; });
    }
  }

  void runWaitAndDispatch(EventChannel::NotificationMode mode);
  void runDispatchAndWait(EventChannel::NotificationMode mode);
  void runDispatchAndWaitWithLookback(EventChannel::NotificationMode mode);
  void runNumPastEvents(EventChannel::NotificationMode mode);
  void runSpuriousWakeup(EventChannel::NotificationMode mode);

  const double lookback_time_sec; // Look-back 100 ms.
  const double
      wait_time_sec; // Assume 50 ms after threaded started, we are already waiting on event.
  unique_ptr<EventChannel> testEventChannel;
  std::mutex launchLock;
  std::condition_variable launchCond;
  uint32_t launchReady;
  uint32_t launchTarget;
  bool launched;
  unique_ptr<std::thread> dispatchThread;
  vector<unique_ptr<std::thread>> waitThreads;
  vector<pair<double, void*>> eventParams;
  EventChannel::Event event;
};

void* dispatchEvents(void* data, bool synchronous) {
  EventTest* test = static_cast<EventTest*>(data);
  test->waitForLaunch();
  for (auto i : test->eventParams) {
    std::this_thread::sleep_for(std::chrono::duration<double>(i.first));
    if (synchronous) {
      // waiting for an event has the effect of completing any previous wakeups
      EventChannel::Event e;
      EXPECT_EQ(EventChannel::Status::TIMEOUT, test->testEventChannel->waitForEvent(e, 0.0, 0.0));
    }
    test->testEventChannel->dispatchEvent(i.second);
  }
  return nullptr;
}

void waitOnEvents(void* data, vector<double> waitIntervals) {
  EventTest* test = static_cast<EventTest*>(data);
  test->waitForLaunch();
  EventChannel::Event event;
  for (auto interval : waitIntervals) {
    std::this_thread::sleep_for(std::chrono::duration<double>(interval));
    EventChannel::Status status =
        test->testEventChannel->waitForEvent(event, EventChannel::kInfiniteTimeout, 0);
    EXPECT_EQ(EventChannel::Status::SUCCESS, status);
  }
}

void EventTest::runWaitAndDispatch(EventChannel::NotificationMode mode) {
  int a;
  setupEvent(mode);
  addEventInstance(wait_time_sec, static_cast<void*>(&a));
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
  std::this_thread::sleep_for(std::chrono::duration<double>(wait_time_sec));

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
  std::this_thread::sleep_for(std::chrono::duration<double>(wait_time_sec));

  EXPECT_EQ(1, testEventChannel->getNumEventsSinceLastWait());
  EXPECT_EQ(
      EventChannel::Status::SUCCESS, testEventChannel->waitForEvent(event, 0, lookback_time_sec));
  EXPECT_EQ(0, testEventChannel->getNumEventsSinceLastWait());
  EXPECT_EQ(0, event.numMissedEvents);
}

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
  std::this_thread::sleep_for(std::chrono::duration<double>(wait_time_sec));

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
