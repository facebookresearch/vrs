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

#include <string>
#include <vector>

#include <gtest/gtest.h>

#include <TestDataDir/TestDataDir.h>
#include <vrs/os/Utils.h>
#include <vrs/utils/RecordFileInfo.h>

#include <vrs/VrsCommand.h>

using namespace std;
using namespace vrs;
using namespace vrs::utils;
using namespace vrscli;

using coretech::getTestDataDir;

struct VrsCommandTest : testing::Test {};

bool parse(VrsCommand& command, vector<string>& args, int& argn, int& outStatusCode) {
  argn = 0;
  outStatusCode = EXIT_SUCCESS;
  vector<char*> argvs(args.size());
  for (size_t index = 0; index < args.size(); index++) {
    argvs[index] = const_cast<char*>(args[index].c_str());
  }
  if (!command.parseCommand(args[0], argvs[++argn])) {
    return EXIT_FAILURE;
  }
  const string appName{args[0]};
  while (++argn < static_cast<int>(argvs.size())) {
    if (!command.parseArgument(
            appName, argn, static_cast<int>(argvs.size()), argvs.data(), outStatusCode) &&
        !command.processUnrecognizedArgument(appName, argvs[argn])) {
      outStatusCode = EXIT_FAILURE;
    }
    if (outStatusCode != EXIT_SUCCESS) {
      return false;
    }
  }
  return true;
}

