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

#include "System.h"

#include <array>
#include <sstream>

#include <vrs/os/Platform.h>

#if IS_ANDROID_PLATFORM()
#include <sys/system_properties.h>

#elif IS_MAC_PLATFORM()
#include <sys/sysctl.h>
#include <cerrno>

#elif IS_LINUX_PLATFORM()
#include <sys/utsname.h>

#elif IS_WINDOWS_PLATFORM()
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#endif

#if IS_VRS_OSS_CODE()
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>
#endif

#define DEFAULT_LOG_CHANNEL "OsSystem"
#include <logging/Log.h>

using namespace std;

string vrs::os::getOsFingerPrint() {
#if IS_ANDROID_PLATFORM()
  array<char, PROP_VALUE_MAX> osFingerprint;
  int osFingerprintLength = __system_property_get("ro.build.fingerprint", osFingerprint.data());
  string osFingerprintString;
  if (osFingerprintLength > 0) {
    osFingerprintString.assign(osFingerprint.data(), static_cast<size_t>(osFingerprintLength));
  }
  return osFingerprintString;

#elif IS_MAC_PLATFORM()
  array<char, 256> osFingerprint;
  size_t size = osFingerprint.size();
  string osFingerprintString = "MacOS ";
  if (sysctlbyname("kern.osrelease", osFingerprint.data(), &size, nullptr, 0) == 0) {
    osFingerprintString.append(osFingerprint.data());
  } else {
    osFingerprintString = "<Unknown>";
  }
  return osFingerprintString;

#elif IS_LINUX_PLATFORM()
  string osFingerprintString;
  struct utsname linuxNames;
  if (uname(&linuxNames) == 0) {
    osFingerprintString = linuxNames.sysname;
    osFingerprintString += " ";
    osFingerprintString += linuxNames.release;
    osFingerprintString += ", ";
    osFingerprintString += linuxNames.machine;
    osFingerprintString += ", ";
    osFingerprintString += linuxNames.version;
  } else {
    osFingerprintString = "Linux version: <Unknown>";
  }
  return osFingerprintString;

#elif IS_WINDOWS_PLATFORM()
  DWORD dwVersion = 0;
  DWORD dwMajorVersion = 0;
  DWORD dwMinorVersion = 0;
  DWORD dwBuild = 0;
  dwVersion = GetVersion();

  // Get the Windows version.
  dwMajorVersion = (DWORD)(LOBYTE(LOWORD(dwVersion)));
  dwMinorVersion = (DWORD)(HIBYTE(LOWORD(dwVersion)));

  // Get the build number.
  if (dwVersion < 0x80000000) {
    dwBuild = (DWORD)(HIWORD(dwVersion));
  }
  string osFingerprintString;
  osFingerprintString = "Windows ";
  osFingerprintString += to_string(dwMajorVersion);
  osFingerprintString += ".";
  osFingerprintString += to_string(dwMinorVersion);
  osFingerprintString += ", build #";
  osFingerprintString += to_string(dwBuild);
  return osFingerprintString;

#else
  XR_LOGW("OS fingerprint not implemented for this OS.");
  return "<unknown>";
#endif
}

#if IS_VRS_OSS_CODE()
string vrs::os::getUniqueSessionId() {
  stringstream sstream;
  boost::uuids::random_generator generator;
  sstream << generator();
  return sstream.str();
}
#endif
