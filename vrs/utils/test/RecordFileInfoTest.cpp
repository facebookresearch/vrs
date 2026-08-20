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

#include <vrs/utils/RecordFileInfo.h>

#include <ctime>
#include <iomanip>
#include <sstream>
#include <string>

#include <gtest/gtest.h>

#include <vrs/RecordFileReader.h>
#include <vrs/RecordFileWriter.h>
#include <vrs/TagConventions.h>
#include <vrs/os/Platform.h>
#include <vrs/os/Utils.h>

using namespace std;
using namespace vrs;

namespace {

void overviewWithFileTag(
    const string& fileName,
    const string& tagName,
    const string& tagValue,
    string& outOverview) {
  const string path = os::pathJoin(os::getTempFolder(), fileName);
  RecordFileWriter writer;
  writer.setTag(tagName, tagValue);
  ASSERT_EQ(writer.writeToFile(path), 0);
  RecordFileReader reader;
  ASSERT_EQ(reader.openFile(path), 0);
  ostringstream out;
  // Without CompleteTags the output is truncated to the terminal width, which varies by host.
  RecordFileInfo::printOverview(
      out, reader, RecordFileInfo::Details::ListFileTags + RecordFileInfo::Details::CompleteTags);
  reader.closeFile();
  os::remove(path);
  outOverview = out.str();
}

} // namespace

TEST(RecordFileInfoTest, MalformedCaptureTimeEpochTagIsPrintedVerbatim) {
  string overview;
  ASSERT_NO_FATAL_FAILURE(overviewWithFileTag(
      "RecordFileInfoTest-malformed-epoch.vrs",
      tag_conventions::kCaptureTimeEpoch,
      "not-a-number",
      overview));
  EXPECT_NE(overview.find("capture_time_epoch = not-a-number"), string::npos);
}

TEST(RecordFileInfoTest, OutOfRangeCaptureTimeEpochTagIsPrintedVerbatim) {
  string overview;
  ASSERT_NO_FATAL_FAILURE(overviewWithFileTag(
      "RecordFileInfoTest-huge-epoch.vrs",
      tag_conventions::kCaptureTimeEpoch,
      "999999999999999999999999999999",
      overview));
  EXPECT_NE(overview.find("capture_time_epoch = 999999999999999999999999999999"), string::npos);
}

TEST(RecordFileInfoTest, CaptureTimeEpochBeyondTimeTIsPrintedWithoutCrashing) {
  // Parses as a uint64 but overflows time_t. Whether libc still formats a date from the
  // wrapped value is implementation-defined, so only the tag itself is asserted.
  string overview;
  ASSERT_NO_FATAL_FAILURE(overviewWithFileTag(
      "RecordFileInfoTest-unrepresentable-epoch.vrs",
      tag_conventions::kCaptureTimeEpoch,
      "9999999999999999999",
      overview));
  EXPECT_NE(overview.find("capture_time_epoch = 9999999999999999999"), string::npos);
}

TEST(RecordFileInfoTest, WellFormedCaptureTimeEpochTagIsDecodedToADate) {
  string overview;
  ASSERT_NO_FATAL_FAILURE(overviewWithFileTag(
      "RecordFileInfoTest-valid-epoch.vrs",
      tag_conventions::kCaptureTimeEpoch,
      "1520364293",
      overview));
  // The rendering of "%c %Z" depends on the host's timezone and locale, so build the expected
  // suffix the same way rather than hard-coding one host's answer.
  time_t epochSec = 1520364293;
  struct tm creationTime{};
#if IS_WINDOWS_PLATFORM()
  ASSERT_EQ(localtime_s(&creationTime, &epochSec), 0);
#else
  ASSERT_NE(localtime_r(&epochSec, &creationTime), nullptr);
#endif
  ostringstream expected;
  expected << "capture_time_epoch = 1520364293" << put_time(&creationTime, " -- %c %Z");
  EXPECT_NE(overview.find(expected.str()), string::npos)
      << "expected to find: " << expected.str() << "\nin overview:\n"
      << overview;
}
