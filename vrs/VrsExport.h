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

#include <vrs/os/Platform.h>

// Cross-platform export annotation for the `vrs_shared` library. Decorate every public class and
// free function with VRS_API so a single shared library exports the whole VRS API, giving one copy
// of every symbol process-wide. The shared library is compiled with VRS_API_SHARED_BUILD (export);
// its consumers compile with VRS_API_SHARED_IMPORT (import). The default static `:vrs` build
// defines neither, so VRS_API expands to nothing and that build is unchanged.
#if IS_WINDOWS_PLATFORM()
#if defined(VRS_API_SHARED_BUILD)
#define VRS_API __declspec(dllexport)
#elif defined(VRS_API_SHARED_IMPORT)
#define VRS_API __declspec(dllimport)
#else
#define VRS_API
#endif
#else
#if defined(VRS_API_SHARED_BUILD) || defined(VRS_API_SHARED_IMPORT)
#define VRS_API __attribute__((visibility("default")))
#else
#define VRS_API
#endif
#endif
