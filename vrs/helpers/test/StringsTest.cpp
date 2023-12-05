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
  EXPECT_EQ(humanReadableDuration(1), "1.000s");
  EXPECT_EQ(humanReadableDuration(999e-3), "999ms");
  EXPECT_EQ(humanReadableDuration(123e-3), "123ms");
  EXPECT_EQ(humanReadableDuration(2e-3), "2ms");
  EXPECT_EQ(humanReadableDuration(1999e-6), "1999us");
  EXPECT_EQ(humanReadableDuration(2e-6), "2us");
  EXPECT_EQ(humanReadableDuration(1999e-9), "1999ns");
  EXPECT_EQ(humanReadableDuration(2e-9), "2ns");
  EXPECT_EQ(humanReadableDuration(1999e-12), "1999ps");
  EXPECT_EQ(humanReadableDuration(2e-12), "2ps");
  EXPECT_EQ(humanReadableDuration(1999e-15), "1999fs");
  EXPECT_EQ(humanReadableDuration(2e-15), "2fs");
  EXPECT_EQ(humanReadableDuration(1999e-18), "1.999fs");
  EXPECT_EQ(humanReadableDuration(19e-19), "0.0019fs");
  EXPECT_EQ(humanReadableDuration(1e-25), "1e-10fs");
  EXPECT_EQ(humanReadableDuration(1000), "16m 40.000s");
  EXPECT_EQ(
      humanReadableDuration(4 * kDay + 3 * kHour + 2 * kMinute + 15.001), "4 days, 3h 2m 15.001s");
  EXPECT_EQ(humanReadableDuration(38 * kDay + 0.001), "5 weeks, 3 days, 0h 0m 0.001s");
  EXPECT_EQ(
      humanReadableDuration(
          kYear * 860 + 6 * kWeek + 3 * kDay + 5 * kHour + 10 * kMinute + 15.123456),
      "860 years, 6 weeks, 3 days, 5h 10m 15.123s");
  EXPECT_EQ(humanReadableDuration(13 * kHour + 59 * kMinute + 59.001), "13h 59m 59.001s");
  EXPECT_EQ(humanReadableDuration(24 * kMinute), "24m 0.000s");
  EXPECT_EQ(humanReadableDuration(-3.2), "-3.200s");
  EXPECT_EQ(humanReadableDuration(5000000000 * kYear), "1.577880000e+17s");
}

TEST_F(StringsHelpersTester, getValueTest) {
  using namespace vrs::helpers;
  map<string, string> m = {
      {"bool_true", "1"},
      {"bool_false", "false"},
      {"bool_0", "0"},
      {"int", "1234567890"},
      {"int64_pos", "1234567890"},
      {"int64_neg", "-1234567890"},
      {"uint64", "1234567890"},
      {"uint64_neg", "-1"},
      {"double", "-3.5"},
      {"double_bad", "abc"},
  };
  bool boolValue;
  EXPECT_TRUE(getBool(m, "bool_true", boolValue));
  EXPECT_EQ(boolValue, true);
  EXPECT_TRUE(getBool(m, "bool_false", boolValue));
  EXPECT_EQ(boolValue, false);
  EXPECT_TRUE(getBool(m, "bool_0", boolValue));
  EXPECT_EQ(boolValue, false);
  EXPECT_FALSE(getBool(m, "nobool", boolValue));

  int intValue;
  EXPECT_TRUE(getInt(m, "int", intValue));
  EXPECT_EQ(intValue, 1234567890);
  EXPECT_FALSE(getInt(m, "noint", intValue));

  int64_t int64Value;
  EXPECT_TRUE(getInt64(m, "int64_pos", int64Value));
  EXPECT_EQ(int64Value, 1234567890);
  EXPECT_TRUE(getInt64(m, "int64_neg", int64Value));
  EXPECT_EQ(int64Value, -1234567890);
  EXPECT_FALSE(getInt64(m, "noint64", int64Value));

  uint64_t uint64Value;
  EXPECT_TRUE(getUInt64(m, "uint64", uint64Value));
  EXPECT_EQ(uint64Value, 1234567890);
  EXPECT_TRUE(getUInt64(m, "uint64_neg", uint64Value));
  EXPECT_EQ(uint64Value, 0xffffffffffffffff);
  EXPECT_FALSE(getUInt64(m, "nouint64", uint64Value));

  double doubleValue;
  EXPECT_TRUE(getDouble(m, "double", doubleValue));
  EXPECT_EQ(doubleValue, -3.5);
  EXPECT_FALSE(getDouble(m, "double_bad", doubleValue));
}

