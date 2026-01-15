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

#include <cstdio>
#include <memory>
#include <sstream>

#include <gtest/gtest.h>

#include <TestDataDir/TestDataDir.h>

#include <test_helpers/GTestMacros.h>
#include <vrs/RecordFileReader.h>
#include <vrs/os/Utils.h>
#include <vrs/utils/Validation.h>

#include <vrs/VrsCommand.h>
#include <vrs/test/VrsProcess.h>

using namespace std;
using namespace vrs;
using namespace vrs::utils;
using namespace vrs::cli;

using coretech::getTestDataDir;

namespace {
string checkVRSFile(VrsCommand& command, CheckType checkType = CheckType::Checksums) {
  CopyOptions options(false);

  if (command.filteredReader.openFile() != 0) {
    return "";
  }
  command.applyFilters(command.filteredReader);
  return checkRecords(command.filteredReader, options, checkType);
}

string checkVRSFile(const string& path, CheckType checkType = CheckType::Checksums) {
  VrsCommand command;
  command.filteredReader.setSource(path);
  return checkVRSFile(command, checkType);
}

string mergeArgs(const vector<string>& args) {
  stringstream ss;
  for (const string& arg : args) {
    ss << ' ' << arg;
  }
  return ss.str();
}

bool parse(VrsCommand& command, const vector<string>& args) {
  int argn = 0;
  int outStatusCode = EXIT_SUCCESS;
  vector<char*> argvs;
  argvs.reserve(args.size() + 1);
  argvs.emplace_back(const_cast<char*>("vrs"));
  for (const string& arg : args) {
    argvs.emplace_back(const_cast<char*>(arg.c_str()));
  }
  if (!command.parseCommand(args[0], argvs[++argn])) {
    return false;
  }
  const string appName{"vrs"};
  while (++argn < static_cast<int>(argvs.size())) {
    if (!command.parseArgument(
            appName, argn, static_cast<int>(argvs.size()), argvs.data(), outStatusCode) &&
        !command.processUnrecognizedArgument(appName, argvs[argn])) {
      outStatusCode = EXIT_FAILURE;
    }
    EXPECT_EQ(outStatusCode, EXIT_SUCCESS);
    if (outStatusCode != EXIT_SUCCESS) {
      return false;
    }
  }
  return true;
}

string checkVRSFile(const vector<string>& args, CheckType checkType = CheckType::Checksums) {
  VrsCommand command;
  EXPECT_TRUE(parse(command, args));
  return checkVRSFile(command, checkType);
}

} // namespace

struct VrsAppTest : testing::Test {};

TEST_F(VrsAppTest, ANDROID_DISABLED(VrsAppTest)) {
  VrsProcess vrs;
  const string kChunkedFile = os::pathJoin(getTestDataDir(), "VRS_Files/chunks.vrs");
  const string outputFile = os::getTempFolder() + "VrsAppTest.vrs";

  // test copying the chunked file into a single file
  EXPECT_TRUE(vrs.start("copy " + kChunkedFile + " --to " + outputFile + " --no-progress"));
  ASSERT_EQ(vrs.runProcess(), 0);

  vrs::RecordFileReader reader;
  ASSERT_EQ(reader.openFile(outputFile), 0);
  EXPECT_EQ(reader.getStreams().size(), 3);
  EXPECT_EQ(reader.getIndex().size(), 306);
  EXPECT_EQ(reader.getTags().size(), 3);

  // Verify that the copied file has the same checksum as the original
  string sourceCS = checkVRSFile(kChunkedFile);
  string outputCS = checkVRSFile(outputFile);
  EXPECT_EQ(sourceCS, outputCS);
  string outputCSS = checkVRSFile(outputFile);
  EXPECT_EQ(outputCS, outputCSS);

  remove(outputFile.c_str());
}

