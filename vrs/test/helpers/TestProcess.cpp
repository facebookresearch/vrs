// Facebook Technologies, LLC Proprietary and Confidential.

#include "TestProcess.h"

#include <system_utils/os/Utils.h>

#define DEFAULT_LOG_CHANNEL "TestProcess"
#include <logging/Log.h>
#include <logging/Verify.h>

#include <vrs/helpers/Strings.h>
#include <vrs/os/Platform.h>
#include <vrs/os/Utils.h>

// Windows has to be special
#if IS_WINDOWS_PLATFORM()
#define EXECUTABLE_SUFFIX ".exe"
#else
#define EXECUTABLE_SUFFIX ""
#endif

namespace vrs::test {

using namespace std;
using namespace vrs;

inline string trim(const string& line) {
  return vrs::helpers::trim(line, " \t\r\n");
}

bool TestProcess::start(string arg, bp::ipstream* sout) {
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

} // namespace vrs::test