TEST_F(StringsHelpersTester, humanReadableTimestampTest) {
  using namespace vrs::helpers;
  EXPECT_EQ(humanReadableTimestamp(0), "0.000");
  EXPECT_EQ(humanReadableTimestamp(0.001), "0.001");
  EXPECT_EQ(humanReadableTimestamp(-0.001), "-0.001");
  EXPECT_EQ(humanReadableTimestamp(0.0009999), "9.999e-04");
  EXPECT_EQ(humanReadableTimestamp(0, 6), "0.000000");
  EXPECT_EQ(humanReadableTimestamp(0.001, 6), "0.001000");
  EXPECT_EQ(humanReadableTimestamp(0.0009999, 6), "0.001000");
  EXPECT_EQ(humanReadableTimestamp(0, 9), "0.000000000");
  EXPECT_EQ(humanReadableTimestamp(0.001, 9), "0.001000000");
  EXPECT_EQ(humanReadableTimestamp(0.0009999, 9), "0.000999900");
  EXPECT_EQ(humanReadableTimestamp(0.0000009, 9), "0.000000900");
  EXPECT_EQ(humanReadableTimestamp(0.000000001, 9), "0.000000001");
  EXPECT_EQ(humanReadableTimestamp(0.0000000009, 9), "9.000e-10");
  EXPECT_EQ(humanReadableTimestamp(1000000000, 9), "1000000000.000000000");
  EXPECT_EQ(humanReadableTimestamp(10000000000, 9), "1.000000000e+10");

  EXPECT_EQ(humanReadableTimestamp(1. / 1000, 3), "0.001");
  EXPECT_EQ(humanReadableTimestamp(1. / 1000000, 6), "0.000001");
  EXPECT_EQ(humanReadableTimestamp(1. / 1000000000, 9), "0.000000001");
  EXPECT_EQ(humanReadableTimestamp(123456789. / 1000000000, 9), "0.123456789");
  EXPECT_EQ(humanReadableTimestamp(1234567123456789. / 1000000000, 9), "1234567.123456789");

  EXPECT_EQ(humanReadableTimestamp(5.3, 3), "5.300");
  EXPECT_EQ(humanReadableTimestamp(5.3, 6), "5.300000");
  EXPECT_EQ(humanReadableTimestamp(5.3, 9), "5.300000000");
  EXPECT_EQ(humanReadableTimestamp(50.3, 3), "50.300");
  EXPECT_EQ(humanReadableTimestamp(500.3, 3), "500.300");
  EXPECT_EQ(humanReadableTimestamp(5000.3, 3), "5000.300");
  EXPECT_EQ(humanReadableTimestamp(50000.3, 3), "50000.300");
  EXPECT_EQ(humanReadableTimestamp(500000.3, 3), "500000.300");
  EXPECT_EQ(humanReadableTimestamp(5000000.3, 3), "5000000.300");
  EXPECT_EQ(humanReadableTimestamp(50000000.3, 3), "50000000.300");
  EXPECT_EQ(humanReadableTimestamp(500000000.3, 3), "500000000.300");
  EXPECT_EQ(humanReadableTimestamp(5000000000.3, 3), "5000000000.300");
  EXPECT_EQ(humanReadableTimestamp(50000000000.3, 3), "5.000000000e+10");
}

TEST_F(StringsHelpersTester, readUnsignedInt32) {
  using namespace vrs::helpers;
  uint32_t outInt;
  const char* strInt = "123";
  readUInt32(strInt, outInt);
  EXPECT_EQ(outInt, 123);

  const char* strWord = "vrs";
  EXPECT_EQ(readUInt32(strWord, outInt), false);
}

TEST_F(StringsHelpersTester, replaceAllTest) {
  string str = "hello world";
  EXPECT_TRUE(helpers::replaceAll(str, " ", "_"));
  EXPECT_EQ(str, "hello_world");

  EXPECT_TRUE(helpers::replaceAll(str, "world", "worlds"));
  EXPECT_EQ(str, "hello_worlds");

  str = "hello\\nworld\\nI'm\\ncoming\\n";
  EXPECT_TRUE(helpers::replaceAll(str, "\\n", "\n"));
  EXPECT_EQ(str, "hello\nworld\nI'm\ncoming\n");
  EXPECT_FALSE(helpers::replaceAll(str, "a", "b"));
  EXPECT_EQ(str, "hello\nworld\nI'm\ncoming\n");

  str = "hello";
  EXPECT_TRUE(helpers::replaceAll(str, str, "bye"));
  EXPECT_EQ(str, "bye");

  str = "[[[]]]";
  EXPECT_TRUE(helpers::replaceAll(str, "[", "{"));
  EXPECT_TRUE(helpers::replaceAll(str, "]]]", "}"));
  EXPECT_EQ(str, "{{{}");
  EXPECT_TRUE(helpers::replaceAll(str, "}", "}}}}"));
  EXPECT_EQ(str, "{{{}}}}");
  EXPECT_TRUE(helpers::replaceAll(str, "{", "{{"));
  EXPECT_EQ(str, "{{{{{{}}}}");
}

