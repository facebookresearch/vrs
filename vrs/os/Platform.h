// Copyright (c) Facebook, Inc. and its affiliates. Confidential and proprietary.

#pragma once

/// Resolve unambiguously what platform & environment the code is compiling for
/// IS_VRS_OSS_CODE()
/// -> tell if compiling in Open Source Software world (equivalent: IS_VRS_FB_INTERNAL() is false)
/// IS_VRS_FB_INTERNAL()
/// -> tell if compiling in Facebook internal world (equivalent: IS_VRS_OSS_CODE() is false)
///
/// IS_ANDROID_PLATFORM()
/// -> are we compiling for the Android platform?
/// IS_LINUX_PLATFORM()
/// -> are we compiling for the Linux platform? (Note: false for Android)
/// IS_APPLE_PLATFORM()
/// -> are we compiling for an "Apple" platform of some kind, Mac, iOS or other?
/// IS_MAC_PLATFORM()
/// -> are we compiling for the MacOS desktop platform? (implies IS_APPLE_PLATFORM() is true)
/// IS_IOS_PLATFORM()
/// -> are we compiling for the iOS mobile platform? (implies IS_APPLE_PLATFORM() is true)
/// IS_WINDOWS_PLATFORM()
/// -> are we compiling for the Windows desktop platform?

#ifdef OSS_CMAKE_OVERRIDE
#define IS_VRS_OSS_CODE() 1
#define IS_VRS_FB_INTERNAL() 0

#else
// @oss-disable: #define IS_VRS_OSS_CODE() 0
// @oss-disable: #define IS_VRS_FB_INTERNAL() 1
#define IS_VRS_OSS_CODE() 1 // @oss-enable
#define IS_VRS_FB_INTERNAL() 0 // @oss-enable
#endif

#if IS_VRS_FB_INTERNAL()
#include <portability/Platform.h>

#else // IS_VRS_OSS_CODE()

#ifdef __linux__

#if defined(__ANDROID__)
// Android-flavored Linux

#define IS_ANDROID_PLATFORM() 1
#define IS_LINUX_PLATFORM() 0
#define IS_APPLE_PLATFORM() 0
#define IS_MAC_PLATFORM() 0
#define IS_IOS_PLATFORM() 0
#define IS_WINDOWS_PLATFORM() 0

#else // !defined(__ANDROID__)
// Linux

#define IS_ANDROID_PLATFORM() 0
#define IS_LINUX_PLATFORM() 1
#define IS_APPLE_PLATFORM() 0
#define IS_MAC_PLATFORM() 0
#define IS_IOS_PLATFORM() 0
#define IS_WINDOWS_PLATFORM() 0

#endif // !defined(__ANDROID__)

#elif defined(__APPLE__)
// Apple

#define IS_ANDROID_PLATFORM() 0
#define IS_LINUX_PLATFORM() 0
#define IS_APPLE_PLATFORM() 1
#define IS_WINDOWS_PLATFORM() 0

// need to distinguish between MacOS and iOS
#include <TargetConditionals.h>

#if TARGET_OS_IPHONE
// Apple: iPhone

#define IS_MAC_PLATFORM() 0
#define IS_IOS_PLATFORM() 1

#elif TARGET_OS_MAC
// Apple: Mac

#define IS_MAC_PLATFORM() 1
#define IS_IOS_PLATFORM() 0

#else
#error "Unsupported Apple platform"

#endif // TARGET_OS

#elif defined(_WIN32) || defined(_WIN64)
// Windows

#define IS_ANDROID_PLATFORM() 0
#define IS_LINUX_PLATFORM() 0
#define IS_APPLE_PLATFORM() 0
#define IS_MAC_PLATFORM() 0
#define IS_IOS_PLATFORM() 0
#define IS_WINDOWS_PLATFORM() 1

#else // !ANDROID && !LINUX && !APPLE && !WINDOWS && !MAC && &!IOS

#error "Unsupported/unrecognized platform"

#endif

#endif // IS_VRS_OSS_CODE()

#if IS_APPLE_PLATFORM() || IS_WINDOWS_PLATFORM() || IS_LINUX_PLATFORM() || IS_ANDROID_PLATFORM()

#define IS_VRS_OSS_TARGET_PLATFORM() 1

#else // Not a platform VRS OSS plans to support

#define IS_VRS_OSS_TARGET_PLATFORM() 0

#endif
