// Copyright (c) Facebook, Inc. and its affiliates. Confidential and proprietary.

#pragma once

#include <vrs/os/Platform.h>

// Utility Macros.
#if IS_WINDOWS_PLATFORM()
#define XR_UNLIKELY(x) !!(x)
#else // !IS_WINDOWS_PLATFORM()
// static branch prediction
#define XR_UNLIKELY(x) (__builtin_expect(!!(x), 0))
#endif // !IS_WINDOWS_PLATFORM()
