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
#include <string>
#include <string_view>
#include <vector>

#include <vrs/os/Platform.h>

#if !IS_WINDOWS_PLATFORM()
#include <strings.h>
#endif

/*
 * Compatibility helpers along with non-fully standardized string utilities.
 */
namespace vrs {
namespace helpers {

// strcasecmp & strncasecmp are named _stricmp & _strnicmp on Windows...
#if IS_WINDOWS_PLATFORM()
inline int strcasecmp(const char* first, const char* second) {
  return _stricmp(first, second);
}
inline int strncasecmp(const char* first, const char* second, size_t size) {
  return _strnicmp(first, second, size);
}
#else
inline int strcasecmp(const char* first, const char* second) {
  return ::strcasecmp(first, second);
}

inline int strncasecmp(const char* first, const char* second, size_t size) {
  return ::strncasecmp(first, second, size);
}
#endif

/// Compare strings, as you'd expect in a modern desktop OS (Explorer/Finder), treating digit
/// sections as numbers, so that "image1.png" is before "image02.png", and "image010.png" is the
/// same as "image00010.png".
/// Note: This is not a total order, since beforeFileName("image1.png", "image01.png") and
/// beforeFileName("image01.png", "image1.png") are both false!
bool beforeFileName(const char* left, const char* right);

inline bool beforefileName(const std::string& left, const std::string& right) {
  return beforeFileName(left.c_str(), right.c_str());
}

/// Returns a copy of the string from which all the characters in whiteChars
/// at the beginning or at the end of the string have been removed.
/// @param text: some utf8 text string to trim
/// @param whiteChars: a series of 1-byte chars to remove
/// @return the trimmed string
std::string trim(const std::string& text, const char* whiteChars = " \t");

/// Tell if a text string starts with the provided prefix.
/// @param text: the text to test
/// @param prefix: the prefix to test
/// @return True if text starts with prefix. Case insensitive.
bool startsWith(const std::string_view& text, const std::string_view& prefix);

/// Tell if a text string ends with the provided suffix.
/// @param text: the text to test
/// @param prefix: the suffix to test
/// @return True if text ends with suffix. Case insensitive.
bool endsWith(const std::string_view& text, const std::string_view& suffix);

/// Replace all occurrences of a string within another string
/// @param inOutString: the string to modify with the replacement(s)
/// @param token: text to replace
/// @param replacement: text to replace token with
/// The function can't fail. Only full instances of token in the original string will be replaced.
/// @return true if at least once instance of token was found and replaced.
bool replaceAll(std::string& inOutString, const std::string& token, const std::string& replacement);

/// Helper to get a field of a string map interpreted as a bool.
/// @param m: the map to search.
/// @param field: the name of the field.
/// @param outValue: on exit, set to the value retrieved.
/// @return True if the field was found and outValue was set.
bool getBool(const std::map<std::string, std::string>& m, const std::string& field, bool& outValue);

/// Helper to get a field of a string map interpreted as an int.
/// @param m: the map to search.
/// @param field: the name of the field.
/// @param outValue: on exit, set to the value retrieved.
/// @return True if the field was found and outValue was set.
bool getInt(const std::map<std::string, std::string>& m, const std::string& field, int& outValue);

/// Helper to get a field of a string map interpreted as an int64_t.
/// @param m: the map to search.
/// @param field: the name of the field.
/// @param outValue: on exit, set to the value retrieved.
/// @return True if the field was found and outValue was set.
bool getInt64(
    const std::map<std::string, std::string>& m,
    const std::string& field,
    int64_t& outValue);

/// Helper to get a field of a string map interpreted as an uint64_t.
/// @param m: the map to search.
/// @param field: the name of the field.
/// @param outValue: on exit, set to the value retrieved.
/// @return True if the field was found and outValue was set.
bool getUInt64(
    const std::map<std::string, std::string>& m,
    const std::string& field,
    uint64_t& outValue);

/// Helper to get a field of a string map interpreted as an uint64_t, with potential unit,
/// such as KB, MB, GB, TB, EB...
/// @param m: the map to search.
/// @param field: the name of the field.
/// @param outByteSize: on exit, set to the value retrieved, or 0.
/// @return True if the field was found and outByteSize was set.
bool getByteSize(
    const std::map<std::string, std::string>& m,
    const std::string& field,
    uint64_t& outByteSize);

/// Helper to get a field of a string map interpreted as a double.
/// @param m: the map to search.
/// @param field: the name of the field.
/// @param outValue: on exit, set to the value retrieved.
/// @return True if the field was found and outValue was set.
bool getDouble(
    const std::map<std::string, std::string>& m,
    const std::string& field,
    double& outValue);

// reads an uint32_t value, moves the string pointer & returns true on success
bool readUInt32(const char*& str, uint32_t& outValue);

/// Helper method to parse a string containing an uint64 value strictly.
/// @param str: the string that needs to be parsed.
/// @param outValue: the parsed value.
/// @return True if the string was parsed successfully and the string was a number only.
bool readUInt64(const std::string& str, uint64_t& outValue);

// Reads a number of bytes with optional KB, MB, GB, TB, EB suffixes
// Returns true on success, false on failure, setting outByteSize to 0.
// Note: ignores unrecognized suffixes.
bool readByteSize(const std::string& strSize, uint64_t& outByteSize);

/// Helper method to print a file size in a human readable way,
/// using B, KB, MB, GB, TB...
std::string humanReadableFileSize(int64_t bytes);
template <class T>
std::string humanReadableFileSize(T bytes) {
  return humanReadableFileSize(static_cast<int64_t>(bytes));
}

/// Helper method to print a count of seconds in a human readable way,
/// using a count of years, weeks, days, hours, minutes, seconds, as appropriate.
std::string humanReadableDuration(double seconds);

/// Helper method to print a count of seconds with a certain precision, when that makes sense.
/// @param seconds: the count of seconds.
/// @param precision: how many fraction digits to print. Optimized for 3, 6 and 9 only.
/// @return The count of seconds printed optimized for human consumption, using a given number of
/// digits past 0, so we always show up to ms, us or ns, but uses the scientific notation for very
/// small or very large numbers.
std::string humanReadableTimestamp(double seconds, uint8_t precision = 3);

/// Helper to print the date and time from an EPOCH timestamp (seconds since EPOCH).
/// @param secondsSinceEpoch: timestamp in seconds since EPOCH.
/// @return a human readable date and time.
std::string humanReadableDateTime(double secondsSinceEpoch);

/// Helper method to make a string printable to expose control characters.
/// This conversion is meant to make string problems visible, rather than be a proper encoding,
/// for instance, you can't differentiate between "\n" and string that would contain a newline char.
std::string make_printable(const std::string& str);

/// Helper method to split a string based on the delimiter.
/// @param inputString: the string that needs to be split.
/// @param delimiter: the delimiter that will be used to split the input string.
/// @param outTokens: the collection of strings generated by the split.
/// @param skipEmpty: if true, won't return empty tokens
/// @param trimChars: if specified, characters to trim for each token
/// @return The number of tokens extracted.
/// If outValues contained values on entry, these are cleared.
size_t split(
    const std::string& inputString,
    char delimiter,
    std::vector<std::string>& outTokens,
    bool skipEmpty = false,
    const char* trimChars = nullptr);

} // namespace helpers
} // namespace vrs
