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

/// Helper template to get the first "good" value of an enum.
/// ASSUMES THAT THE FIRST "GOOD" VALUE IS THE ENUM THAT CONVERTS TO 1
/// This is accurate with VRS enums, because 0 is an uninitialized/undefined state.
template <class Enum>
constexpr Enum enumFirst() {
  return static_cast<Enum>(1);
}

/// Helper template to the last value of an enum, which assumes there is an enum value
/// named "COUNT" (capitalization matters) after all the values.
template <class Enum>
constexpr Enum enumLast() {
  return static_cast<Enum>(static_cast<int>(Enum::COUNT) - 1);
}

/// For any enum, returns the next enum value, assuming the next value is valid.
template <class Enum>
constexpr Enum enumNext(Enum value) {
  return static_cast<Enum>(static_cast<int>(value) + 1);
}

/// Helper define to loop over a range of enum values.
/// Assumes every enum between FIRST_ENUM and LAST_ENUM maps to a valid enum value we want.
#define FOR_EACH_ENUM_RANGE(ENUM, VARIABLE, FIRST_ENUM, LAST_ENUM) \
  for (ENUM(VARIABLE) = (FIRST_ENUM); (VARIABLE) <= (LAST_ENUM);   \
       (VARIABLE) = vrs::enumNext<ENUM>(VARIABLE))

/// Helper define to loop over all enum values.
/// Assumptions:
/// - The first enum value has the numberic value 1.
/// - The enum has a value named "COUNT" (capitalization matters) that counts the enum values.
/// - Every value between 1 and COUNT-1 maps to a valid enum value we want to use.
/// These assummptions are usually correct with VRS enums, because 0 is typically an
/// uninitialized/undefined state.
#define FOR_EACH_ENUM(ENUM, VARIABLE)                                                \
  for (ENUM(VARIABLE) = vrs::enumFirst<ENUM>(); (VARIABLE) <= vrs::enumLast<ENUM>(); \
       (VARIABLE) = vrs::enumNext<ENUM>(VARIABLE))

/// Helper to check if an enum value is valid, that is, between the first and last "good" values.
template <class Enum>
inline bool enumIsValid(Enum ENUM_VALUE) {
  return (ENUM_VALUE) >= vrs::enumFirst<Enum>() && (ENUM_VALUE) <= vrs::enumLast<Enum>();
}

template <class Enum, typename EnumValue>
inline bool enumIsValid_cast(EnumValue ENUM_VALUE) {
  return vrs::enumIsValid<Enum>(static_cast<Enum>(ENUM_VALUE));
}

} // namespace vrs
