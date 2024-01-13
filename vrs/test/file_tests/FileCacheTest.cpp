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

#include <fstream>
#include <string>
#include <thread>

#include <gtest/gtest.h>

#include <TestDataDir/TestDataDir.h>
#include <vrs/ErrorCode.h>
#include <vrs/FileCache.h>
#include <vrs/FileDetailsCache.h>
#include <vrs/RecordFileReader.h>
#include <vrs/os/Utils.h>

using namespace std;
using namespace vrs;

namespace {

struct FileCacheTest : testing::Test {};

} // namespace

TEST_F(FileCacheTest, cacheTest) {
  const string& mainFolder = os::getTempFolder();
  const string cacheName = "unit_test_vrs_file_cache";
  ASSERT_EQ(FileCache::makeFileCache(cacheName, mainFolder), 0);
  FileCache* fcache = FileCache::getFileCache();
  EXPECT_NE(fcache, nullptr);
  string location;
  EXPECT_EQ(fcache->getFile("123.txt", location), FILE_NOT_FOUND);
  {
    ofstream out(location);
    out << location;
  }
  string location2;
  EXPECT_EQ(fcache->getFile("123.txt", location2), 0);
  EXPECT_EQ(location, location2);
  // put a folder in the way...
  os::remove(location2);
  os::makeDir(location2);
  EXPECT_EQ(fcache->getFile("123.txt", location2), INVALID_DISK_DATA);
}

TEST_F(FileCacheTest, cacheDomainTest) {
  const string& mainFolder = os::getTempFolder();
  const string cacheName = "unit_test_vrs_file_cache";
  ASSERT_EQ(FileCache::makeFileCache(cacheName, mainFolder), 0);
  FileCache* fcache = FileCache::getFileCache();
  EXPECT_NE(fcache, nullptr);
  string location;
  string domain = "domain";
  EXPECT_EQ(fcache->getFile(domain, "123.txt", location), FILE_NOT_FOUND);
  {
    ofstream out(location);
    out << location;
  }
  string location2;
  EXPECT_EQ(fcache->getFile(domain, "123.txt", location2), 0);
  EXPECT_EQ(location, location2);
  string str = DiskFile::readTextFile(location);
  EXPECT_EQ(str, location);
  // put a folder in the way of the file...
  os::remove(location);
  os::makeDir(location);
  EXPECT_EQ(fcache->getFile(domain, "123.txt", location2), INVALID_DISK_DATA);
  // put a file in the way of the domain's folder
  domain = "domain2";
  EXPECT_EQ(fcache->getFile(domain, location), FILE_NOT_FOUND);
  {
    ofstream out(location);
    out << location;
  }
  EXPECT_EQ(fcache->getFile(domain, "123.txt", location2), INVALID_DISK_DATA);
}

void verifyDetails(
    const string& cacheFile,
    const RecordFileReader& reader,
    const bool hasIndex,
    bool failOk) {
  set<StreamId> streamIds;
  map<string, string> fileTags;
  map<StreamId, StreamTags> streamTags;
  vector<IndexRecord::RecordInfo> recordIndex;
  bool hasProperIndex = false;
  int readStatus = FileDetailsCache::read(
      cacheFile, streamIds, fileTags, streamTags, recordIndex, hasProperIndex);
  if (readStatus == 0) {
    EXPECT_EQ(streamIds, reader.getStreams());
    EXPECT_EQ(fileTags, reader.getTags());
    EXPECT_EQ(streamTags, reader.getStreamTags());
    EXPECT_EQ(recordIndex, reader.getIndex());
    EXPECT_EQ(hasIndex, hasProperIndex);
  } else {
    EXPECT_TRUE(failOk || readStatus == 0);
  }
}

void testDetails(
    const string& cacheFile,
    const RecordFileReader& reader,
    const bool hasIndex,
    bool failOk) {
  int writeStatus = FileDetailsCache::write(
      cacheFile,
      reader.getStreams(),
      reader.getTags(),
      reader.getStreamTags(),
      reader.getIndex(),
      hasIndex);
  EXPECT_TRUE(failOk || writeStatus == 0);
  verifyDetails(cacheFile, reader, hasIndex, failOk);
}

struct ThreadParam {
  const string& cacheFile;
  const RecordFileReader& reader;
  const bool hasIndex;
};

void createRecordsThreadTask(ThreadParam* param) {
  testDetails(param->cacheFile, param->reader, param->hasIndex, true);
}

TEST_F(FileCacheTest, detailsTest) {
  string kTestFile = string(coretech::getTestDataDir()) + "/VRS_Files/sample_file.vrs";
  const string cacheFile = os::getTempFolder() + "detailsTest.vrsi";
  RecordFileReader reader;
  ASSERT_EQ(reader.openFile(kTestFile), 0);
  testDetails(cacheFile, reader, true, false);
  testDetails(cacheFile, reader, false, false);

  const uint32_t kThreadCount = thread::hardware_concurrency();

  vector<thread> threads;
  threads.reserve(kThreadCount);
  ThreadParam params{cacheFile, reader, false};
  for (uint32_t threadIndex = 0; threadIndex < kThreadCount; threadIndex++) {
    threads.emplace_back(createRecordsThreadTask, &params);
  }
  for (uint32_t threadIndex = 0; threadIndex < kThreadCount; threadIndex++) {
    threads[threadIndex].join();
  }
  verifyDetails(cacheFile, reader, false, true);
}
