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

#include <vrs/os/System.h>

#include <array>
#include <atomic>
#include <sstream>

#include <vrs/os/Platform.h>

#if IS_ANDROID_PLATFORM()
#include <sys/system_properties.h>

#elif IS_APPLE_PLATFORM()
#include <sys/ioctl.h>
#include <sys/sysctl.h>
#include <unistd.h>

#elif IS_LINUX_PLATFORM()
#include <sys/ioctl.h>
#include <sys/utsname.h>
#include <unistd.h>

#elif IS_WINDOWS_PLATFORM()
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#endif

#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>

#define DEFAULT_LOG_CHANNEL "OsSystem"
#include <logging/Log.h>

#include <vrs/os/Time.h>

using namespace std;

namespace vrs {
namespace os {

string getOsFingerPrint() {
#if IS_ANDROID_PLATFORM()
  array<char, PROP_VALUE_MAX> osFingerprint;
  int osFingerprintLength = __system_property_get("ro.build.fingerprint", osFingerprint.data());
  string osFingerprintString;
  if (osFingerprintLength > 0) {
    osFingerprintString.assign(osFingerprint.data(), static_cast<size_t>(osFingerprintLength));
  }
  return osFingerprintString;

#elif IS_APPLE_PLATFORM()
  array<char, 256> osFingerprint{};
  size_t size = osFingerprint.size();
  string osFingerprintString = IS_MAC_PLATFORM() ? "MacOS " : "iOS ";
  if (sysctlbyname("kern.osrelease", osFingerprint.data(), &size, nullptr, 0) == 0) {
    osFingerprintString.append(osFingerprint.data());
  } else {
    osFingerprintString = "<Unknown>";
  }
  return osFingerprintString;

#elif IS_LINUX_PLATFORM()
  string osFingerprintString;
  struct utsname linuxNames{};
  if (uname(&linuxNames) == 0) {
    osFingerprintString = fmt::format(
        "{} {}, {}, {}",
        linuxNames.sysname,
        linuxNames.release,
        linuxNames.machine,
        linuxNames.version);
  } else {
    osFingerprintString = "Linux version: <Unknown>";
  }
  return osFingerprintString;

#elif IS_WINDOWS_PLATFORM()
  DWORD dwVersion = GetVersion();
  DWORD dwMajorVersion = (DWORD)(LOBYTE(LOWORD(dwVersion)));
  DWORD dwMinorVersion = (DWORD)(HIBYTE(LOWORD(dwVersion)));
  DWORD dwBuild = (dwVersion < 0x80000000) ? (DWORD)(HIWORD(dwVersion)) : 0;
  return fmt::format("Windows {}.{}, build #{}", dwMajorVersion, dwMinorVersion, dwBuild);

#else
  XR_LOGW("OS fingerprint not implemented for this OS.");
  return "<unknown>";
#endif
}

string getUniqueSessionId() {
  stringstream sstream;
  boost::uuids::random_generator generator;
  sstream << generator();
  return sstream.str();
}

uint32_t getTerminalWidth(uint32_t setValue) {
  static atomic<uint32_t> sTerminalWidth = 0;
  static atomic<double> sLastWidthCheckTime = 0;

  const double kWidthCheckInterval = 5.0; // seconds
  const uint32_t kDefaultWidth = 160;
  double now = getTimestampSec();
  uint32_t width = sTerminalWidth.load(memory_order_relaxed);
  if (setValue != 0) {
    width = setValue;
    sTerminalWidth = width;
    sLastWidthCheckTime = now + 10000; // make the value stick for a long while
  } else if (
      width == 0 || (sLastWidthCheckTime.load(memory_order_relaxed) + kWidthCheckInterval) < now) {
#if IS_WINDOWS_PLATFORM()
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    width =
        (GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi) ? csbi.dwSize.X
                                                                            : kDefaultWidth);
#elif IS_LINUX_PLATFORM() || IS_MAC_PLATFORM()
    struct winsize w{};
    width = ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) == 0 ? w.ws_col : kDefaultWidth;

#else
    width = kDefaultWidth;

#endif
    if (width < 40 || width > 1000) {
      width = kDefaultWidth; // when sanity check fails, use default
    }
    sTerminalWidth = width;
    sLastWidthCheckTime = now;
  }
  return width;
}

} // namespace os
} // namespace vrs
