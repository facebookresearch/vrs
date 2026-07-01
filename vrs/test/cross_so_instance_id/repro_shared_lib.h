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

#include <memory>

#include <vrs/Recordable.h>
#include <vrs/os/Platform.h>

// Exported so the test binary can link across the shared-library boundary on every platform
// (Windows and macOS do not export shared-library symbols by default).
#if IS_WINDOWS_PLATFORM()
#if defined(REPRO_SHARED_LIB_BUILDING)
#define REPRO_SHARED_LIB_API __declspec(dllexport)
#else
#define REPRO_SHARED_LIB_API __declspec(dllimport)
#endif
#else
#define REPRO_SHARED_LIB_API __attribute__((visibility("default")))
#endif

namespace vrs::test {

// Builds a Recordable inside this shared library, which links the shared VRS library (vrs_shared)
// dynamically — the same single VRS .so the test binary links — so both draw instance ids from one
// process-wide counter.
REPRO_SHARED_LIB_API std::shared_ptr<::vrs::Recordable> makeSharedLibRecordable();

} // namespace vrs::test