TEST_F(VrsCommandTest, miscCommands) {
  int argn = 1;
  int statusCode = EXIT_SUCCESS;
  const string inputFile = os::pathJoin(getTestDataDir(), "VRS_Files/simulated.vrs");
  const string inputFile2 = os::pathJoin(getTestDataDir(), "VRS_Files/sample_file.vrs");
  const string outputFile = os::pathJoin(getTestDataDir(), "VRS_Files/some_output.vrs");

  {
    vector<string> args = {"vrs", "check", inputFile};
    VrsCommand command;
    EXPECT_TRUE(parse(command, args, argn, statusCode));
    EXPECT_EQ(argn, 3);
    EXPECT_EQ(statusCode, EXIT_SUCCESS);
    EXPECT_EQ(command.cmd, Command::Check);
    EXPECT_EQ(command.filteredReader.spec.chunks.size(), 3);
    EXPECT_EQ(command.filteredReader.getFileSize(), 21337114);
  }

  {
    vector<string> args = {"vrs", "checksum", inputFile};
    VrsCommand command;
    EXPECT_TRUE(parse(command, args, argn, statusCode));
    EXPECT_EQ(argn, 3);
    EXPECT_EQ(statusCode, EXIT_SUCCESS);
    EXPECT_EQ(command.cmd, Command::Checksum);
    EXPECT_EQ(command.filteredReader.spec.chunks.size(), 3);
    EXPECT_EQ(command.filteredReader.getFileSize(), 21337114);
  }

  {
    vector<string> args = {"vrs", "compare", inputFile};
    VrsCommand command;
    EXPECT_TRUE(parse(command, args, argn, statusCode));
    EXPECT_EQ(argn, 3);
    EXPECT_EQ(statusCode, EXIT_SUCCESS);
    EXPECT_EQ(command.cmd, Command::Compare);
    EXPECT_EQ(command.filteredReader.spec.chunks.size(), 3);
    EXPECT_EQ(command.filteredReader.getFileSize(), 21337114);
  }

  {
    vector<string> args = {"vrs", "compare-verbatim", inputFile};
    VrsCommand command;
    EXPECT_TRUE(parse(command, args, argn, statusCode));
    EXPECT_EQ(argn, 3);
    EXPECT_EQ(statusCode, EXIT_SUCCESS);
    EXPECT_EQ(command.cmd, Command::CompareVerbatim);
    EXPECT_EQ(command.filteredReader.spec.chunks.size(), 3);
    EXPECT_EQ(command.filteredReader.getFileSize(), 21337114);
  }

  {
    vector<string> args = {"vrs", "copy", inputFile, "--to", outputFile};
    VrsCommand command;
    EXPECT_TRUE(parse(command, args, argn, statusCode));
    EXPECT_EQ(argn, 5);
    EXPECT_EQ(statusCode, EXIT_SUCCESS);
    EXPECT_EQ(command.cmd, Command::Copy);
    EXPECT_EQ(command.filteredReader.spec.chunks.size(), 3);
    EXPECT_EQ(command.filteredReader.getFileSize(), 21337114);
    EXPECT_EQ(command.targetPath, outputFile);
  }

  {
    vector<string> args = {"vrs", "copy", inputFile, "--to", outputFile, inputFile2};
    VrsCommand command;
    EXPECT_TRUE(parse(command, args, argn, statusCode));
    EXPECT_EQ(argn, 6);
    EXPECT_EQ(statusCode, EXIT_SUCCESS);
    EXPECT_EQ(command.cmd, Command::Copy);
    EXPECT_EQ(command.filteredReader.spec.chunks.size(), 3);
    EXPECT_EQ(command.filteredReader.getFileSize(), 21337114);
    EXPECT_EQ(command.otherFilteredReaders.back().getPathOrUri(), inputFile2);
    EXPECT_EQ(command.otherFilteredReaders.size(), 1);
    EXPECT_EQ(command.targetPath, outputFile);
  }

  {
    vector<string> args = {"vrs", "copy", inputFile, inputFile2, "--to", outputFile};
    VrsCommand command;
    EXPECT_TRUE(parse(command, args, argn, statusCode));
    EXPECT_EQ(argn, 6);
    EXPECT_EQ(statusCode, EXIT_SUCCESS);
    EXPECT_EQ(command.cmd, Command::Copy);
    EXPECT_EQ(command.filteredReader.spec.chunks.size(), 3);
    EXPECT_EQ(command.filteredReader.getFileSize(), 21337114);
    EXPECT_EQ(command.otherFilteredReaders.back().getPathOrUri(), inputFile2);
    EXPECT_EQ(command.otherFilteredReaders.size(), 1);
    EXPECT_EQ(command.targetPath, outputFile);
  }

  {
    vector<string> args = {"vrs", "merge", inputFile, inputFile2, "--to", outputFile};
    VrsCommand command;
    EXPECT_TRUE(parse(command, args, argn, statusCode));
    EXPECT_EQ(argn, 6);
    EXPECT_EQ(statusCode, EXIT_SUCCESS);
    EXPECT_EQ(command.cmd, Command::Merge);
    EXPECT_EQ(command.filteredReader.spec.chunks.size(), 3);
    EXPECT_EQ(command.filteredReader.getFileSize(), 21337114);
    EXPECT_EQ(command.otherFilteredReaders.back().getPathOrUri(), inputFile2);
    EXPECT_EQ(command.otherFilteredReaders.size(), 1);
    EXPECT_EQ(command.targetPath, outputFile);
  }

  {
    vector<string> args = {"vrs", "debug", inputFile};
    VrsCommand command;
    EXPECT_TRUE(parse(command, args, argn, statusCode));
    EXPECT_EQ(argn, 3);
    EXPECT_EQ(statusCode, EXIT_SUCCESS);
    EXPECT_EQ(command.cmd, Command::Debug);
    EXPECT_EQ(command.filteredReader.spec.chunks.size(), 3);
    EXPECT_EQ(command.filteredReader.getFileSize(), 21337114);
  }

  {
    vector<string> args = {"vrs", "record-formats", inputFile};
    VrsCommand command;
    EXPECT_TRUE(parse(command, args, argn, statusCode));
    EXPECT_EQ(argn, 3);
    EXPECT_EQ(statusCode, EXIT_SUCCESS);
    EXPECT_EQ(command.cmd, Command::PrintRecordFormats);
    EXPECT_EQ(command.filteredReader.spec.chunks.size(), 3);
    EXPECT_EQ(command.filteredReader.getFileSize(), 21337114);
  }

  {
    vector<string> args = {"vrs", "list", inputFile};
    VrsCommand command;
    EXPECT_TRUE(parse(command, args, argn, statusCode));
    EXPECT_EQ(argn, 3);
    EXPECT_EQ(statusCode, EXIT_SUCCESS);
    EXPECT_EQ(command.cmd, Command::ListRecords);
    EXPECT_EQ(command.filteredReader.spec.chunks.size(), 3);
    EXPECT_EQ(command.filteredReader.getFileSize(), 21337114);
  }
}