TEST_F(VrsAppTest, ANDROID_DISABLED(limitedVrsAppTest)) {
  VrsProcess vrs;
  const string kChunkedFile = os::pathJoin(getTestDataDir(), "VRS_Files/simulated.vrs");
  const string outputFile = os::getTempFolder() + "limitedVrsAppTest.vrs";

  vector<string> args = {
      "copy",
      kChunkedFile,
      "--to",
      outputFile,
      "--after",
      "+2.2",
      "--before",
      "14",
      "+",
      "1201",
      "--no-progress"};

  // test copying the chunked file into a single file
  EXPECT_TRUE(vrs.start(mergeArgs(args)));
  ASSERT_EQ(vrs.runProcess(), 0);

  vrs::RecordFileReader reader;
  ASSERT_EQ(reader.openFile(outputFile), 0);
  EXPECT_EQ(reader.getStreams().size(), 1);
  EXPECT_EQ(reader.getIndex().size(), 179);
  EXPECT_EQ(reader.getTags().size(), 4);
  StreamId streamId{RecordableTypeId::SlamCameraData, 1};
  EXPECT_NEAR(reader.getRecord(streamId, Record::Type::DATA, 0)->timestamp, 2.266, 0.001);
  EXPECT_NEAR(reader.getIndex().back().timestamp, 14.000, 0.0005);

  // Verify that the copied file has the same checksum as the original file, filtered
  string sourceCS = checkVRSFile(args); // original file, filtered
  string outputCS = checkVRSFile(outputFile); // output file (no filters needed)
  EXPECT_EQ(sourceCS, outputCS);

  remove(outputFile.c_str());
}

static void checkTag(const map<string, string>& tags, const string& name, const string& value) {
  auto iter = tags.find(name);
  if (iter != tags.end()) {
    EXPECT_STREQ(iter->second.c_str(), value.c_str());
  } else {
    ASSERT_NO_FATAL_FAILURE() << name;
  }
}

TEST_F(VrsAppTest, ANDROID_DISABLED(copyWithTags)) {
  VrsProcess vrs;
  const string inputFile = os::pathJoin(getTestDataDir(), "VRS_Files/simulated.vrs");
  const string outputFile = os::getTempFolder() + "copyWithTags.vrs";

  // test copying a piece of file (to be faster), and add tags.
  EXPECT_TRUE(vrs.start(
      "copy " + inputFile + " --to " + outputFile + " --before +0.1 --file-tag myTag myValue " +
      "--file-tag device_type quest --stream-tag 1201-1 position left --no-progress"));
  ASSERT_EQ(vrs.runProcess(), 0);

  vrs::RecordFileReader reader;
  ASSERT_EQ(reader.openFile(outputFile), 0);
  EXPECT_EQ(reader.getStreams().size(), 3);
  EXPECT_EQ(reader.getIndex().size(), 109);
  auto& tags = reader.getTags();
  EXPECT_EQ(tags.size(), 6);
  checkTag(tags, "myTag", "myValue"); // added a tag
  checkTag(tags, "device_type", "quest"); // overwrote an existing tag
  // use our new stream tag, to find the stream using that new tag name-value pair!
  vrs::StreamId id =
      reader.getStreamForTag("position", "left", vrs::RecordableTypeId::SlamCameraData);
  EXPECT_EQ(id, vrs::StreamId(vrs::RecordableTypeId::SlamCameraData, 1));

  remove(outputFile.c_str());
}

TEST_F(VrsAppTest, ANDROID_DISABLED(merge2FilesTest)) {
  VrsProcess vrs;
  const string kFirstFile = os::pathJoin(getTestDataDir(), "VRS_Files/simulated.vrs");
  const string kSecondFile = os::pathJoin(getTestDataDir(), "VRS_Files/simulated2.vrs");
  const string outputFile = os::getTempFolder() + "merge2FilesTest.vrs";

  // test merging two files into a single file, keeping streams separate
  EXPECT_TRUE(vrs.start(
      "copy " + kFirstFile + " " + kSecondFile + " --to " + outputFile + " --no-progress"));
  ASSERT_EQ(vrs.runProcess(), 0);

  vrs::RecordFileReader reader;
  ASSERT_EQ(reader.openFile(outputFile), 0);
  EXPECT_EQ(reader.getStreams().size(), 4); // streams are kept separate; 3 + 1 = 4
  EXPECT_EQ(reader.getIndex().size(), 15605);
  EXPECT_EQ(reader.getTags().size(), 4);
  EXPECT_NEAR(reader.getIndex().front().timestamp, 0, 0.0001);
  EXPECT_NEAR(reader.getIndex().back().timestamp, 15.071, 0.0001);
  StreamId streamId{RecordableTypeId::SlamImuData, 1};
  EXPECT_NEAR(reader.getRecord(streamId, Record::Type::DATA, 0)->timestamp, 0.001, 0.00001);
  StreamId cam1id{RecordableTypeId::SlamCameraData, 1};
  EXPECT_NEAR(reader.getRecord(cam1id, Record::Type::DATA, 0)->timestamp, 0, 0.00001);
  EXPECT_NEAR(reader.getLastRecord(cam1id, Record::Type::DATA)->timestamp, 15.000, 0.00001);
  StreamId cam2id{RecordableTypeId::SlamCameraData, 2};
  EXPECT_NEAR(reader.getRecord(cam2id, Record::Type::DATA, 0)->timestamp, 0.001, 0.00001);
  EXPECT_NEAR(reader.getLastRecord(cam2id, Record::Type::DATA)->timestamp, 15.001, 0.00001);

  remove(outputFile.c_str());
}

