// (c) Facebook, Inc. and its affiliates. Confidential and proprietary.

#pragma once

#include <string>

#include <vrs/os/Platform.h>

#if IS_WINDOWS_PLATFORM()
#define CT_UNLIKELY(x) !!(x)
#else // !IS_WINDOWS_PLATFORM()
// static branch prediction
#define CT_UNLIKELY(x) (__builtin_expect(!!(x), 0))
#endif // !IS_WINDOWS_PLATFORM()

namespace vrs::logging {
void logAndAbort(const std::string& condition, const std::string& message = "");
} // namespace vrs::logging

//
// Check Macros.
//
#define XR_CHECK(condition, ...)                          \
  while (CT_UNLIKELY(!(condition))) {                     \
    vrs::logging::logAndAbort(#condition, ##__VA_ARGS__); \
  }                                                       \
  (void)0

#define XR_CHECK_EQ(val1, val2, ...) XR_CHECK((val1) == (val2), ##__VA_ARGS__)

#define XR_CHECK_NE(val1, val2, ...) XR_CHECK((val1) != (val2), ##__VA_ARGS__)

#define XR_CHECK_GE(val1, val2, ...) XR_CHECK((val1) >= (val2), ##__VA_ARGS__)

#define XR_CHECK_GT(val1, val2, ...) XR_CHECK((val1) > (val2), ##__VA_ARGS__)

#define XR_CHECK_LE(val1, val2, ...) XR_CHECK((val1) <= (val2), ##__VA_ARGS__)

#define XR_CHECK_LT(val1, val2, ...) XR_CHECK((val1) < (val2, ##__VA_ARGS__)

#define XR_CHECK_NOTNULL(val, ...) XR_CHECK((val) != nullptr, ##__VA_ARGS__)
#define XR_CHECK_TRUE(val, ...) XR_CHECK_EQ(true, static_cast<bool>(val), ##__VA_ARGS__)
#define XR_CHECK_FALSE(val, ...) XR_CHECK_EQ(false, static_cast<bool>(val), ##__VA_ARGS__)
