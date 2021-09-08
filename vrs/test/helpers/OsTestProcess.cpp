// Facebook Technologies, LLC Proprietary and Confidential.

#include "OsTestProcess.h"

#include <portability/Platform.h>
#include <system_utils/os/Utils.h>

#define DEFAULT_LOG_CHANNEL "OsTestProcess"
#include <logging/Log.h>
#include <logging/Verify.h>

#include <vrs/os/Utils.h>
#include <vrs/utils/Strings.h>

// Windows has to be special
#if IS_WINDOWS_PLATFORM()
#define EXECUTABLE_SUFFIX ".exe"
#else
#define EXECUTABLE_SUFFIX ""
#endif

namespace vrs::os {

using namespace std;

inline std::string trim(const std::string& line) {
  return vrs::utils::str::trim(line, " \t\r\n");
}

bool OsTestProcess::start(string arg, bp::ipstream* sout) {
  string path = string(processName_);
  if (findBinary(path)) {
    if (looksLikeAFbCentOSServer()) {
      path = arvr::system_utils::os::getCurrentFBCodeLoader() + ' ' + path;
    }
    if (sout != nullptr) {
      process.reset(new bp::child(path + ' ' + arg, bp::std_out > *sout, bp::std_err > bp::null));
    } else {
      process.reset(new bp::child(path + ' ' + arg));
    }
    return true;
  }
  return false;
}

string OsTestProcess::getJsonOutput(bp::ipstream& output) const {
  std::string line;
  while (std::getline(output, line)) {
    line = trim(line);
    if (!line.empty() && line.front() == '{' && line.back() == '}') {
      return line;
    }
  }
  return {};
}

int OsTestProcess::runProcess() {
  if (!process) {
    return -1;
  }
  process->wait();
  return process->exit_code();
}

bool OsTestProcess::looksLikeAFbCentOSServer() {
#if IS_LINUX_PLATFORM()
  return isFile("/etc/fb-os-release");
#else
  return false;
#endif
}

bool OsTestProcess::findBinary(string& inOutName) {
  // With Buck, we expect the process's path to be injected using an environment variable
  std::string envVarName{inOutName};
  transform(envVarName.begin(), envVarName.end(), envVarName.begin(), ::toupper);
  envVarName += "_EXE"; // ex: "VRStool" -> "VRSTOOL_EXE"
  const char* exactPath = std::getenv(envVarName.c_str());
  if (exactPath != nullptr) {
    inOutName = exactPath;
  } else {
    // cmake-generator setup: look for the tool next to the unit test
    inOutName = getParentFolder(getCurrentExecutablePath()) + '/' + inOutName + EXECUTABLE_SUFFIX;
  }
  return isFile(inOutName);
}

} // namespace vrs::os
