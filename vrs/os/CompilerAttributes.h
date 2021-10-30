// Copyright (c) Facebook, Inc. and its affiliates. Confidential and proprietary.

#pragma once

#include <vrs/os/Platform.h>

#if IS_VRS_OSS_CODE()
#define DISABLE_THREAD_SAFETY_ANALYSIS
#else
#include <vrs/os/CompilerAttributes_fb.h>
#endif
