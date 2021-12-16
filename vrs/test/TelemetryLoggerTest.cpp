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

#include <gtest/gtest.h>

#include <vrs/TelemetryLogger.h>

using namespace vrs;

namespace {

struct TelemetryLoggerTest : testing::Test {};

} // namespace

TEST_F(TelemetryLoggerTest, cacheTest) {
  TrafficEvent event;
  // test real urls
  const char* kStoragePathA =
      "https://interncache-ftw.fbcdn.net/v/t63.8864-7/10000000_183207295885771_544599"
      "8435176022016_n.jpg?efg=eyJ1cmxnZW4iOiJwaHBfdXJsZ2VuX2NsaWVudC9lbnRfZ2VuL0VudEdhaWFSZWNvcmRp"
      "bmdGaWxlIn0\\u00253D&_nc_ht=interncache-ftw&oh=3334291b4af972a40c0a8bfa35f620ad&oe=5E6ACB0A";
  const char* kStoragePathB =
      "https://interncache-atn.fbcdn.net/storageb/bucketname/tree/QmYwFXxNQGAwodZOpoCPEn"
      "FZnXGHbgdtxUuMgpksqceZopEWEjcVjzJdOEgpMHLx";
  event.setUrl(kStoragePathA);
  EXPECT_EQ(event.serverName, "interncache-ftw.fbcdn.net");
  event.setUrl(kStoragePathB);
  EXPECT_EQ(event.serverName, "interncache-atn.fbcdn.net");
  // test corner cases
  event.setUrl("ftp://ftp.facebook.com/dir/path/folder/file.txt");
  EXPECT_EQ(event.serverName, "ftp.facebook.com");
  event.setUrl("thefacebook.net/index.htm");
  EXPECT_EQ(event.serverName, "thefacebook.net");
  event.setUrl("http://thefacebook.net/");
  EXPECT_EQ(event.serverName, "thefacebook.net");
  event.setUrl("http://thefacebook.net");
  EXPECT_EQ(event.serverName, "thefacebook.net");
}
