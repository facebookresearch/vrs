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

#include <map>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include <vrs/RecordFileReader.h>
#include <vrs/RecordFileWriter.h>
#include <vrs/os/Utils.h>
#include <vrs/utils/FilterCopy.h>

using namespace std;
using namespace vrs;
using namespace vrs::utils;

namespace {

struct FilterCopyTest : testing::Test {
  string firstPath{os::getUniquePath(os::getTempFolder() + "FilterCopyTest-first")};
  string secondPath{os::getUniquePath(os::getTempFolder() + "FilterCopyTest-second")};
  string outputPath{os::getUniquePath(os::getTempFolder() + "FilterCopyTest-output")};

  ~FilterCopyTest() override {
    os::remove(firstPath);
    os::remove(secondPath);
    os::remove(outputPath);
  }

  static int writeFile(const string& path, const map<string, string>& tags) {
    RecordFileWriter writer;
    writer.addTags(tags);
    return writer.writeToFile(path);
  }
};

TEST_F(FilterCopyTest, fileTagOverridesDoNotCreateMergedTags) {
  ASSERT_EQ(writeFile(firstPath, {{"overridden", "first"}, {"preserved", "first"}}), 0);
  ASSERT_EQ(writeFile(secondPath, {{"overridden", "second"}, {"preserved", "second"}}), 0);
  FilteredFileReader firstReader;
  firstReader.setSource(firstPath);
  ASSERT_EQ(firstReader.openFile({}), 0);
  FilteredFileReader secondReader;
  secondReader.setSource(secondPath);
  ASSERT_EQ(secondReader.openFile({}), 0);
  CopyOptions options{false};
  options.getTagOverrider().fileTags["overridden"] = "final";
  vector<FilteredFileReader*> additionalReaders{&secondReader};
  ASSERT_EQ(filterMerge(firstReader, additionalReaders, outputPath, options), 0);
  RecordFileReader outputReader;
  ASSERT_EQ(outputReader.openFile(outputPath), 0);
  const auto& tags = outputReader.getTags();
  const auto overridden = tags.find("overridden");
  ASSERT_NE(overridden, tags.end());
  EXPECT_EQ(overridden->second, "final");
  EXPECT_EQ(tags.find("overridden_merged"), tags.end());
  const auto preserved = tags.find("preserved");
  ASSERT_NE(preserved, tags.end());
  EXPECT_EQ(preserved->second, "first");
  const auto preservedMerged = tags.find("preserved_merged");
  ASSERT_NE(preservedMerged, tags.end());
  EXPECT_EQ(preservedMerged->second, "second");
}

} // namespace
