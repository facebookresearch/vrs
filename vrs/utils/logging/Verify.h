// Copyright (c) Facebook, Inc. and its affiliates. Confidential and proprietary.

#pragma once

#include <fmt/color.h>
#include <fmt/core.h>

#include "Utils.h"

#ifndef DEFAULT_LOG_CHANNEL
#error "DEFAULT_LOG_CHANNEL must be defined before including <logging/Verify.h>"
#endif // DEFAULT_LOG_CHANNEL

//
// Verify Macros.
// These macros print the message when condition evaluates to false, and return the evaluation of
// condition.
//

#define XR_VERIFY_C(channel_, condition_, ...) \
  if (XR_UNLIKELY(!(condition_))) {            \
    fmt::print(                                \
        stderr,                                \
        fg(fmt::color::red),                   \
        "Verify {} failed: {}",                \
        #condition_,                           \
        fmt::format(__VA_ARGS__));             \
  }                                            \
  return condition_;

#define XR_VERIFY(cond, ...) XR_VERIFY_C(DEFAULT_LOG_CHANNEL, cond, ##__VA_ARGS__)
