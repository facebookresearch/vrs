// Facebook Technologies, LLC Proprietary and Confidential.

#include <vrs/utils/Strings.h>
#include <cstring>

namespace vrs::utils::str {

std::string trim(const std::string& text, const char* whiteChars) {
  size_t end = text.length();
  while (end > 0 && std::strchr(whiteChars, text[end - 1]) != nullptr) {
    end--;
  }
  if (end == 0) {
    return {};
  }
  size_t start = 0;
  while (start < end && std::strchr(whiteChars, text[start]) != nullptr) {
    start++;
  }
  if (start > 0 || end < text.length()) {
    return text.substr(start, end - start);
  }
  return text;
}

bool startsWith(const std::string& text, const std::string& prefix) {
  return text.length() >= prefix.length() &&
      vrs::utils::str::strncasecmp(text.c_str(), prefix.c_str(), prefix.length()) == 0;
}

bool endsWith(const std::string& text, const std::string& suffix) {
  return text.length() >= suffix.length() &&
      vrs::utils::str::strncasecmp(
          text.c_str() + text.length() - suffix.length(), suffix.c_str(), suffix.length()) == 0;
}

} // namespace vrs::utils::str
