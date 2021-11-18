// Copyright (c) Facebook, Inc. and its affiliates. Confidential and proprietary.

#pragma once

#include <algorithm>
#include <string>

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

/// Returns a copy of the string from which all the characters in whiteChars
/// at the beginning or at the end of the string have been removed.
/// @param text: some utf8 text string to trim
/// @param whiteChars: a series of 1-byte chars to remove
/// @return the trimed string
std::string trim(const std::string& text, const char* whiteChars = " \t");

/// Tell if a text string starts with the provided prefix.
/// @param text: the text to test
/// @param prefix: the prefix to test
/// @return True if text starts with prefix. Case insensitive.
bool startsWith(const std::string& text, const std::string& prefix);

/// Tell if a text string ends with the provided suffix.
/// @param text: the text to test
/// @param prefix: the suffix to test
/// @return True if text ends with suffix. Case insensitive.
bool endsWith(const std::string& text, const std::string& suffix);

/// Helper method to print a file size in a human readable way,
/// using B, KB, MB, GB, TB...
std::string humanReadableFileSize(int64_t bytes);
template <class T>
std::string humanReadableFileSize(T bytes) {
  return humanReadableFileSize(static_cast<int64_t>(bytes));
}

/// Helper method to print a count of seconds in a human readable way,
/// using a count of years, weeks, days, hours, minutes, seconds...
std::string humanReadableDuration(double seconds);
std::string humanReadableTimestamp(double timestamp);

/// Helper method to make a string printable to expose control characters.
/// This conversion is meant to make string problems visible, rather than be a proper encoding,
/// for instance, you can't differentiate between "\n" and string that would contain a newline char.
std::string make_printable(const std::string& str);

} // namespace helpers
} // namespace vrs
