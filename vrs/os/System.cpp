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

#include <algorithm>
#include <array>
#include <atomic>
#include <sstream>
#include <thread>

#include <vrs/os/Platform.h>

#if IS_ANDROID_PLATFORM()
#include <sys/system_properties.h>

#elif IS_APPLE_PLATFORM()
#include <sys/ioctl.h>
#include <sys/sysctl.h>
#include <unistd.h>

#elif IS_LINUX_PLATFORM()
#include <sched.h>
#include <sys/ioctl.h>
#include <sys/utsname.h>
#include <unistd.h>

#include <fstream>

#elif IS_WINDOWS_PLATFORM()
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#endif

#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <fmt/format.h>

#define DEFAULT_LOG_CHANNEL "OsSystem"
#include <logging/Log.h>

#include <vrs/os/Time.h>

using namespace std;

namespace vrs {
namespace os {

namespace {

#if IS_LINUX_PLATFORM()

// Combine two limits, where 0 means no limit.
uint32_t combineLimits(uint32_t a, uint32_t b) {
  return a == 0 ? b : (b == 0 ? a : min(a, b));
}

// Number of CPUs in the affinity mask of the calling thread, or 0 if unknown.
uint32_t getAffinityCpuCount() {
  cpu_set_t cpuSet;
  CPU_ZERO(&cpuSet);
  return sched_getaffinity(0, sizeof(cpuSet), &cpuSet) == 0 ? CPU_COUNT(&cpuSet) : 0;
}

// CPUs allowed by the cgroup v2 CPU quota of the process' cgroup and its ancestors, rounded up,
// or 0 if there is no quota.
uint32_t getCgroupCpuLimit() {
  // cgroup v2: the process' cgroup is the "0::<path>" line, and each cgroup has a "cpu.max" file:
  // "max <period>" (no limit), or "<quota> <period>", in microseconds.
  ifstream cgroups("/proc/self/cgroup");
  string path;
  for (string line; getline(cgroups, line);) {
    if (line.compare(0, 3, "0::") == 0) { // NOLINT(modernize-use-starts-ends-with): C++17 in OSS
      path = line.substr(3);
    }
  }
  if (!path.empty() && path.back() == '/') {
    path.pop_back();
  }
  // A quota can be set on any ancestor: check them all, up to the root of the mount.
  // In a container, the process' cgroup may be the root of the mount: then only the root matches.
  uint32_t limit = 0;
  while (true) {
    ifstream cpuMax("/sys/fs/cgroup" + path + "/cpu.max");
    int64_t quota = 0;
    int64_t period = 0;
    if (cpuMax >> quota >> period && quota > 0 && period > 0) {
      limit = combineLimits(limit, static_cast<uint32_t>((quota + period - 1) / period));
    }
    size_t lastSlash = path.rfind('/');
    if (lastSlash == string::npos) {
      return limit;
    }
    path.resize(lastSlash);
  }
}

#endif // IS_LINUX_PLATFORM()

} // namespace

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

uint32_t getAvailableConcurrency() {
  static const uint32_t sAvailableConcurrency = []() {
    uint32_t concurrency = thread::hardware_concurrency(); // 0 when unknown
#if IS_LINUX_PLATFORM()
    concurrency = combineLimits(concurrency, getAffinityCpuCount());
    concurrency = combineLimits(concurrency, getCgroupCpuLimit());
#endif
    return max<uint32_t>(concurrency, 1);
  }();
  return sAvailableConcurrency;
}

} // namespace os
} // namespace vrs
