// Copyright (c) Facebook, Inc. and its affiliates. Confidential and proprietary.

#include "Checks.h"

#include <cstdlib>
#include <string>

#include <fmt/color.h>
#include <fmt/core.h>

// Abort Macro.
#if IS_ANDROID_PLATFORM()
#include <android/log.h>

// Logview requires __android_log_assert to distinguish each abort.
// NOTE: Size must be cast to `int` as that is what printf formatting functions expect; and `size_t`
// may have a different size than `int`.
#define XR_ABORT_IMPL(msg) \
  __android_log_assert(nullptr, "[VRS]", "%.*s", static_cast<int>(msg.size()), msg.data())

#else // !IS_ANDROID_PLATFORM()

#define XR_ABORT_IMPL(msg) std::abort()

#endif //! IS_ANDROID_PLATFORM()

namespace vrs::logging {
void logAndAbort(const std::string& condition, const std::string& message) {
  fmt::print(stderr, fg(fmt::color::red), "{} {}", condition, message);
  XR_ABORT_IMPL(message);
}
} // namespace vrs::logging
