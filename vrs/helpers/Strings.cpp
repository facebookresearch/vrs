// Copyright (c) Meta Platforms, Inc. and affiliates.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "Strings.h"

#include <cctype>
#include <climits>
#include <cmath>
#include <cstring>

#include <iomanip>
#include <sstream>

#include <fmt/format.h>

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

bool startsWith(const string& text, const string& prefix) {
  return text.length() >= prefix.length() &&
      vrs::helpers::strncasecmp(text.c_str(), prefix.c_str(), prefix.length()) == 0;
}

bool endsWith(const string& text, const string& suffix) {
  return text.length() >= suffix.length() &&
      vrs::helpers::strncasecmp(
          text.c_str() + text.length() - suffix.length(), suffix.c_str(), suffix.length()) == 0;
}

string humanReadableFileSize(int64_t bytes) {
  const int64_t unit = 1024;
  if (bytes < unit) {
    return to_string(bytes) + " B";
  }
  double exp = floor(log(bytes) / log(unit));
  char pre = string("KMGTPE").at(static_cast<size_t>(exp - 1));
  stringstream oss;
  double r = bytes / pow(unit, exp);
  if (r < 1000.) {
    oss << setprecision(3);
  } else {
    oss << setprecision(0) << fixed; // avoid scientific notation switch for 1000-1023...
  }
  oss << r << ' ' << pre << 'B';
  return oss.str();
}

string humanReadableDuration(double seconds) {
  stringstream ss;
  if (seconds < 0) {
    ss << '-';
    seconds = -seconds;
  }
  const double kYear = 31557600; // Julian astronomical year
  if (seconds < 1000000000 * kYear) {
    const double kMinute = 60;
    const double kHour = 60 * kMinute;
    const double kDay = 24 * kHour;
    const double kWeek = 7 * kDay;
    bool showNext = false;
    if (seconds > kYear) {
      int years = static_cast<int>(seconds / kYear);
      ss << years << ((years == 1) ? " year " : " years ");
      seconds -= years * kYear;
      showNext = true;
    }
    if (showNext || seconds > kWeek) {
      int weeks = static_cast<int>(seconds / kWeek);
      ss << weeks << ((weeks == 1) ? " week " : " weeks ");
      seconds -= weeks * kWeek;
      showNext = true;
    }
    if (showNext || seconds > kDay) {
      int days = static_cast<int>(seconds / kDay);
      ss << days << ((days == 1) ? " day " : " days ");
      seconds -= days * kDay;
      showNext = true;
    }
    if (showNext || seconds > kHour) {
      int hours = static_cast<int>(seconds / kHour);
      ss << hours << "h ";
      seconds -= hours * kHour;
      showNext = true;
    }
    if (showNext || seconds > kMinute) {
      int minutes = static_cast<int>(seconds / kMinute);
      ss << minutes << "m ";
      seconds -= minutes * kMinute;
      showNext = true;
    }
    if (showNext || seconds == 0 || seconds >= 1) {
      ss << humanReadableTimestamp(seconds) << "s";
    } else if (seconds >= 2e-3) {
      ss << fmt::format("{:.0f}ms", seconds * 1000);
    } else if (seconds >= 2e-6) {
      ss << fmt::format("{:.0f}us", seconds * 1000000);
    } else if (seconds >= 2e-9) {
      ss << fmt::format("{:.0f}ns", seconds * 1000000000);
    } else {
      ss << fmt::format("{:.9e}", seconds);
    }
  } else {
    ss << humanReadableTimestamp(seconds) << "s";
  }
  return ss.str();
}

string humanReadableTimestamp(double seconds, uint8_t precision) {
  const char* cPreferedFormat = "{:.3f}";
  double cTinyLimit = 1e-3;
  const char* cTinyFormat = "{:.3e}";
  const double cHugeLimit = 1e10;
  const char* cHugeFormat = "{:.9e}";
  if (precision > 3) {
    if (precision <= 6) {
      cTinyLimit = 1e-6;
      cPreferedFormat = "{:.6f}";
    } else {
      cTinyLimit = 1e-9;
      cPreferedFormat = "{:.9f}";
    }
  }
  double atimestamp = abs(seconds);
  if (atimestamp < cTinyLimit) {
    return fmt::format(fmt::runtime(atimestamp > 0 ? cTinyFormat : cPreferedFormat), seconds);
  }
  return fmt::format(
      fmt::runtime(atimestamp >= cHugeLimit ? cHugeFormat : cPreferedFormat), seconds);
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

bool getBool(const std::map<string, string>& m, const string& field, bool& outValue) {
  const auto iter = m.find(field);
  if (iter != m.end() && !iter->second.empty()) {
    outValue = iter->second != "0" && iter->second != "false";
    return true;
  }
  return false;
}

bool getInt(const std::map<string, string>& m, const string& field, int& outValue) {
  const auto iter = m.find(field);
  if (iter != m.end() && !iter->second.empty()) {
    try {
      outValue = stoi(iter->second);
      return true;
    } catch (std::logic_error&) {
      /* do nothing */
    }
  }
  return false;
}

bool getInt64(const std::map<string, string>& m, const string& field, int64_t& outValue) {
  const auto iter = m.find(field);
  if (iter != m.end() && !iter->second.empty()) {
    try {
      outValue = stoll(iter->second);
      return true;
    } catch (std::logic_error&) {
      /* do nothing */
    }
  }
  return false;
}

bool getUInt64(const std::map<string, string>& m, const string& field, uint64_t& outValue) {
  const auto iter = m.find(field);
  if (iter != m.end() && !iter->second.empty()) {
    try {
      outValue = stoull(iter->second);
      return true;
    } catch (std::logic_error&) {
      /* do nothing */
    }
  }
  return false;
}

bool getDouble(const std::map<string, string>& m, const string& field, double& outValue) {
  const auto iter = m.find(field);
  if (iter != m.end() && !iter->second.empty()) {
    try {
      outValue = stod(iter->second);
      return true;
    } catch (std::logic_error&) {
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

} // namespace helpers
} // namespace vrs
