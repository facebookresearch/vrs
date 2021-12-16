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

#include <vrs/helpers/EnumStringConverter.h>
#include <vrs/helpers/Strings.h>

struct StringsHelpersTester : testing::Test {};

using namespace std;
using namespace vrs;

TEST_F(StringsHelpersTester, strcasecmpTest) {
  EXPECT_EQ(helpers::strcasecmp("hello", "Hello"), 0);
  EXPECT_EQ(helpers::strcasecmp("hello", "HELLO"), 0);
  EXPECT_EQ(helpers::strcasecmp("hellO", "HELLO"), 0);
  EXPECT_GT(helpers::strcasecmp("hello", "bye"), 0);
  EXPECT_LT(helpers::strcasecmp("bye", "hello"), 0);
}

TEST_F(StringsHelpersTester, strncasecmpTest) {
  EXPECT_EQ(helpers::strncasecmp("hello New-York", "Hello Paris", 6), 0);
  EXPECT_LT(helpers::strncasecmp("hello New-York", "Hello Paris", 7), 0);
  EXPECT_GT(helpers::strncasecmp("hello New-York", "Hello ", 7), 0);
}

TEST_F(StringsHelpersTester, trim) {
  EXPECT_STREQ(helpers::trim("").c_str(), "");
  EXPECT_STREQ(helpers::trim(" ").c_str(), "");
  EXPECT_STREQ(helpers::trim("\t").c_str(), "");
  EXPECT_STREQ(helpers::trim(" \t ").c_str(), "");
  EXPECT_STREQ(helpers::trim(" he l\tlo ").c_str(), "he l\tlo");
  EXPECT_STREQ(helpers::trim(" hello").c_str(), "hello");
  EXPECT_STREQ(helpers::trim("hello ").c_str(), "hello");
  EXPECT_STREQ(helpers::trim("hello\t").c_str(), "hello");

  EXPECT_STREQ(helpers::trim(" hello ", " ").c_str(), "hello");
  EXPECT_STREQ(helpers::trim(" hello ", "").c_str(), " hello ");
  EXPECT_STREQ(helpers::trim("hello\r", " \t\n\r").c_str(), "hello");
  EXPECT_STREQ(helpers::trim("\n", " \t\n\r").c_str(), "");
  EXPECT_STREQ(helpers::trim(" ", " \t\n\r").c_str(), "");
  EXPECT_STREQ(helpers::trim("\t", " \t\n\r").c_str(), "");
  EXPECT_STREQ(helpers::trim("\n", " \t\n\r").c_str(), "");
  EXPECT_STREQ(helpers::trim("\r", " \t\n\r").c_str(), "");
  EXPECT_STREQ(helpers::trim("\rhello \t\n\rhello", " \t\n\r").c_str(), "hello \t\n\rhello");
}

TEST_F(StringsHelpersTester, startsWith) {
  using namespace vrs::helpers;
  EXPECT_TRUE(startsWith("hello", ""));
  EXPECT_TRUE(startsWith("hello", "h"));
  EXPECT_TRUE(startsWith("hello", "he"));
  EXPECT_TRUE(startsWith("hello", "hel"));
  EXPECT_TRUE(startsWith("hello", "hell"));
  EXPECT_TRUE(startsWith("hello", "hello"));
  EXPECT_FALSE(startsWith("hello", "helloo"));
  EXPECT_TRUE(startsWith("hello", "H"));
  EXPECT_TRUE(startsWith("hello", "hE"));
  EXPECT_TRUE(startsWith("hello", "hEl"));
  EXPECT_TRUE(startsWith("hello", "HELL"));
  EXPECT_TRUE(startsWith("hello", "HELLo"));
  EXPECT_TRUE(startsWith("hello", "HELLO"));
  EXPECT_TRUE(startsWith("", ""));
  EXPECT_FALSE(startsWith("", "a"));
  EXPECT_FALSE(startsWith("ba", "a"));
}

TEST_F(StringsHelpersTester, endsWith) {
  using namespace vrs::helpers;
  EXPECT_TRUE(endsWith("hello", ""));
  EXPECT_TRUE(endsWith("hello", "o"));
  EXPECT_TRUE(endsWith("hello", "lo"));
  EXPECT_TRUE(endsWith("hello", "llo"));
  EXPECT_TRUE(endsWith("hello", "ello"));
  EXPECT_TRUE(endsWith("hello", "hello"));
  EXPECT_FALSE(endsWith("hello", "hhello"));
  EXPECT_TRUE(endsWith("hello", "O"));
  EXPECT_TRUE(endsWith("hello", "LO"));
  EXPECT_TRUE(endsWith("hello", "LLO"));
  EXPECT_TRUE(endsWith("hello", "ELlO"));
  EXPECT_TRUE(endsWith("hello", "HElLO"));
  EXPECT_TRUE(endsWith("", ""));
  EXPECT_FALSE(endsWith("", "a"));
  EXPECT_FALSE(endsWith("ba", "b"));
}

