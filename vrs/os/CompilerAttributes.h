// Copyright (c) Facebook, Inc. and its affiliates. Confidential and proprietary.

#pragma once

#include <vrs/os/Platform.h>

#if IS_VRS_OSS_CODE()
#ifdef _MSC_VER

#if _MSVC_LANG >= 201703L
#define HAS_CPP_17() 1
#else
#define HAS_CPP_17() 0
#endif

#else // !_MSC_VER

#if __cplusplus >= 201703L
#define HAS_CPP_17() 1
#else
#define HAS_CPP_17() 0
#endif

#endif // !_MSC_VER

#if HAS_CPP_17()
#define MAYBE_UNUSED [[maybe_unused]]

#else // !HAS_CPP_17()

#ifdef __has_cpp_attribute

#if __has_cpp_attribute(maybe_unused)
#define MAYBE_UNUSED [[maybe_unused]]
#elif __has_cpp_attribute(gnu::unused)
#define MAYBE_UNUSED [[gnu::unused]]
#endif

#else // !__has_cpp_attribute

#define MAYBE_UNUSED

#endif // !__has_cpp_attribute

#endif // !HAS_CPP_17()

// static branch prediction
#if defined(__clang__) || defined(__GNUC__)
#define XR_UNLIKELY(x) (__builtin_expect(!!(x), 0))
#else // !(clang or gcc)
#define XR_UNLIKELY(x) !!(x)
#endif // !(clang or gcc)

#else
#include <portability/CompilerAttributes.h>
#endif

// XROS builds with -Wthread-safety-analysis, which (correctly) flags that the DataLayouter mutex
// can be used in an unsafe fashion.
#if IS_XROS_PLATFORM()
#define DISABLE_THREAD_SAFETY_ANALYSIS __attribute__((no_thread_safety_analysis))
#else // !IS_XROS_PLATFORM()
#define DISABLE_THREAD_SAFETY_ANALYSIS
#endif // !IS_XROS_PLATFORM()
