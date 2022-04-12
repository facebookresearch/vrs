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

#include <vrs/os/Platform.h>

#if IS_VRS_OSS_CODE()

#if defined(_MSC_VER) && !defined(__clang__)

#define VISIBILITY_DEFAULT __declspec(dllexport)
#define VISIBILITY_HIDDEN
#define FORCE_INLINE __forceinline
#define NOINLINE __declspec(noinline)
#define RESTRICT __restrict
#define CURRENT_FUNCTION __FUNCSIG__
#define UNREACHABLE_CODE() __assume(0)

#ifdef _CPPUNWIND
#define ARE_EXCEPTIONS_ENABLED() 1
#else
#define ARE_EXCEPTIONS_ENABLED() 0
#endif

#else // !_MSC_VER || __clang__

#define VISIBILITY_DEFAULT __attribute__((visibility("default")))
#define VISIBILITY_HIDDEN __attribute__((visibility("hidden")))
#define FORCE_INLINE __attribute__((always_inline)) inline
#define NOINLINE __attribute__((noinline))
#define RESTRICT __restrict__
#define CURRENT_FUNCTION __PRETTY_FUNCTION__
#define UNREACHABLE_CODE() __builtin_unreachable()

#if (defined(__cpp_exceptions) || defined(__EXCEPTIONS))
#define ARE_EXCEPTIONS_ENABLED() 1
#else
#define ARE_EXCEPTIONS_ENABLED() 0
#endif

#endif // !_MSC_VER || __clang__

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

#define DISABLE_THREAD_SAFETY_ANALYSIS