TEST_F(VrsAppTest, ANDROID_DISABLED(fuseTest)) {
  VrsProcess vrs;
  const string kFirstFile = os::pathJoin(getTestDataDir(), "VRS_Files/simulated.vrs");
  const string kSecondFile = os::pathJoin(getTestDataDir(), "VRS_Files/simulated2.vrs");
  const string outputFile = os::getTempFolder() + "fuseTest.vrs";

  // test merging two files into a single file, merging streams with the same RecordableTypeId
  EXPECT_TRUE(vrs.start(
      "merge " + kFirstFile + " " + kSecondFile + " --to " + outputFile + " --no-progress"));
  ASSERT_EQ(vrs.runProcess(), 0);

  vrs::RecordFileReader reader;
  ASSERT_EQ(reader.openFile(outputFile), 0);
  EXPECT_EQ(reader.getStreams().size(), 3); // streams are fused; 3 + 1 = 3
  EXPECT_EQ(reader.getIndex().size(), 15605);
  EXPECT_EQ(reader.getTags().size(), 4);
  EXPECT_NEAR(reader.getIndex().front().timestamp, 0, 0.0001);
  EXPECT_NEAR(reader.getIndex().back().timestamp, 15.071, 0.0001);
  StreamId imuId{RecordableTypeId::SlamImuData, 1};
  EXPECT_NEAR(reader.getRecord(imuId, Record::Type::DATA, 0)->timestamp, 0.001, 0.00001);
  EXPECT_NEAR(reader.getLastRecord(imuId, Record::Type::DATA)->timestamp, 15.071, 0.00001);
  StreamId cam1id{RecordableTypeId::SlamCameraData, 1};
  EXPECT_NEAR(reader.getRecord(cam1id, Record::Type::DATA, 0)->timestamp, 0, 0.00001);
  EXPECT_NEAR(reader.getLastRecord(cam1id, Record::Type::DATA)->timestamp, 15.001, 0.00001);

  remove(outputFile.c_str());
}

TEST_F(VrsAppTest, ANDROID_DISABLED(mergeRecordablesFilterTest)) {
  VrsProcess vrs;
  const string kFirstFile = os::pathJoin(getTestDataDir(), "VRS_Files/simulated.vrs");
  const string kSecondFile = os::pathJoin(getTestDataDir(), "VRS_Files/simulated2.vrs");
  const string outputFile = os::getTempFolder() + "mergeRecordablesFilterTest.vrs";

  // test merging two files into a single file, filtering specific recordables
  EXPECT_TRUE(vrs.start(
      "copy " + kFirstFile + " " + kSecondFile + " --to " + outputFile +
      " + 1201-1 + 1202 --no-progress"));
  ASSERT_EQ(vrs.runProcess(), 0);

  vrs::RecordFileReader reader;
  ASSERT_EQ(reader.openFile(outputFile), 0);
  EXPECT_EQ(reader.getStreams().size(), 3);
  EXPECT_EQ(reader.getIndex().size(), 15073 + 228 + 228);
  EXPECT_EQ(reader.getTags().size(), 4);

  remove(outputFile.c_str());
}

