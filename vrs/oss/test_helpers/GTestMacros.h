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

#if IS_MOBILE_PLATFORM()
#define MOBILE_DISABLED(x) DISABLE_TEST(x)
#else
#define MOBILE_DISABLED(x) x
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
