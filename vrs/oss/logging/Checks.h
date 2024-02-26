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

#include <string>

#include <fmt/core.h>

namespace vrs {
namespace logging {

void logAndAbort(const char* condition, const std::string& message = {});

} // namespace logging
} // namespace vrs

//
// Check Macros.
//

#define XR_CHECK_FORMAT(condition, fmtstr, ...) \
  (condition ? 0 : ((vrs::logging::logAndAbort(#condition, fmt::format(fmtstr, ##__VA_ARGS__))), 0))

#define XR_CHECK(condition, ...) XR_CHECK_FORMAT(condition, "" __VA_ARGS__)

#define XR_CHECK_EQ(val1, val2, ...) XR_CHECK((val1) == (val2), ##__VA_ARGS__)

#define XR_CHECK_NE(val1, val2, ...) XR_CHECK((val1) != (val2), ##__VA_ARGS__)

#define XR_CHECK_GE(val1, val2, ...) XR_CHECK((val1) >= (val2), ##__VA_ARGS__)

#define XR_CHECK_GT(val1, val2, ...) XR_CHECK((val1) > (val2), ##__VA_ARGS__)

#define XR_CHECK_LE(val1, val2, ...) XR_CHECK((val1) <= (val2), ##__VA_ARGS__)

#define XR_CHECK_LT(val1, val2, ...) XR_CHECK((val1) < (val2), ##__VA_ARGS__)

#define XR_CHECK_NOTNULL(val, ...) XR_CHECK((val) != nullptr, ##__VA_ARGS__)

#define XR_CHECK_TRUE(val, ...) XR_CHECK_EQ(true, static_cast<bool>(val), ##__VA_ARGS__)
#define XR_CHECK_FALSE(val, ...) XR_CHECK_EQ(false, static_cast<bool>(val), ##__VA_ARGS__)

#define XR_FATAL_ERROR(...) UNREACHABLE_CODE()

#define XR_DEV_CHECK(condition, ...) XR_CHECK(condition, ##__VA_ARGS__)
#define XR_DEV_CHECK_EQ(val1, val2, ...) XR_CHECK_EQ(val1, val2, ##__VA_ARGS__)
#define XR_DEV_CHECK_NE(val1, val2, ...) XR_CHECK_NE(val1, val2, ##__VA_ARGS__)
#define XR_DEV_CHECK_GE(val1, val2, ...) XR_CHECK_GE(val1, val2, ##__VA_ARGS__)
#define XR_DEV_CHECK_GT(val1, val2, ...) XR_CHECK_GT(val1, val2, ##__VA_ARGS__)
#define XR_DEV_CHECK_LE(val1, val2, ...) XR_CHECK_LE(val1, val2, ##__VA_ARGS__)
#define XR_DEV_CHECK_LT(val1, val2, ...) XR_CHECK_LT(val1, val2, ##__VA_ARGS__)
#define XR_DEV_CHECK_NOTNULL(val, ...) XR_CHECK_NOTNULL(val, ##__VA_ARGS__)
#define XR_DEV_CHECK_TRUE(val, ...) XR_CHECK_TRUE(val, ##__VA_ARGS__)
#define XR_DEV_CHECK_FALSE(val, ...) XR_CHECK_FALSE(val, ##__VA_ARGS__)
#define XR_DEV_CHECK_GE_LT(val1, val2, val3, ...) XR_CHECK_GE_LT(val1, val2, val3, ##__VA_ARGS__)
#define XR_DEV_CHECK_GE_LE(val1, val2, val3, ...) XR_CHECK_GE_LE(val1, val2, val3, ##__VA_ARGS__)
#define XR_DEV_FATAL_ERROR(...) XR_FATAL_ERROR(__VA_ARGS__)

// XR_PRECONDITION_NOTNULL performs a not-null check but returns the value,
// so this macro can be used in initializer lists
#define XR_PRECONDITION_NOTNULL(_val, ...) ((XR_CHECK_NOTNULL(_val, ##__VA_ARGS__)), _val)

// XR_DEV_PRECONDITION_NOTNULL is the same as XR_PRECONDITION_NOTNULL,
// except that the check is compiled out in production builds
#define XR_DEV_PRECONDITION_NOTNULL(_val, ...) ((XR_DEV_CHECK_NOTNULL(_val, ##__VA_ARGS__)), _val)
