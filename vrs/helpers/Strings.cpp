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
      ss << years << "y ";
      seconds -= years * kYear;
      showNext = true;
    }
    if (showNext || seconds > kWeek) {
      int weeks = static_cast<int>(seconds / kWeek);
      ss << weeks << "w ";
      seconds -= weeks * kWeek;
      showNext = true;
    }
    if (showNext || seconds > kDay) {
      int days = static_cast<int>(seconds / kDay);
      ss << days << "d ";
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
    }
    ss << fixed << setprecision(3) << seconds << "s";
  } else {
    ss << scientific << setprecision(3) << seconds << "s";
  }
  return ss.str();
}

string humanReadableTimestamp(double timestamp) {
  double atimestamp = abs(timestamp);
  if (atimestamp < 1e-3 || atimestamp > 1e10) {
    return fmt::format("{:.3e}", timestamp);
  } else {
    return fmt::format("{:.3f}", timestamp);
  }
}

string make_printable(const string& str) {
  string sanitized;
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
  return sanitized;
}

} // namespace helpers
} // namespace vrs