TEST_F(VrsAppTest, ANDROID_DISABLED(mergeTimeFilterTest)) {
  VrsProcess vrs;
  const string kFirstFile = os::pathJoin(getTestDataDir(), "VRS_Files/simulated.vrs");
  const string kSecondFile = os::pathJoin(getTestDataDir(), "VRS_Files/simulated2.vrs");
  const string outputFile = os::getTempFolder() + "mergeTimeFilterTest.vrs";

  // test merging two files into a single file, filtering specific recordables
  EXPECT_TRUE(vrs.start(
      "copy " + kFirstFile + " " + kSecondFile + " --to " + outputFile +
      " --range +2.001 -3 --no-progress"));
  ASSERT_EQ(vrs.runProcess(), 0);

  vrs::RecordFileReader reader;
  ASSERT_EQ(reader.openFile(outputFile), 0);
  EXPECT_EQ(reader.getStreams().size(), 4);
  EXPECT_EQ(reader.getIndex().size(), 10429);
  EXPECT_EQ(reader.getTags().size(), 4);
  // 1 config & 1 state record per stream are NOT constrained by the range limitation (preroll)
  EXPECT_NEAR(reader.getIndex().front().timestamp, 0, 0.0001);
  // Data records are constrained by the +0.2 range
  EXPECT_NEAR(reader.getFirstDataRecordTime(), 2.002, 0.0001);
  EXPECT_NEAR(reader.getIndex().back().timestamp, 12.071, 0.0001);
  EXPECT_NEAR(reader.getLastDataRecordTime(), 12.071, 0.0001);

  remove(outputFile.c_str());
}

TEST_F(VrsAppTest, ANDROID_DISABLED(chunkAndMergeTest)) {
  VrsProcess vrs;
  const string kOriginal = os::pathJoin(getTestDataDir(), "VRS_Files/simulated.vrs");
  const string kPart1 = os::getTempFolder() + "chunk1.vrs";
  const string kPart2 = os::getTempFolder() + "chunk2.vrs";
  const string kMerged = os::getTempFolder() + "chunkAndMergeTest.vrs";
  ASSERT_TRUE(vrs.start("copy " + kOriginal + " --to " + kPart1 + " --before 10 --no-progress"));
  ASSERT_EQ(vrs.runProcess(), 0);
  ASSERT_TRUE(vrs.start("copy " + kOriginal + " --to " + kPart2 + " --after 10 --no-progress"));
  ASSERT_EQ(vrs.runProcess(), 0);
  ASSERT_TRUE(vrs.start("merge " + kPart1 + " " + kPart2 + " --to " + kMerged + " --no-progress"));
  ASSERT_EQ(vrs.runProcess(), 0);
  EXPECT_EQ(recordsChecksum(kOriginal, false), recordsChecksum(kMerged, false));
  remove(kPart1.c_str());
  remove(kPart2.c_str());
  remove(kMerged.c_str());
}

TEST_F(VrsAppTest, ANDROID_DISABLED(decimateTest)) {
  VrsProcess vrs;
  const string inputFile = os::pathJoin(getTestDataDir(), "VRS_Files/simulated.vrs");
  const string outputFile = os::getTempFolder() + "decimateTest.vrs";

  EXPECT_TRUE(vrs.start(
      "copy " + inputFile + " --to " + outputFile +
      " --range +1 +2 --decimate 1202 0.010 --no-progress"));
  ASSERT_EQ(vrs.runProcess(), 0);

  vrs::RecordFileReader reader;
  ASSERT_EQ(reader.openFile(outputFile), 0);
  EXPECT_EQ(reader.getIndex().size(), 126);
  EXPECT_EQ(reader.getStreams().size(), 3);
  StreamId imuId{RecordableTypeId::SlamImuData, 1};
  EXPECT_NEAR(reader.getRecord(imuId, Record::Type::DATA, 0)->timestamp, 1.001, 0.00001);
  EXPECT_EQ(reader.getRecordCount(imuId, vrs::Record::Type::CONFIGURATION), 1);
  EXPECT_EQ(reader.getRecordCount(imuId, vrs::Record::Type::STATE), 1);
  EXPECT_EQ(reader.getRecordCount(imuId, vrs::Record::Type::DATA), 100);
  StreamId cam1id{RecordableTypeId::SlamCameraData, 1};
  EXPECT_NEAR(reader.getRecord(cam1id, Record::Type::DATA, 0)->timestamp, 1.067, 0.0005);
  EXPECT_NEAR(reader.getLastRecord(cam1id, Record::Type::DATA)->timestamp, 2.000, 0.00001);
  EXPECT_EQ(reader.getRecordCount(cam1id, vrs::Record::Type::CONFIGURATION), 1);
  EXPECT_EQ(reader.getRecordCount(cam1id, vrs::Record::Type::STATE), 1);
  EXPECT_EQ(reader.getRecordCount(cam1id, vrs::Record::Type::DATA), 15);

  remove(outputFile.c_str());
}

