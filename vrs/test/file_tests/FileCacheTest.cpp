// Facebook Technologies, LLC Proprietary and Confidential.

#include <fstream>
#include <string>

#include <gtest/gtest.h>

#include <TestDataDir/TestDataDir.h>
#include <vrs/ErrorCode.h>
#include <vrs/FileCache.h>
#include <vrs/FileDetailsCache.h>
#include <vrs/RecordFileReader.h>
#include <vrs/os/Utils.h>

using namespace vrs;

namespace {

struct FileCacheTest : testing::Test {};

} // namespace

TEST_F(FileCacheTest, cacheTest) {
  const string mainFolder = os::getTempFolder();
  const std::string cacheName = "unit_test_vrs_file_cache";
  ASSERT_EQ(FileCache::makeFileCache(cacheName, mainFolder), 0);
  FileCache* fcache = FileCache::getFileCache();
  EXPECT_NE(fcache, nullptr);
  string location;
  EXPECT_EQ(fcache->getFile("123.txt", location), FILE_NOT_FOUND);
  {
    std::ofstream out(location);
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
  const string mainFolder = os::getTempFolder();
  const std::string cacheName = "unit_test_vrs_file_cache";
  ASSERT_EQ(FileCache::makeFileCache(cacheName, mainFolder), 0);
  FileCache* fcache = FileCache::getFileCache();
  EXPECT_NE(fcache, nullptr);
  string location;
  string domain = "domain";
  EXPECT_EQ(fcache->getFile(domain, "123.txt", location), FILE_NOT_FOUND);
  {
    std::ofstream out(location);
    out << location;
  }
  string location2;
  EXPECT_EQ(fcache->getFile(domain, "123.txt", location2), 0);
  EXPECT_EQ(location, location2);
  // put a folder in the way of the file...
  os::remove(location);
  os::makeDir(location);
  EXPECT_EQ(fcache->getFile(domain, "123.txt", location2), INVALID_DISK_DATA);
  // put a file in the way of the domain's folder
  domain = "domain2";
  EXPECT_EQ(fcache->getFile(domain, location), FILE_NOT_FOUND);
  {
    std::ofstream out(location);
    out << location;
  }
  EXPECT_EQ(fcache->getFile(domain, "123.txt", location2), INVALID_DISK_DATA);
}

void testDetails(const string& cacheFile, const RecordFileReader& reader, const bool hasIndex) {
  EXPECT_EQ(
      FileDetailsCache::write(
          cacheFile,
          reader.getStreams(),
          reader.getTags(),
          reader.getStreamTags(),
          reader.getIndex(),
          hasIndex),
      0);
  set<StreamId> streamIds;
  map<string, string> fileTags;
  map<StreamId, StreamTags> streamTags;
  vector<IndexRecord::RecordInfo> recordIndex;
  bool hasProperIndex;
  EXPECT_EQ(
      FileDetailsCache::read(
          cacheFile, streamIds, fileTags, streamTags, recordIndex, hasProperIndex),
      0);
  EXPECT_EQ(streamIds, reader.getStreams());
  EXPECT_EQ(fileTags, reader.getTags());
  EXPECT_EQ(streamTags, reader.getStreamTags());
  EXPECT_EQ(recordIndex, reader.getIndex());
  EXPECT_EQ(hasIndex, hasProperIndex);
}

TEST_F(FileCacheTest, detailsTest) {
  std::string kTestFile = string(coretech::getTestDataDir()) + "/VRS_Files/ar_camera.vrs";
  const string cacheFile = os::getTempFolder() + "detailsTest.vrsi";
  RecordFileReader reader;
  ASSERT_EQ(reader.openFile(kTestFile), 0);
  testDetails(cacheFile, reader, true);
  testDetails(cacheFile, reader, false);
}