TEST_F(StringsHelpersTester, splitTest) {
  string str =
      "Lorem ipsum dolor sit amet, consectetur adipiscing elit, "
      "sed do eiusmod tempor incididunt ut labore et dolore magna aliqua.";
  vector<string> expectedTokens{
      "Lorem ipsum dolor sit amet",
      " consectetur adipiscing elit",
      " sed do eiusmod tempor incididunt ut labore et dolore magna aliqua."};
  vector<string> actualTokens;

  helpers::split(str, ',', actualTokens);
  EXPECT_EQ(actualTokens, expectedTokens);

  expectedTokens = {str};
  actualTokens.clear();
  helpers::split(str, '_', actualTokens);
  EXPECT_EQ(actualTokens, expectedTokens);

  actualTokens.clear();
  str = "hello elle is cool lol. le bol de lait";
  expectedTokens = {"he", "", "o e", "", "e is coo", " ", "o", ". ", "e bo", " de ", "ait"};
  helpers::split(str, 'l', actualTokens);
  EXPECT_EQ(actualTokens, expectedTokens);

  actualTokens.clear();
  expectedTokens = {"he", "o e", "e is coo", " ", "o", ". ", "e bo", " de ", "ait"};
  helpers::split(str, 'l', actualTokens, true);
  EXPECT_EQ(actualTokens, expectedTokens);

  actualTokens.clear();
  expectedTokens = {"he", "o e", "e is coo", "o", ".", "e bo", "de", "ait"};
  helpers::split(str, 'l', actualTokens, true, " ");
  EXPECT_EQ(actualTokens, expectedTokens);
}

#define CHECK_BEFORE(a, b)                    \
  EXPECT_TRUE(helpers::beforeFileName(a, b)); \
  EXPECT_FALSE(helpers::beforeFileName(b, a));

#define CHECK_SAME(a, b)                       \
  EXPECT_FALSE(helpers::beforeFileName(a, b)); \
  EXPECT_FALSE(helpers::beforeFileName(b, a));

#define CHECK_BEFORE_SELF(a) EXPECT_FALSE(helpers::beforeFileName(a, a))

TEST_F(StringsHelpersTester, beforeFileNameTest) {
  helpers::beforeFileName("part0image10.png", "part0000image011.png");

  CHECK_BEFORE_SELF("");
  CHECK_BEFORE_SELF("a");
  CHECK_BEFORE_SELF("abcd");
  CHECK_BEFORE_SELF("abcd000z");

  CHECK_BEFORE("", "a");
  CHECK_BEFORE("", "0");
  CHECK_BEFORE("00", "001");
  CHECK_BEFORE("00", "0a");
  CHECK_BEFORE("10", "011");

  CHECK_SAME("0", "00");
  CHECK_SAME("0", "0000000");
  CHECK_SAME("10", "0010");
  CHECK_SAME("123", "123");
  CHECK_SAME("123", "0123");
  CHECK_SAME("0123", "00000000123");
  CHECK_SAME("image0123section3z", "image000123section003z");
  CHECK_SAME("02image0123section3z", "2image0123section03z");

  CHECK_SAME("image10.png", "image10.png");
  CHECK_SAME("image010.png", "image10.png");
  CHECK_SAME("image0010.png", "image10.png");
  CHECK_SAME("image010.png", "image000010.png");

  CHECK_BEFORE("image10a", "image10b");
  CHECK_BEFORE("image010a", "image10b");
  CHECK_BEFORE("image010a", "image0010b");

  CHECK_BEFORE("image10.png", "image11.png");
  CHECK_BEFORE("image010.png", "image11.png");
  CHECK_BEFORE("image10.png", "image011.png");
  CHECK_BEFORE("image90.png", "image0110.png");
  CHECK_BEFORE("image90.png", "image0190.png");
  CHECK_BEFORE("image19.png", "image90.png");
  CHECK_BEFORE("image019.png", "image90.png");
  CHECK_BEFORE("image019.png", "image0090.png");
  CHECK_BEFORE("image1901.png", "image19010.png");

  CHECK_BEFORE("part0image10.png", "part0image11.png");
  CHECK_BEFORE("part00image010.png", "part0image11.png");
  CHECK_BEFORE("part0image10.png", "part0000image011.png");
  CHECK_BEFORE("part0image90.png", "part000image0110.png");
  CHECK_BEFORE("part0image90.png", "part0image0190.png");
  CHECK_BEFORE("part0image19.png", "part00image90.png");
  CHECK_BEFORE("part0image019.png", "part0image90.png");
  CHECK_BEFORE("part0image019.png", "part0image0090.png");
}
