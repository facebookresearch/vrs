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

#include <cstring>

#include <string>
#include <string_view>

#include <vrs/helpers/EnumTemplates.hpp>
#include <vrs/helpers/Strings.h>

namespace vrs {

/*
 * Helper template class to convert enums to strings & back, in trivial cases.
 * Requirements:
 *  - the enum type must be cast-able to size_t
 *  - the enum values must map to a string_view static array of names
 *
 * Watch for irregular values, and cases when the enum & the names aren't kept in sync.
 *
 * Sample:
 *   // your enum:
 *   enum class Cars : size_t { Unknown, Renault, Peugeot, Citroen };
 *   // The corresponding names:
 *   static std::string_view sCarNames[] = {"Unknown", "Renault", "Peugeot", "Citroen"};
 *   // Build the converter:
 *   struct CarConverter : public
 *       EnumStringConverter<Cars, cCarNames, array_size(sCarNames), Cars::Unknown, Cars::Unknown>
 * {};
 *
 *   To convert a car enum to a string:  CarConverter::toString(Cars::Peugeot);
 *   To convert a car name to an enum:   CarConverter::toEnum("Peugeot");
 */

template <
    class E, // an enum (or enum class that can be cast to/from size_t)
    const std::string_view NAMES[], // static array of names
    size_t NAMES_COUNT, // size of the static array of names (use array_size)
    E DEFAULT_ENUM, // enum to use when name to enum fails
    E DEFAULT_NAME = DEFAULT_ENUM, // enum to use when enum to name fails
    bool USE_INDEX_ZERO = false> // By default, the first value is reserved for uninitialized state
struct EnumStringConverter {
  static constexpr size_t cNamesCount = NAMES_COUNT;

  static std::string_view toStringView(E value);
  inline static std::string toString(E value) {
    return std::string(toStringView(value));
  }
  inline static const char* toCString(E value) {
    // valid, because NAMES is a static array initialized with const char* values
    return toStringView(value).data();
  }

  // Case sensitive string to enum conversion
  static E toEnum(std::string_view name);
  inline static E toEnum(const std::string& name) {
    return toEnum(std::string_view(name));
  }
  inline static E toEnum(const char* name) {
    return toEnum(std::string_view(name));
  }

  // Case insensitive string to enum conversion
  static E toEnumNoCase(std::string_view name);
  inline static E toEnumNoCase(const std::string& name) {
    return toEnumNoCase(std::string_view(name));
  }
  inline static E toEnumNoCase(const char* name) {
    return toEnumNoCase(std::string_view(name));
  }
};

// Implementation moved outside class to reduce template instantiation overhead
template <
    class E,
    const std::string_view NAMES[],
    size_t NAMES_COUNT,
    E DEFAULT_ENUM,
    E DEFAULT_NAME,
    bool USE_INDEX_ZERO>
std::string_view
EnumStringConverter<E, NAMES, NAMES_COUNT, DEFAULT_ENUM, DEFAULT_NAME, USE_INDEX_ZERO>::
    toStringView(E value) {
  const size_t index = static_cast<size_t>(value);
  if (index < cNamesCount) {
    return NAMES[index];
  }
  const size_t defaultIndex = static_cast<size_t>(DEFAULT_NAME);
  return defaultIndex < cNamesCount ? NAMES[defaultIndex] : "<Invalid value>";
}

template <
    class E,
    const std::string_view NAMES[],
    size_t NAMES_COUNT,
    E DEFAULT_ENUM,
    E DEFAULT_NAME,
    bool USE_INDEX_ZERO>
E EnumStringConverter<E, NAMES, NAMES_COUNT, DEFAULT_ENUM, DEFAULT_NAME, USE_INDEX_ZERO>::toEnum(
    std::string_view name) {
  for (size_t k = USE_INDEX_ZERO ? 0 : 1; k < cNamesCount; k++) {
    if (name == NAMES[k]) {
      return static_cast<E>(k);
    }
  }
  return DEFAULT_ENUM;
}

template <
    class E,
    const std::string_view NAMES[],
    size_t NAMES_COUNT,
    E DEFAULT_ENUM,
    E DEFAULT_NAME,
    bool USE_INDEX_ZERO>
E EnumStringConverter<E, NAMES, NAMES_COUNT, DEFAULT_ENUM, DEFAULT_NAME, USE_INDEX_ZERO>::
    toEnumNoCase(std::string_view name) {
  for (size_t k = USE_INDEX_ZERO ? 0 : 1; k < cNamesCount; k++) {
    if (name.size() == NAMES[k].size() &&
        vrs::helpers::strncasecmp(name.data(), NAMES[k].data(), name.size()) == 0) {
      return static_cast<E>(k);
    }
  }
  return DEFAULT_ENUM;
}

template <typename T, size_t N>
inline constexpr size_t array_size(const T (&iarray)[N]) {
  return N;
}

} // namespace vrs
