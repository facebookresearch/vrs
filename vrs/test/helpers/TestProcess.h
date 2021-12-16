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

#include <string>

#include <boost/process/io.hpp>
#include <boost/process/system.hpp>

/// To test command line tools
/// Add a definition for the location of the tool in the BUCK file, as an env variable.
/// See definition of TESTEDTOOL_EXE used to test this class in vrs/test/BUCK.

namespace vrs {
namespace test {

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

} // namespace test
} // namespace vrs
