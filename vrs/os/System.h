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

#include <string>

namespace vrs {
namespace os {

std::string getOsFingerPrint();
std::string getUniqueSessionId();
uint32_t getTerminalWidth(uint32_t setValue = 0); // option to set value for tests

/// Get the number of threads this process can run in parallel, at least 1. Use it to size thread
/// pools instead of std::thread::hardware_concurrency(), which returns the number of CPUs of the
/// host, even when the process is limited to fewer CPUs, as in containers.
/// On Linux, the result is also limited by the CPU affinity of the calling thread, and by the CPU
/// quota of the process' cgroup and its ancestors (cgroup v2).
/// Computed on the first call, then cached for the life of the process: later changes of affinity
/// or quota are ignored.
uint32_t getAvailableConcurrency();

} // namespace os
} // namespace vrs
