// Facebook Technologies, LLC Proprietary and Confidential.

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
namespace vrs::helpers {

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

} // namespace vrs::helpers
