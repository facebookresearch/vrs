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

#include <gtest/gtest.h>
#include <map>

#include <vrs/DiskFile.h>
#include <vrs/ErrorCode.h>

using namespace std;

struct FileHandlerJsonTest : testing::Test {
  const string kJSONPathWithChunks = "{\"chunks\": [\"file1\", \"file2\"]}";
  const string kJSONPathWithSingleChunk = "{\"chunks\": [\"file1\"]}";
  const string kJSONPathWithChunksAndFileHandle =
      "{\"storage\": \"mystorage\",\"chunks\": [\"file1\", \"file2\"]}";
  const string kJSONPathWithChunksAndFileName =
      "{\"filename\": \"sample.vrs\",\"chunks\": [\"file1\", \"file2\"]}";
  const string kJSONPathWithChunksAndFileSizes =
      "{\"chunk_sizes\": [12345, 67890],\"chunks\": [\"file1\", \"file2\"]}";
  const string kJSONPathWithSingleExtraField =
      "{\"storage\": \"mystorage\",\"chunks\": [\"file1\", \"file2\"],"
      "\"bucketname\": \"bucketname1\"}";
  const string kJSONPathWithMultipleExtraField =
      "{\"storage\": \"mystorage\",\"chunks\": [\"file1\", \"file2\"],"
      "\"bucketname\": \"bucketname1\", \"extra1\": \"extra1\","
      "\"extra2\": [\"extra2-1\", \"extra2-2\"]}";
  const string kNonJSONPath = "file1";

  const string kUriPath = "mystorage:test/path/file.vrs?key1=val1&key2=val2";
  const string kUriPathWithNoHost = "test/path/file.vrs?key1=val1";
  const string kUriPathWithNoHostWithColonSlash = ":test/path/file.vrs?key1=val1";
  const string kUriPathWithNoPath = "mystorage:";
  const string kUriPathWithNoPathWithQuery = "mystorage:?key1=val1";
  const string kUriPathWithInvalidQuery = "mystorage:test/path/file.vrs?key1=";
  const string kUriPathWithInvalidQuery2 = "mystorage:test/path/file.vrs?=val1";
  const string kUriWithEncodedPath = "mystorage:test%2Fpath%2Ffile.vrs";
};

TEST_F(FileHandlerJsonTest, JSONPathWithChunks) {
  vrs::FileSpec spec;
  EXPECT_TRUE(spec.fromJson(kJSONPathWithChunks));
  EXPECT_EQ(spec.chunks.size(), 2);
  EXPECT_EQ(spec.chunks[0], "file1");
  EXPECT_EQ(spec.chunks[1], "file2");
}

TEST_F(FileHandlerJsonTest, JSONPathWithSingleChunk) {
  vrs::FileSpec spec;
  EXPECT_TRUE(spec.fromJson(kJSONPathWithSingleChunk));
  EXPECT_EQ(spec.chunks.size(), 1);
  EXPECT_EQ(spec.chunks[0], "file1");
}
TEST_F(FileHandlerJsonTest, JSONPathWithChunksAndFileHandle) {
  vrs::FileSpec spec;
  EXPECT_TRUE(spec.fromJson(kJSONPathWithChunksAndFileHandle));
  EXPECT_EQ(spec.chunks.size(), 2);
  EXPECT_EQ(spec.chunks[0], "file1");
  EXPECT_EQ(spec.chunks[1], "file2");
  EXPECT_EQ(spec.fileHandlerName, "mystorage");
}
TEST_F(FileHandlerJsonTest, JSONPathWithChunksAndFileName) {
  vrs::FileSpec spec;
  EXPECT_TRUE(spec.fromJson(kJSONPathWithChunksAndFileName));
  EXPECT_EQ(spec.chunks.size(), 2);
  EXPECT_EQ(spec.chunks[0], "file1");
  EXPECT_EQ(spec.chunks[1], "file2");
  EXPECT_EQ(spec.fileName, "sample.vrs");
}

TEST_F(FileHandlerJsonTest, JSONPathWithExtraField) {
  vrs::FileSpec spec;
  EXPECT_TRUE(spec.fromJson(kJSONPathWithSingleExtraField));
  EXPECT_EQ(spec.chunks.size(), 2);
  EXPECT_EQ(spec.fileHandlerName, "mystorage");
  EXPECT_EQ(spec.extras.size(), 1);
  EXPECT_NE(spec.extras.find("bucketname"), spec.extras.end());
  EXPECT_EQ(spec.extras["bucketname"], "bucketname1");

  EXPECT_TRUE(spec.fromJson(kJSONPathWithMultipleExtraField));
  EXPECT_EQ(spec.extras.size(), 2);
  EXPECT_NE(spec.extras.find("bucketname"), spec.extras.end());
  EXPECT_EQ(spec.extras["bucketname"], "bucketname1");
  EXPECT_NE(spec.extras.find("extra1"), spec.extras.end());
  EXPECT_EQ(spec.extras["extra1"], "extra1");
}

TEST_F(FileHandlerJsonTest, JSONPathWithChunksAndFileSizes) {
  vrs::FileSpec spec;
  EXPECT_TRUE(spec.fromJson(kJSONPathWithChunksAndFileSizes));
  EXPECT_EQ(spec.chunks.size(), 2);
  EXPECT_EQ(spec.chunks[0], "file1");
  EXPECT_EQ(spec.chunks[1], "file2");
  EXPECT_EQ(spec.chunkSizes[0], 12345);
  EXPECT_EQ(spec.chunkSizes[1], 67890);
}

