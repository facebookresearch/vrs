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

#pragma once

#include <vrs/os/Platform.h>

#if IS_WINDOWS_PLATFORM()
#ifndef BOOST_USE_WINDOWS_H
#define BOOST_USE_WINDOWS_H
#endif
#include <windows.h>
#endif

#include <string>

#include <vrs/os/Process.h>

/// To test command line tools
/// Add a definition for the location of the tool in the BUCK file, as an env variable.
/// See definition of TESTEDTOOL_EXE used to test this class in vrs/test/BUCK.

namespace vrs {
namespace test {

namespace bp = vrs::os::process;

class TestProcess {
 public:
  explicit TestProcess(const char* processName) : processName_{processName} {}
  bool start(const std::string& arg, bp::ipstream& sout) {
    return start(arg, &sout);
  }
  bool start(const std::string& arg, bp::ipstream* sout = nullptr);

  static std::string getJsonOutput(bp::ipstream& output);

  int runProcess();

 private:
  std::unique_ptr<bp::child> process;
  const char* processName_;

  static bool looksLikeAFbCentOSServer();

  static bool findBinary(std::string& inOutName);
};

} // namespace test
} // namespace vrs
