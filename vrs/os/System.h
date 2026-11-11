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

/// Get how much memory this process may use in total, in bytes: not how much is free now, but how
/// much the process could allocate before the system kills or throttles it. Use it to size caches,
/// buffers and queues. It is the size of the physical memory, limited:
/// - on Linux, by the memory limits of the process' cgroup and its ancestors (cgroup v2
///   memory.max and memory.high), as in containers;
/// - on Android and iOS, to 2 GiB, since these OSes kill apps well before they use all the memory.
/// Computed on the first call, then cached for the life of the process.
uint64_t getAvailableMemory();

} // namespace os
} // namespace vrs