TEST_F(VrsCommandTest, ArgTestsGood) {
  const string inputFile = os::pathJoin(getTestDataDir(), "VRS_Files/simulated.vrs");

  int argn = 1;
  int statusCode = EXIT_SUCCESS;

  {
    VrsCommand command;
    vector<string> args = {"vrs", "list", "--before", "123", "--after", "+1", inputFile};
    EXPECT_TRUE(parse(command, args, argn, statusCode));
    EXPECT_EQ(command.filteredReader.filter.maxTime, 123);
    EXPECT_EQ(command.filteredReader.filter.relativeMaxTime, false);
    EXPECT_EQ(command.filteredReader.filter.minTime, 1);
    EXPECT_EQ(command.filteredReader.filter.relativeMinTime, true);
  }

  {
    VrsCommand command;
    vector<string> args = {"vrs", "list", "--before", "-1", "--after", "123", inputFile};
    EXPECT_TRUE(parse(command, args, argn, statusCode));
    EXPECT_EQ(command.filteredReader.filter.maxTime, -1);
    EXPECT_EQ(command.filteredReader.filter.relativeMaxTime, true);
    EXPECT_EQ(command.filteredReader.filter.minTime, 123);
    EXPECT_EQ(command.filteredReader.filter.relativeMinTime, false);
  }

  {
    VrsCommand command;
    vector<string> args = {"vrs", "list", "--range", "+1", "-2", inputFile};
    EXPECT_TRUE(parse(command, args, argn, statusCode));
    EXPECT_EQ(command.filteredReader.filter.maxTime, -2);
    EXPECT_EQ(command.filteredReader.filter.relativeMaxTime, true);
    EXPECT_EQ(command.filteredReader.filter.minTime, 1);
    EXPECT_EQ(command.filteredReader.filter.relativeMinTime, true);
  }

  {
    VrsCommand command;
    vector<string> args = {"vrs", "list", "-", "1203", inputFile};
    EXPECT_TRUE(parse(command, args, argn, statusCode));
    vector<string> recordableFilters = {"-", "1203"};
    EXPECT_EQ(command.filters.streamFilters, recordableFilters);
  }

  {
    VrsCommand command;
    vector<string> args = {"vrs", "list", "-", "1203-5", "+", "1101-3", inputFile};
    EXPECT_TRUE(parse(command, args, argn, statusCode));
    vector<string> recordableFilters = {"-", "1203-5", "+", "1101-3"};
    EXPECT_EQ(command.filters.streamFilters, recordableFilters);
  }
}

TEST_F(VrsCommandTest, ArgTestsBad) {
  const string inputFile = os::pathJoin(getTestDataDir(), "VRS_Files/sample_file.vrs");

  int argn = 1;
  int statusCode = EXIT_SUCCESS;
  vector<string> args;

  {
    VrsCommand command;
    args = {"vrs", "check", "checksum", inputFile};
    EXPECT_FALSE(parse(command, args, argn, statusCode));
    EXPECT_NE(statusCode, 0);
  }

  {
    VrsCommand command;
    args = {"vrs", "check", "compare", inputFile};
    EXPECT_FALSE(parse(command, args, argn, statusCode));
    EXPECT_NE(statusCode, 0);
  }

  {
    VrsCommand command;
    args = {"vrs", "list", "--before", "a", "--after", "+1", inputFile};
    EXPECT_FALSE(parse(command, args, argn, statusCode));
    EXPECT_NE(statusCode, 0);
  }

  {
    VrsCommand command;
    args = {"vrs", "list", "--before", "+1", "--after", "c", inputFile};
    EXPECT_FALSE(parse(command, args, argn, statusCode));
    EXPECT_NE(statusCode, 0);
  }

  {
    VrsCommand command;
    args = {"vrs", "list", "--range", "no", "-2", inputFile};
    EXPECT_FALSE(parse(command, args, argn, statusCode));
    EXPECT_NE(statusCode, 0);
  }

  {
    VrsCommand command;
    args = {"vrs", "list", "-", "no", inputFile};
    EXPECT_FALSE(parse(command, args, argn, statusCode));
    EXPECT_NE(statusCode, 0);
  }

  {
    VrsCommand command;
    args = {"vrs", "list", "+", "100000", inputFile};
    EXPECT_FALSE(parse(command, args, argn, statusCode));
    EXPECT_NE(statusCode, 0);
  }
}