TEST_F(FileHandlerJsonTest, NonJSONPath) {
  vrs::FileSpec spec;
  EXPECT_FALSE(spec.fromJson(kNonJSONPath));
}

TEST_F(FileHandlerJsonTest, ParseURI) {
  map<string, string> m;
  string fileHandlerName;
  string path;
  EXPECT_EQ(
      vrs::FileSpec::parseUri(kUriPathWithNoPath, fileHandlerName, path, m),
      vrs::INVALID_URI_FORMAT);
  EXPECT_EQ(
      vrs::FileSpec::parseUri(kUriPathWithNoPathWithQuery, fileHandlerName, path, m),
      vrs::INVALID_URI_FORMAT);
  EXPECT_EQ(
      vrs::FileSpec::parseUri(kUriPathWithNoHostWithColonSlash, fileHandlerName, path, m),
      vrs::INVALID_URI_FORMAT);

  EXPECT_EQ(vrs::FileSpec::parseUri(kUriPath, fileHandlerName, path, m), 0);
  EXPECT_EQ(path, "test/path/file.vrs");
  EXPECT_EQ(fileHandlerName, "mystorage");
  EXPECT_EQ(m.size(), 2);
  EXPECT_EQ(m["key1"], "val1");
  EXPECT_EQ(m["key2"], "val2");

  EXPECT_EQ(vrs::FileSpec::parseUri(kUriPathWithNoHost, fileHandlerName, path, m), 0);
  EXPECT_EQ(path, "test/path/file.vrs");
  EXPECT_TRUE(fileHandlerName.empty());
  EXPECT_EQ(m.size(), 1);
  EXPECT_EQ(m["key1"], "val1");

  EXPECT_EQ(vrs::FileSpec::parseUri(kUriPathWithInvalidQuery, fileHandlerName, path, m), 0);
  EXPECT_EQ(path, "test/path/file.vrs");
  EXPECT_EQ(fileHandlerName, "mystorage");
  EXPECT_TRUE(m.empty());

  EXPECT_EQ(vrs::FileSpec::parseUri(kUriPathWithInvalidQuery2, fileHandlerName, path, m), 0);
  EXPECT_EQ(path, "test/path/file.vrs");
  EXPECT_EQ(fileHandlerName, "mystorage");
  EXPECT_TRUE(m.empty());

  EXPECT_EQ(vrs::FileSpec::parseUri(kUriWithEncodedPath, fileHandlerName, path, m), 0);
  EXPECT_EQ(path, "test/path/file.vrs");
  EXPECT_EQ(fileHandlerName, "mystorage");
  EXPECT_TRUE(m.empty());
}

TEST_F(FileHandlerJsonTest, DecodeUrlQuery) {
  const string kURLQuery = "testkey=42";
  const string kURLQueryWithEncode = "testkey=value%3D%23%2F42";
  const string kURLQueryWithEncodeAndSpace = "test%20key=value%3D42";
  const string kURLQueryWithEncodeAndSpace2 = "test+key=value%3D42";
  const string kURLQueryWithInvalidCharKey = "%1F%20key=value";
  const string kURLQueryWithInvalidCharValue = "test%20key=%1F";

  string key, value;
  EXPECT_EQ(vrs::FileSpec::decodeQuery(kURLQuery, key, value), 0);
  EXPECT_EQ(key, "testkey");
  EXPECT_EQ(value, "42");

  EXPECT_EQ(vrs::FileSpec::decodeQuery(kURLQueryWithEncode, key, value), 0);
  EXPECT_EQ(key, "testkey");
  EXPECT_EQ(value, "value=#/42");
  EXPECT_EQ(vrs::FileSpec::decodeQuery(kURLQueryWithEncodeAndSpace, key, value), 0);
  EXPECT_EQ(key, "test key");
  EXPECT_EQ(value, "value=42");
  EXPECT_EQ(vrs::FileSpec::decodeQuery(kURLQueryWithEncodeAndSpace2, key, value), 0);
  EXPECT_EQ(key, "test+key");
  EXPECT_EQ(value, "value=42");

  EXPECT_EQ(
      vrs::FileSpec::decodeQuery(kURLQueryWithInvalidCharKey, key, value), vrs::INVALID_URI_VALUE);
  EXPECT_EQ(
      vrs::FileSpec::decodeQuery(kURLQueryWithInvalidCharValue, key, value),
      vrs::INVALID_URI_VALUE);
}

TEST_F(FileHandlerJsonTest, SetAndGetExtras) {
  vrs::FileSpec spec;

  const string kString = "42";
  const string kStrZero = "0";
  const string kStrFalse = "false";

  spec.setExtra("str", kString);
  spec.setExtra("int", 42);
  spec.setExtra("double", 42.0);
  spec.setExtra("bool_true", true);
  spec.setExtra("bool_false", false);
  spec.setExtra("zero", kStrZero);
  spec.setExtra("false", kStrFalse);

  EXPECT_EQ(spec.getExtra("str"), "42");
  EXPECT_EQ(spec.getExtraAsInt("int"), 42);
  EXPECT_EQ(spec.getExtraAsDouble("double"), 42.0);
  EXPECT_TRUE(spec.getExtraAsBool("bool_true"));
  EXPECT_FALSE(spec.getExtraAsBool("bool_false"));
  EXPECT_FALSE(spec.getExtraAsBool("zero"));
  EXPECT_FALSE(spec.getExtraAsBool("false"));
  EXPECT_TRUE(spec.getExtraAsBool("str"));

  spec.unsetExtra("str");
  EXPECT_FALSE(spec.hasExtra("str"));
}
