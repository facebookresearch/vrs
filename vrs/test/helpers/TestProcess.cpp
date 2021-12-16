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

#include "TestProcess.h"

#include <vrs/os/Platform.h>

#if IS_VRS_FB_INTERNAL()
#include <system_utils/os/Utils.h>
#endif

#define DEFAULT_LOG_CHANNEL "TestProcess"
#include <logging/Log.h>
#include <logging/Verify.h>

#include <vrs/helpers/Strings.h>
#include <vrs/os/Utils.h>

// Windows has to be special
#if IS_WINDOWS_PLATFORM()
#define EXECUTABLE_SUFFIX ".exe"
#else
#define EXECUTABLE_SUFFIX ""
#endif

namespace vrs {
namespace test {

using namespace std;
using namespace vrs;

inline string trim(const string& line) {
  return vrs::helpers::trim(line, " \t\r\n");
}

bool TestProcess::start(string arg, bp::ipstream* sout) {
  string path = string(processName_);
  if (findBinary(path)) {
#if IS_VRS_FB_INTERNAL()
    if (looksLikeAFbCentOSServer()) {
      path = arvr::system_utils::os::getCurrentFBCodeLoader() + ' ' + path;
    }
#endif
    if (sout != nullptr) {
      process.reset(new bp::child(path + ' ' + arg, bp::std_out > *sout, bp::std_err > bp::null));
    } else {
      process.reset(new bp::child(path + ' ' + arg));
    }
    return true;
  }
  return false;
}

string TestProcess::getJsonOutput(bp::ipstream& output) const {
  string line;
  while (getline(output, line)) {
    line = trim(line);
    if (!line.empty() && line.front() == '{' && line.back() == '}') {
      return line;
    }
  }
  return {};
}

int TestProcess::runProcess() {
  if (!process) {
    return -1;
  }
  process->wait();
  return process->exit_code();
}

bool TestProcess::looksLikeAFbCentOSServer() {
#if IS_LINUX_PLATFORM()
  return os::isFile("/etc/fb-os-release");
#else
  return false;
#endif
}

bool TestProcess::findBinary(string& inOutName) {
  // With Buck, we expect the process's path to be injected using an environment variable
  string envVarName{inOutName};
  transform(envVarName.begin(), envVarName.end(), envVarName.begin(), ::toupper);
  envVarName += "_EXE"; // ex: "VRStool" -> "VRSTOOL_EXE"
  const char* exactPath = getenv(envVarName.c_str());
  if (exactPath != nullptr) {
    inOutName = exactPath;
  } else {
    // cmake-generator setup: look for the tool next to the unit test
    string exeFolder = os::getParentFolder(os::getCurrentExecutablePath());
    inOutName = os::pathJoin(exeFolder, inOutName + EXECUTABLE_SUFFIX);
  }
  return os::isFile(inOutName);
}

} // namespace test
} // namespace vrs
