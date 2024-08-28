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

#include "Strings.h"

#include <climits>
#include <cmath>
#include <cstring>
#include <ctime>

#include <algorithm>
#include <sstream>

#include <fmt/format.h>

#include <vrs/os/Platform.h>

namespace vrs {
namespace helpers {

using namespace std;

string trim(const string& text, const char* whiteChars) {
  size_t end = text.length();
  while (end > 0 && strchr(whiteChars, text[end - 1]) != nullptr) {
    end--;
  }
  if (end == 0) {
    return {};
  }
  size_t start = 0;
  while (start < end && strchr(whiteChars, text[start]) != nullptr) {
    start++;
  }
  if (start > 0 || end < text.length()) {
    return text.substr(start, end - start);
  }
  return text;
}

bool startsWith(const string_view& text, const string_view& prefix) {
  return text.length() >= prefix.length() &&
      vrs::helpers::strncasecmp(text.data(), prefix.data(), prefix.length()) == 0;
}

bool endsWith(const string_view& text, const string_view& suffix) {
  return text.length() >= suffix.length() &&
      vrs::helpers::strncasecmp(
          text.data() + text.length() - suffix.length(), suffix.data(), suffix.length()) == 0;
}

inline bool isdigit(char c) {
  return std::isdigit(static_cast<uint8_t>(c));
}

static uint32_t lastDigitIndex(const char* str, uint32_t index) {
  while (isdigit(str[index + 1])) {
    index++;
  }
  return index;
}

inline char paddedChar(const char* str, uint32_t pos, uint32_t pad, uint32_t index) {
  return index < pad ? '0' : str[pos + index - pad];
}

#define LEFT_C (left[left_p])
#define RIGHT_C (right[right_p])

bool beforeFileName(const char* left, const char* right) {
  uint32_t leftPos = 0;
  uint32_t rightPos = 0;
  bool bothDigits = false;
  while ((bothDigits = (isdigit(left[leftPos]) && isdigit(right[rightPos]))) ||
         (left[leftPos] == right[rightPos] && left[leftPos] != 0)) {
    if (bothDigits) {
      uint32_t leftDigitLength = lastDigitIndex(left, leftPos) - leftPos;
      uint32_t rightDigitLength = lastDigitIndex(right, rightPos) - rightPos;
      uint32_t leftPad =
          leftDigitLength < rightDigitLength ? rightDigitLength - leftDigitLength : 0;
      uint32_t rightPad =
          rightDigitLength < leftDigitLength ? leftDigitLength - rightDigitLength : 0;
      uint32_t lastDigitIndex = max<uint32_t>(leftDigitLength, rightDigitLength);
      for (uint32_t digitIndex = 0; digitIndex <= lastDigitIndex; digitIndex++) {
        char lc = paddedChar(left, leftPos, leftPad, digitIndex);
        char rc = paddedChar(right, rightPos, rightPad, digitIndex);
        if (lc != rc) {
          return lc < rc;
        }
      }
      leftPos += leftDigitLength;
      rightPos += rightDigitLength;
    }
    leftPos++, rightPos++;
  }
  if (left[leftPos] == 0) {
    return right[rightPos] != 0;
  }
  return left[leftPos] < right[rightPos];
}

string humanReadableFileSize(int64_t bytes) {
  const char* sign = "";
  if (bytes < 0) {
    sign = "-";
    bytes = -bytes;
  }
  uint64_t ubytes = static_cast<uint64_t>(bytes);
  const uint64_t kB = 1 << 10; // aka 1024
  if (ubytes < kB) {
    return fmt::format("{}{} B", sign, ubytes);
  }
  const char* unitFactor = "KMGTPE";
  const uint64_t unitFactorsLimit = strlen(unitFactor) - 1;
  uint64_t factor = kB;
  uint64_t e = 0;
  while (e < unitFactorsLimit && ubytes >= (factor << 10)) {
    e++;
    factor <<= 10;
  }
  char pre = unitFactor[e];
  uint64_t intPart = ubytes >> ((e + 1) * 10);
  if (intPart >= 100) {
    // avoid scientific notation switch for 100-1023...
    return fmt::format("{}{} {}iB", sign, intPart, pre);
  }
  double rest = double((ubytes % factor) >> e * 10) / kB;
  if (intPart >= 10) {
    double r = intPart + floor(rest * 16) / 16; // prevent rounding up when there are too many 9s
    return fmt::format("{}{:.1f} {}iB", sign, r, pre);
  }
  double r = intPart + floor(rest * 160) / 160; // prevent rounding up when there are too many 9s
  return fmt::format("{}{:.2f} {}iB", sign, r, pre);
}

string humanReadableDuration(double seconds) {
  string str;
  str.reserve(30);
  if (seconds < 0) {
    str.push_back('-');
    seconds = -seconds;
  }
  const double kYear = 31557600; // Julian astronomical year
  if (seconds < 1e9 * kYear) {
    const double kMinute = 60;
    const double kHour = 60 * kMinute;
    const double kDay = 24 * kHour;
    const double kWeek = 7 * kDay;
    bool showNext = false;
    if (seconds > kYear) {
      int years = static_cast<int>(seconds / kYear);
      str.append(to_string(years)).append((years == 1) ? " year, " : " years, ");
      seconds -= years * kYear;
      showNext = true;
    }
    if (showNext || seconds > kWeek) {
      int weeks = static_cast<int>(seconds / kWeek);
      str.append(to_string(weeks)).append((weeks == 1) ? " week, " : " weeks, ");
      seconds -= weeks * kWeek;
      showNext = true;
    }
    if (showNext || seconds > kDay) {
      int days = static_cast<int>(seconds / kDay);
      str.append(to_string(days)).append((days == 1) ? " day, " : " days, ");
      seconds -= days * kDay;
      showNext = true;
    }
    if (showNext || seconds > kHour) {
      int hours = static_cast<int>(seconds / kHour);
      str.append(to_string(hours)).append("h ");
      seconds -= hours * kHour;
      showNext = true;
    }
    if (showNext || seconds > kMinute) {
      int minutes = static_cast<int>(seconds / kMinute);
      str.append(to_string(minutes)).append("m ");
      seconds -= minutes * kMinute;
      showNext = true;
    }
    if (showNext || seconds == 0 || seconds >= 1) {
      str.append(humanReadableTimestamp(seconds)).append("s");
    } else if (seconds >= 2e-3) {
      str.append(fmt::format("{:.0f}ms", seconds * 1e03));
    } else if (seconds >= 2e-6) {
      str.append(fmt::format("{:.0f}us", seconds * 1e06));
    } else if (seconds >= 2e-9) {
      str.append(fmt::format("{:.0f}ns", seconds * 1e09));
    } else if (seconds >= 2e-12) {
      str.append(fmt::format("{:.0f}ps", seconds * 1e12));
    } else if (seconds >= 2e-15) {
      str.append(fmt::format("{:.0f}fs", seconds * 1e15));
    } else if (seconds >= 2e-18) {
      str.append(fmt::format("{:.3f}fs", seconds * 1e15));
    } else {
      str.append(fmt::format("{:g}fs", seconds * 1e15));
    }
  } else {
    str.append(humanReadableTimestamp(seconds)).append("s");
  }
  return str;
}

string humanReadableTimestamp(double seconds, uint8_t precision) {
  // This code must work with Ubuntu's 20.4 fmt and the newest compilers/fmt.
  // Bottom line: fmt::format must use a constant format string to compile everywhere.
  enum Format { f3, f6, f9, e3, e9 };
  Format format = f3;
  double cTinyLimit = 1e-3;
  const double cHugeLimit = 1e10;
  if (precision > 3) {
    if (precision <= 6) {
      cTinyLimit = 1e-6;
      format = f6;
    } else {
      cTinyLimit = 1e-9;
      format = f9;
    }
  }
  double atimestamp = abs(seconds);
  if (atimestamp < cTinyLimit) {
    if (atimestamp > 0) {
      format = e3;
    }
  } else if (atimestamp >= cHugeLimit) {
    format = e9;
  }
  switch (format) {
    case f3:
      return fmt::format("{:.3f}", seconds);
    case f6:
      return fmt::format("{:.6f}", seconds);
    case f9:
      return fmt::format("{:.9f}", seconds);
    case e3:
      return fmt::format("{:.3e}", seconds);
    case e9:
      return fmt::format("{:.9e}", seconds);
  }

  // Mute bogus warnings needed by some compilers.
  return fmt::format("{:.3f}", seconds);
}

string humanReadableDateTime(double secondsSinceEpoch) {
  string date(std::size("YYYY-MM-DD HH:MM:SS"), 0);
  time_t creationTimeSec = static_cast<time_t>(secondsSinceEpoch);
  struct tm ltm {};
#if IS_WINDOWS_PLATFORM()
  localtime_s(&ltm, &creationTimeSec); // because "Windows"
#else
  localtime_r(&creationTimeSec, &ltm);
#endif
  date.resize(std::strftime(date.data(), date.size(), "%F %T", &ltm));
  return date;
}

string make_printable(const string& str) {
  string sanitized;
  if (!str.empty()) {
    sanitized.reserve(str.size() + 10);
    for (unsigned char c : str) {
      if (c < 128 && isprint(c)) {
        sanitized.push_back(c);
      } else if (c == '\n') {
        sanitized += "\\n"; // LF
      } else if (c == '\r') {
        sanitized += "\\r"; // CR
      } else if (c == '\t') {
        sanitized += "\\t"; // tab
      } else if (c == '\b') {
        sanitized += "\\b"; // backspace
      } else if (c == 0x1B) {
        sanitized += "\\e"; // esc
      } else {
        // stringstream methods just won't work right, even with Stackoverflow's help...
        static const char* digits = "0123456789abcdef";
        sanitized += "\\x";
        sanitized.push_back(digits[(c >> 4) & 0xf]);
        sanitized.push_back(digits[c & 0xf]);
      }
    }
  }
  return sanitized;
}

bool getBool(const map<string, string>& m, const string& field, bool& outValue) {
  const auto iter = m.find(field);
  if (iter != m.end() && !iter->second.empty()) {
    outValue = iter->second != "0" && iter->second != "false";
    return true;
  }
  return false;
}

bool getInt(const map<string, string>& m, const string& field, int& outValue) {
  const auto iter = m.find(field);
  if (iter != m.end() && !iter->second.empty()) {
    try {
      outValue = stoi(iter->second);
      return true;
    } catch (logic_error&) {
      /* do nothing */
    }
  }
  return false;
}

bool getInt64(const map<string, string>& m, const string& field, int64_t& outValue) {
  const auto iter = m.find(field);
  if (iter != m.end() && !iter->second.empty()) {
    try {
      outValue = stoll(iter->second);
      return true;
    } catch (logic_error&) {
      /* do nothing */
    }
  }
  return false;
}

bool getUInt64(const map<string, string>& m, const string& field, uint64_t& outValue) {
  const auto iter = m.find(field);
  return iter != m.end() && readUInt64(iter->second, outValue);
}

bool getByteSize(
    const std::map<std::string, std::string>& m,
    const std::string& field,
    uint64_t& outByteSize) {
  outByteSize = 0;
  const auto iter = m.find(field);
  return iter != m.end() && readByteSize(iter->second, outByteSize);
}

bool getDouble(const map<string, string>& m, const string& field, double& outValue) {
  const auto iter = m.find(field);
  if (iter != m.end() && !iter->second.empty()) {
    try {
      outValue = stod(iter->second);
      return true;
    } catch (logic_error&) {
      /* do nothing */
    }
  }
  return false;
}

bool readUInt32(const char*& str, uint32_t& outValue) {
  char* newStr = nullptr;
  errno = 0;
  long long int readInt = strtoll(str, &newStr, 10);
  if (readInt < 0 || (readInt == LLONG_MAX && errno == ERANGE) ||
      readInt > numeric_limits<uint32_t>::max() || str == newStr) {
    return false;
  }

  outValue = static_cast<uint32_t>(readInt);

  str = newStr;
  return true;
}

inline char safetolower(char c) {
  return tolower(static_cast<unsigned char>(c));
}

inline char safeisdigit(char c) {
  return isdigit(static_cast<unsigned char>(c));
}

// Interpret strings with units such as "5KB" or "23mb"
bool readByteSize(const string& strSize, uint64_t& outByteSize) {
  if (strSize.empty()) {
    outByteSize = 0;
    return false;
  }
  char* next = nullptr;
  outByteSize = std::strtoull(strSize.c_str(), &next, 10);
  if (*next == 0) {
    return true;
  }
  uint64_t factor = 1;
  switch (safetolower(*next)) {
    case 'e':
      factor <<= 50;
      break;
    case 't':
      factor <<= 40;
      break;
    case 'g':
      factor <<= 30;
      break;
    case 'm':
      factor <<= 20;
      break;
    case 'k':
      factor <<= 10;
      break;
    case 'b':
      if (next[1] == 0) {
        return true;
      }
      break;
    default:
      break;
  }
  if (factor == 1 || safetolower(next[1]) != 'b' || next[2] != 0) {
    return false;
  }
  outByteSize *= factor;
  return true;
}

bool replaceAll(string& inOutString, const string& token, const string& replacement) {
  bool replaced = false;
  if (!token.empty()) {
    size_t pos = inOutString.find(token, 0);
    while (pos != string::npos) {
      inOutString.replace(pos, token.length(), replacement);
      replaced = true;
      pos = inOutString.find(token, pos + replacement.length());
    }
  }
  return replaced;
}

size_t split(
    const std::string& inputString,
    char delimiter,
    std::vector<std::string>& tokens,
    bool skipEmpty,
    const char* trimChars) {
  std::stringstream ss(inputString);
  std::string item;

  while (getline(ss, item, delimiter)) {
    if (trimChars != nullptr) {
      item = helpers::trim(item, trimChars);
    }
    if (!(item.empty() && skipEmpty)) {
      tokens.push_back(item);
    }
  }
  return tokens.size();
}

bool readUInt64(const std::string& str, uint64_t& outValue) {
  if (!str.empty() && safeisdigit(str.front())) {
    char* next = nullptr;
    outValue = std::strtoull(str.c_str(), &next, 10);
    if (*next == 0 && (outValue != ULLONG_MAX || errno == 0)) {
      return true;
    }
  }
  outValue = 0;
  return false;
}

} // namespace helpers
} // namespace vrs