TEST_F(VrsAppTest, ANDROID_DISABLED(relativeRangeTest)) {
  VrsProcess vrs;
  const string inputFile = os::pathJoin(getTestDataDir(), "VRS_Files/simulated.vrs");
  const string outputFile = os::getTempFolder() + "relativeRangeTest.vrs";

  // Copy a sub-range of data records, while keeping the config & state records,
  // which are outside of the data record timestamp range.
  // Stream 214-1: 3 data records, from 5.100 to 5.500
  // Stream 1201-1: 7 data records, from 5.067 to 5.467
  EXPECT_TRUE(vrs.start(
      "copy " + inputFile + " --to " + outputFile +
      " + 214-1 + 1201-1 --range +5 -9.5 --no-progress"));
  ASSERT_EQ(vrs.runProcess(), 0);

  vrs::RecordFileReader reader;
  ASSERT_EQ(reader.openFile(outputFile), 0);
  EXPECT_EQ(reader.getIndex().size(), 14);
  EXPECT_EQ(reader.getStreams().size(), 2);
  StreamId rgbCam{RecordableTypeId::RgbCameraRecordableClass, 1};
  EXPECT_EQ(reader.getRecordCount(rgbCam, vrs::Record::Type::CONFIGURATION), 1);
  EXPECT_EQ(reader.getRecordCount(rgbCam, vrs::Record::Type::STATE), 1);
  EXPECT_EQ(reader.getRecordCount(rgbCam, vrs::Record::Type::DATA), 3);
  EXPECT_NEAR(reader.getRecord(rgbCam, vrs::Record::Type::DATA, 0)->timestamp, 5.100, 0.001);
  EXPECT_NEAR(reader.getRecord(rgbCam, vrs::Record::Type::DATA, 2)->timestamp, 5.500, 0.001);

  StreamId slamCam{RecordableTypeId::SlamCameraData, 1};
  EXPECT_EQ(reader.getRecordCount(slamCam, vrs::Record::Type::CONFIGURATION), 1);
  EXPECT_EQ(reader.getRecordCount(slamCam, vrs::Record::Type::STATE), 1);
  EXPECT_EQ(reader.getRecordCount(slamCam, vrs::Record::Type::DATA), 7);
  EXPECT_NEAR(reader.getRecord(slamCam, vrs::Record::Type::DATA, 0)->timestamp, 5.067, 0.001);
  EXPECT_NEAR(reader.getRecord(slamCam, vrs::Record::Type::DATA, 6)->timestamp, 5.467, 0.001);

  remove(outputFile.c_str());
}

TEST_F(VrsAppTest, ANDROID_DISABLED(syntaxErrorTest)) {
  VrsProcess vrs;
  const string kFirstFile = os::pathJoin(getTestDataDir(), "VRS_Files/simulated.vrs");
  EXPECT_TRUE(vrs.start(kFirstFile + " -not-a-command"));
  ASSERT_NE(vrs.runProcess(), 0);
}

TEST_F(VrsAppTest, ANDROID_DISABLED(noErrorTest)) {
  VrsProcess vrs;
  const string kFirstFile = os::pathJoin(getTestDataDir(), "VRS_Files/not-a-file.vrs");
  EXPECT_TRUE(vrs.start(kFirstFile));
  ASSERT_NE(vrs.runProcess(), 0);
}

TEST_F(VrsAppTest, ANDROID_DISABLED(badFileErrorTest)) {
  VrsProcess vrs;
  const string kFirstFile = os::pathJoin(getTestDataDir(), "VRS_Files/simulated.vrs");
  const string outputFile = os::getTempFolder() + "bad-file-!*:/";
  EXPECT_TRUE(vrs.start(kFirstFile + " -c " + outputFile));
  ASSERT_NE(vrs.runProcess(), 0);
}
