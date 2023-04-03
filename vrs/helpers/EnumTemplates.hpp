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

/// Helper template to convert a string to an enum
/// Expect the enum to have a symmetric definition (no template needed):
/// string toString(Enum enumValue);
template <class Enum>
Enum toEnum(const std::string& name);

/// Helper template to get the number of values in an enum, which assumes there is an enum value
/// named "COUNT" (capitalization matters) that gives the value.
template <class Enum>
constexpr uint32_t enumCount() {
  return static_cast<uint32_t>(Enum::COUNT);
}

} // namespace vrs
