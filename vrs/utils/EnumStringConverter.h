// Facebook Technologies, LLC Proprietary and Confidential.

#pragma once

#include <cstring>

#include <string>

#include <vrs/utils/Strings.h>

/*
 * Helper template class to convert enums to strings & back, in trivial cases.
 * Requirements:
 *  - the enum type must be cast-able to size_t
 *  - the enum values must map to a C-Style static array of names
 *
 * Watch for irregular values, and cases when the enum & the names aren't kept in sync.
 *
 * Sample:
 *   // your enum:
 *   enum class Cars : size_t { Unknown, Renault, Peugeot, Citroen };
 *   // The corresponding names:
 *   static const char* sCarNames[] = {"Unknown", "Renault", "Peugeot", "Citroen"};
 *   // Build the converter:
 *   struct CarConverter : public
 *       EnumStringConverter<Cars, cCarNames, COUNT_OF(sCarNames), Cars::Unknown, Cars::Unknown> {};
 *
 *   To convert a car enum to a string:  CarConverter::toString(Cars::Peugeot);
 *   To convert a car name to an enum:   CarConverter::toEnum("Peugeot");
 */

template <
    class E, // an enum (or enum class that can be cast to/from size_t)
    const char* NAMES[], // static array of names
    size_t NAMES_COUNT, // size of the static array of names (use COUNT_OF)
    E DEFAULT_ENUM, // enum to use when name to enum fails
    E DEFAULT_NAME, // enum to use when enum to name fails
    bool USE_INDEX_ZERO = false> // By default, the first value is reserved for unitialized state
struct EnumStringConverter {
  static const char* toString(E value) {
    size_t index = static_cast<size_t>(value);
    if (index < NAMES_COUNT) {
      return NAMES[index];
    }
    size_t defaultIndex = static_cast<size_t>(DEFAULT_NAME);
    return defaultIndex < NAMES_COUNT ? NAMES[defaultIndex] : "<Invalid value>";
  }

  // Case sensitive string to enum conversion
  static E toEnum(const char* name) {
    for (size_t k = USE_INDEX_ZERO ? 0 : 1; k < NAMES_COUNT; k++) {
      if (strcmp(name, NAMES[k]) == 0) {
        return static_cast<E>(k);
      }
    }
    return DEFAULT_ENUM;
  }
  inline static E toEnum(const std::string& name) {
    return toEnum(name.c_str());
  }

  // Case insensitive string to enum conversion
  static E toEnumNoCase(const char* name) {
    for (size_t k = USE_INDEX_ZERO ? 0 : 1; k < NAMES_COUNT; k++) {
      if (vrs::utils::str::strcasecmp(name, NAMES[k]) == 0) {
        return static_cast<E>(k);
      }
    }
    return DEFAULT_ENUM;
  }
  inline static E toEnumNoCase(const std::string& name) {
    return toEnumNoCase(name.c_str());
  }

  static constexpr size_t cNamesCount = NAMES_COUNT;
};

// From:
// https://stackoverflow.com/questions/1500363/compile-time-sizeof-array-without-using-a-macro/1500917

#define COUNT_OF(arr)                                                \
  (0 * sizeof(reinterpret_cast<const ::Bad_arg_to_COUNT_OF*>(arr)) + \
   0 * sizeof(::Bad_arg_to_COUNT_OF::check_type((arr), &(arr))) + sizeof(arr) / sizeof((arr)[0]))

struct Bad_arg_to_COUNT_OF {
  class Is_pointer; // incomplete
  class Is_array {};
  template <typename T>
  static Is_pointer check_type(const T*, const T* const*);
  static Is_array check_type(const void*, const void*);
};
