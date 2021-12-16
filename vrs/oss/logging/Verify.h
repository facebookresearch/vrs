// Copyright (c) Meta Platforms, Inc. and affiliates.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#pragma once

#include <fmt/color.h>
#include <fmt/core.h>

#include <vrs/os/CompilerAttributes.h>

#include "LogLevel.h"

#ifndef DEFAULT_LOG_CHANNEL
#error "DEFAULT_LOG_CHANNEL must be defined before including <logging/Verify.h>"
#endif // DEFAULT_LOG_CHANNEL

//
// Verify Macros.
// These macros print the message when condition evaluates to false, and return the evaluation of
// condition.
//

#define XR_VERIFY_C(channel_, condition_, ...)  \
  (condition_ ? 1                               \
              : (fmt::print(                    \
                     stderr,                    \
                     fg(fmt::color::red),       \
                     "Verify {} failed: {}",    \
                     #condition_,               \
                     fmt::format(__VA_ARGS__)), \
                 0))

#define XR_VERIFY(cond, ...) XR_VERIFY_C(DEFAULT_LOG_CHANNEL, cond, ##__VA_ARGS__, "")
