// Copyright (c) Facebook, Inc. and its affiliates. Confidential and proprietary.

#pragma once

#include <vrs/os/Platform.h>

// Platform specific macros to disable unit tests:
// Usage:
//   TEST(MyTestSuite, DISABLE_TEST(testingThings)) { ... }
//   TEST(MyTestSuite, ANDROID_DISABLE(testingThings)) { ... }
#define DISABLE_TEST(x) DISABLED_##x

#if IS_ANDROID_PLATFORM()
#define ANDROID_DISABLED(x) DISABLE_TEST(x)
#else
#define ANDROID_DISABLED(x) x
#endif

#if IS_WINDOWS_PLATFORM()
#define WINDOWS_DISABLED(x) DISABLE_TEST(x)
#else
#define WINDOWS_DISABLED(x) x
#endif

// ASSERT_EQ can't be used from a nested function if the function does not return void
#define RETURN_ON_FAILURE(operation) \
  {                                  \
    int result = (operation);        \
    EXPECT_EQ(result, 0);            \
    if (result != 0) {               \
      return result;                 \
    }                                \
  }
