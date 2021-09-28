// Copyright (c) Facebook, Inc. and its affiliates. Confidential and proprietary.

#include <string>

#include <boost/process/io.hpp>
#include <boost/process/system.hpp>

/// To test command line tools
/// Add a definition for the location of the tool in the BUCK file, as an env variable.
/// See definition of TESTEDTOOL_EXE used to test this class in vrs/test/BUCK.

namespace vrs::test {

namespace bp = boost::process;

class TestProcess {
 public:
  TestProcess(const char* processName) : processName_{processName} {}
  bool start(std::string arg, bp::ipstream& sout) {
    return start(arg, &sout);
  }
  bool start(std::string arg, bp::ipstream* sout = nullptr);

  std::string getJsonOutput(bp::ipstream& output) const;

  int runProcess();

 private:
  std::unique_ptr<bp::child> process;
  const char* processName_;

  bool looksLikeAFbCentOSServer();

  bool findBinary(std::string& inOutName);
};

} // namespace vrs::test
