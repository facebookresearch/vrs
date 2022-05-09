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

#include <memory>
#include <string>

#include <gtest/gtest.h>

#include <TestDataDir/TestDataDir.h>

#include <vrs/os/Utils.h>
#include <vrs/utils/FilteredFileReader.h>

using namespace std;
using namespace vrs;
using namespace vrs::utils;

using coretech::getTestDataDir;

struct FilteredFileReaderTest : testing::Test {
  string kTestFilePath = os::pathJoin(getTestDataDir(), "VRS_Files/sample_raw_pixel_formats.vrs");
  FilteredFileReader filteredReader;

  void SetUp() override {
    filteredReader.setSource(kTestFilePath);
    ASSERT_EQ(filteredReader.openFile({}), 0);
    ASSERT_EQ(filteredReader.reader.getStreams().size(), 19);
  }
};

TEST_F(FilteredFileReaderTest, excludeStreams) {
  utils::RecordFilterParams filters;
  filteredReader.applyFilters(filters);
  EXPECT_EQ(filteredReader.filter.streams.size(), 19);

  filters.excludeStream("100-test/synthetic/grey8");
  filteredReader.applyFilters(filters);
  EXPECT_EQ(filteredReader.filter.streams.size(), 18);

  filters.excludeStream("100-4");
  filteredReader.applyFilters(filters);
  EXPECT_EQ(filteredReader.filter.streams.size(), 17);
}

TEST_F(FilteredFileReaderTest, includeStreams) {
  {
    utils::RecordFilterParams filters;
    filters.includeStream("100-test/synthetic/grey8");
    filteredReader.applyFilters(filters);
    EXPECT_EQ(filteredReader.filter.streams.size(), 1);
  }
  {
    utils::RecordFilterParams filters;
    filters.includeStream("100-");
    filteredReader.applyFilters(filters);
    EXPECT_EQ(filteredReader.filter.streams.size(), 19);
  }
  {
    utils::RecordFilterParams filters;
    filters.includeStream("100");
    filteredReader.applyFilters(filters);
    EXPECT_EQ(filteredReader.filter.streams.size(), 19);
  }
  {
    utils::RecordFilterParams filters;
    filters.includeStream("200");
    filteredReader.applyFilters(filters);
    EXPECT_EQ(filteredReader.filter.streams.size(), 0);
  }
  {
    utils::RecordFilterParams filters;
    filters.includeStream("100-5");
    filters.includeStream("100-7");
    filters.includeStream("100-10");
    filters.includeStream("101-10");
    filteredReader.applyFilters(filters);
    EXPECT_EQ(filteredReader.filter.streams.size(), 3);
  }
}

TEST_F(FilteredFileReaderTest, includeExcludeStreams) {
  {
    utils::RecordFilterParams filters;
    filters.includeStream("100-");
    filters.excludeStream("100-");
    filteredReader.applyFilters(filters);
    EXPECT_EQ(filteredReader.filter.streams.size(), 0);
  }
  {
    utils::RecordFilterParams filters;
    filters.includeStream("100-1");
    filters.excludeStream("100-4");
    filteredReader.applyFilters(filters);
    EXPECT_EQ(filteredReader.filter.streams.size(), 1);

    filters.includeStream("100-10");
    filters.includeStream("100-15");
    filters.excludeStream("100-test/synthetic/nope");
    filters.excludeStream("100-test/synthetic/grey8");
    filteredReader.applyFilters(filters);
    EXPECT_EQ(filteredReader.filter.streams.size(), 2);

    filters.includeStream("100-");
    filteredReader.applyFilters(filters);
    EXPECT_EQ(filteredReader.filter.streams.size(), 19);
  }
}
