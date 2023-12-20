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

#include "Checks.h"

#include <cstdlib>

#include <fmt/color.h>
#include <fmt/core.h>

#include <vrs/os/Platform.h>

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

namespace vrs {
namespace logging {

void logAndAbort(const char* condition, const std::string& message) {
  fmt::print(stderr, fg(fmt::color::red), "Check '{}' failed. {}\n", condition, message);
  XR_ABORT_IMPL(message);
}

} // namespace logging
} // namespace vrs
