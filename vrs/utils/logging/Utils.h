// (c) Facebook, Inc. and its affiliates. Confidential and proprietary.

#pragma once

#include <string>

#include <vrs/os/Platform.h>

// Utility Macros.
#if IS_WINDOWS_PLATFORM()
#define CT_UNLIKELY(x) !!(x)
#else // !IS_WINDOWS_PLATFORM()
// static branch prediction
#define CT_UNLIKELY(x) (__builtin_expect(!!(x), 0))
#endif // !IS_WINDOWS_PLATFORM()
