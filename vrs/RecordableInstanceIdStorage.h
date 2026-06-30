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

#include <cstdint>
#include <map>
#include <mutex>

#include <vrs/StreamId.h>
#include <vrs/os/Platform.h>

// The instance-id counter that backs Recordable::getNewInstanceId() lives behind these accessors so
// it can be linked as either a static archive or a single shared library. When the same process
// statically links VRS into several shared libraries, routing every copy of the allocation logic to
// one shared definition of this counter keeps Recordable StreamIds unique process-wide. Only the
// accessors are exported; the counter itself stays internal to the owning translation unit.
#if IS_WINDOWS_PLATFORM()
#if defined(VRS_INSTANCE_ID_SHARED_BUILD)
#define VRS_INSTANCE_ID_API __declspec(dllexport)
#else
#define VRS_INSTANCE_ID_API
#endif
#else
#define VRS_INSTANCE_ID_API __attribute__((visibility("default")))
#endif

namespace vrs {

/// Mutex guarding the process instance-id counter; shared by every copy of the allocation logic.
VRS_INSTANCE_ID_API std::recursive_mutex& getInstanceIdMutex();

/// The per-RecordableTypeId instance-id counter. Access only while holding getInstanceIdMutex().
VRS_INSTANCE_ID_API std::map<RecordableTypeId, uint16_t>& getInstanceIdMap();

} // namespace vrs
