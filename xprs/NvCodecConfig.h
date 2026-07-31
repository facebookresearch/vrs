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

// Single source of truth for NVIDIA codec compilation guards.
//
// `WITH_NVCODEC` is the build-system flag (Buck preprocessor flag, CMake
// `option(ENABLE_NVCODEC ...)`). Source files should use the self-documenting
// macros below instead of `WITH_NVCODEC` so it's clear at the call site
// whether the gated code requires NVDEC (decode), NVENC (encode), or both.
//
// Both macros are defined together when WITH_NVCODEC is set. The split into
// separate macros prepares for build configurations that ship only one half
// (e.g., OSS distributions that include NVDEC but omit NVENC due to driver,
// hardware, or licensing constraints).

#ifdef WITH_NVCODEC
#define XPRS_HAS_NVDEC
#define XPRS_HAS_NVENC
#endif