TEST_F(StringsHelpersTester, makePrintableTest) {
  using namespace vrs::helpers;
  string kStrings[] = {"hello\n", "\t", {0}, {0, 13, 10, 32, 9, 8, 127, 0x1b, 1, 0, 2}, ""};
  string kCleanStrings[] = {
      "hello\\n", "\\t", "\\x00", "\\x00\\r\\n \\t\\b\\x7f\\e\\x01\\x00\\x02", ""};
  size_t kCount = COUNT_OF(kStrings);
  for (size_t k = 0; k < kCount; k++) {
    EXPECT_EQ(make_printable(kStrings[k]), kCleanStrings[k]);
  }
  string test;
  test.reserve(256);
  for (int c = 0; c < 256; c++) {
    test.push_back(static_cast<char>(c));
  }
  EXPECT_EQ(
      make_printable(test),
      "\\x00\\x01\\x02\\x03\\x04\\x05\\x06\\x07\\b\\t\\n\\x0b\\x0c\\r\\x0e\\x0f\\x10\\x11\\x12\\x13"
      "\\x14\\x15\\x16\\x17\\x18\\x19\\x1a\\e\\x1c\\x1d\\x1e\\x1f !\"#$%&'()*+,-./0123456789:;<=>?@"
      "ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrstuvwxyz{|}~\\x7f\\x80\\x81\\x82\\x83"
      "\\x84\\x85\\x86\\x87\\x88\\x89\\x8a\\x8b\\x8c\\x8d\\x8e\\x8f\\x90\\x91\\x92\\x93\\x94\\x95"
      "\\x96\\x97\\x98\\x99\\x9a\\x9b\\x9c\\x9d\\x9e\\x9f\\xa0\\xa1\\xa2\\xa3\\xa4\\xa5\\xa6\\xa7"
      "\\xa8\\xa9\\xaa\\xab\\xac\\xad\\xae\\xaf\\xb0\\xb1\\xb2\\xb3\\xb4\\xb5\\xb6\\xb7\\xb8\\xb9"
      "\\xba\\xbb\\xbc\\xbd\\xbe\\xbf\\xc0\\xc1\\xc2\\xc3\\xc4\\xc5\\xc6\\xc7\\xc8\\xc9\\xca\\xcb"
      "\\xcc\\xcd\\xce\\xcf\\xd0\\xd1\\xd2\\xd3\\xd4\\xd5\\xd6\\xd7\\xd8\\xd9\\xda\\xdb\\xdc\\xdd"
      "\\xde\\xdf\\xe0\\xe1\\xe2\\xe3\\xe4\\xe5\\xe6\\xe7\\xe8\\xe9\\xea\\xeb\\xec\\xed\\xee\\xef"
      "\\xf0\\xf1\\xf2\\xf3\\xf4\\xf5\\xf6\\xf7\\xf8\\xf9\\xfa\\xfb\\xfc\\xfd\\xfe\\xff");
}

TEST_F(StringsHelpersTester, humanReadableDurationTest) {
  using namespace vrs::helpers;
  const double kMinute = 60;
  const double kHour = 60 * kMinute;
  const double kDay = 24 * kHour;
  const double kWeek = 7 * kDay;
  const double kYear = 31557600; // Julian astronomical year
  EXPECT_EQ(humanReadableDuration(0), "0.000s");
  EXPECT_EQ(humanReadableDuration(4 * kDay + 3 * kHour + 2 * kMinute + 15.001), "4d 3h 2m 15.001s");
  EXPECT_EQ(humanReadableDuration(38 * kDay + 0.001), "5w 3d 0h 0m 0.001s");
  EXPECT_EQ(
      humanReadableDuration(
          kYear * 860 + 6 * kWeek + 3 * kDay + 5 * kHour + 10 * kMinute + 15.123456),
      "860y 6w 3d 5h 10m 15.123s");
  EXPECT_EQ(humanReadableDuration(13 * kHour + 59 * kMinute + 59.001), "13h 59m 59.001s");
  EXPECT_EQ(humanReadableDuration(24 * kMinute), "24m 0.000s");
  EXPECT_EQ(humanReadableDuration(-3.2), "-3.200s");
  EXPECT_EQ(humanReadableDuration(5000000000 * kYear), "1.578e+17s");
}
