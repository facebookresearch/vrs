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
  EXPECT_NE(helpers::strcasecmp("hello", "Helloo"), 0);
  EXPECT_NE(helpers::strcasecmp("hello", "Hella"), 0);
}

TEST_F(StringsHelpersTester, strcasecmpStringViewTest) {
  using namespace std;

  // Equal strings with different cases
  EXPECT_EQ(helpers::strcasecmp(string_view("hello"), string_view("Hello")), 0);
  EXPECT_EQ(helpers::strcasecmp(string_view("hello"), string_view("HELLO")), 0);
  EXPECT_EQ(helpers::strcasecmp(string_view("HELLO"), string_view("hello")), 0);
  EXPECT_EQ(helpers::strcasecmp(string_view("HeLLo"), string_view("hEllO")), 0);

  // Empty strings
  EXPECT_EQ(helpers::strcasecmp(string_view(""), string_view("")), 0);
  EXPECT_LT(helpers::strcasecmp(string_view(""), string_view("a")), 0);
  EXPECT_GT(helpers::strcasecmp(string_view("a"), string_view("")), 0);

  // Different lengths with same prefix - longer string is greater
  EXPECT_LT(helpers::strcasecmp(string_view("hello"), string_view("helloo")), 0);
  EXPECT_GT(helpers::strcasecmp(string_view("helloo"), string_view("hello")), 0);
  EXPECT_LT(helpers::strcasecmp(string_view("HELLO"), string_view("helloWorld")), 0);
  EXPECT_GT(helpers::strcasecmp(string_view("HelloWorld"), string_view("hello")), 0);

  // Different content - ordering tests
  EXPECT_LT(helpers::strcasecmp(string_view("apple"), string_view("Banana")), 0);
  EXPECT_GT(helpers::strcasecmp(string_view("banana"), string_view("Apple")), 0);
  EXPECT_LT(helpers::strcasecmp(string_view("abc"), string_view("ABD")), 0);
  EXPECT_GT(helpers::strcasecmp(string_view("abd"), string_view("ABC")), 0);

  // Mixed with string_view from substrings (common use case)
  string fullString = "prefix_Hello_suffix";
  string_view extracted = string_view(fullString).substr(7, 5); // "Hello"
  EXPECT_EQ(helpers::strcasecmp(extracted, string_view("HELLO")), 0);
  EXPECT_EQ(helpers::strcasecmp(string_view("hello"), extracted), 0);

  // Single character comparisons
  EXPECT_EQ(helpers::strcasecmp(string_view("A"), string_view("a")), 0);
  EXPECT_LT(helpers::strcasecmp(string_view("A"), string_view("B")), 0);
  EXPECT_GT(helpers::strcasecmp(string_view("b"), string_view("A")), 0);
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

TEST_F(StringsHelpersTester, trimView) {
  using namespace vrs::helpers;

  EXPECT_EQ(trimView(""), "");
  EXPECT_EQ(trimView(" "), "");
  EXPECT_EQ(trimView("\t"), "");
  EXPECT_EQ(trimView(" \t "), "");
  EXPECT_EQ(trimView(" he l\tlo "), "he l\tlo");
  EXPECT_EQ(trimView(" hello"), "hello");
  EXPECT_EQ(trimView("hello "), "hello");
  EXPECT_EQ(trimView("hello\t"), "hello");

  EXPECT_EQ(trimView(" hello ", " "), "hello");
  EXPECT_EQ(trimView(" hello ", ""), " hello ");
  EXPECT_EQ(trimView("hello\r", " \t\n\r"), "hello");
  EXPECT_EQ(trimView("\n", " \t\n\r"), "");
  EXPECT_EQ(trimView(" ", " \t\n\r"), "");
  EXPECT_EQ(trimView("\t", " \t\n\r"), "");
  EXPECT_EQ(trimView("\n", " \t\n\r"), "");
  EXPECT_EQ(trimView("\r", " \t\n\r"), "");
  EXPECT_EQ(trimView("\rhello \t\n\rhello", " \t\n\r"), "hello \t\n\rhello");

  // Test that trimView returns a view into the original string (zero allocation)
  string original = "  hello world  ";
  string_view result = trimView(original);
  EXPECT_EQ(result, "hello world");
  EXPECT_EQ(result.data(), original.data() + 2);
  EXPECT_EQ(result.size(), 11);

  // Test with string_view input
  string_view sv = "  test  ";
  EXPECT_EQ(trimView(sv), "test");

  // Test edge cases
  EXPECT_EQ(trimView("   "), "");
  EXPECT_EQ(trimView("hello"), "hello");
  EXPECT_EQ(trimView("a"), "a");
  EXPECT_EQ(trimView(" a "), "a");
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

TEST_F(StringsHelpersTester, humanReadableFileSizeTest) {
  using namespace vrs::helpers;
  int64_t kB = 1024;
  EXPECT_EQ(humanReadableFileSize(0), "0 B");
  EXPECT_EQ(humanReadableFileSize(kB - 1), "1023 B");
  EXPECT_EQ(humanReadableFileSize(kB), "1.00 KiB");
  EXPECT_EQ(humanReadableFileSize(kB * kB - 1), "1023 KiB");
  EXPECT_EQ(humanReadableFileSize(kB * kB), "1.00 MiB");
  EXPECT_EQ(humanReadableFileSize(kB * kB * 10 - 1), "9.99 MiB");
  EXPECT_EQ(humanReadableFileSize(kB * kB * 10), "10.0 MiB");
  EXPECT_EQ(humanReadableFileSize(kB * kB * 100 - 1), "99.9 MiB");
  EXPECT_EQ(humanReadableFileSize(kB * kB * 100), "100 MiB");
  EXPECT_EQ(humanReadableFileSize(kB * kB * kB - 1), "1023 MiB");
  EXPECT_EQ(humanReadableFileSize(kB * kB * kB), "1.00 GiB");
  EXPECT_EQ(humanReadableFileSize(kB * kB * kB * 9), "9.00 GiB");
  EXPECT_EQ(humanReadableFileSize(kB * kB * kB * 10 - 1), "9.99 GiB");
  EXPECT_EQ(humanReadableFileSize(kB * kB * kB * 10), "10.0 GiB");
  EXPECT_EQ(humanReadableFileSize(kB * kB * kB * 99), "99.0 GiB");
  EXPECT_EQ(humanReadableFileSize(kB * kB * kB * 100 - 1), "99.9 GiB");
  EXPECT_EQ(humanReadableFileSize(kB * kB * kB * 100), "100 GiB");
  EXPECT_EQ(humanReadableFileSize(kB * kB * kB * kB - 1), "1023 GiB");
  EXPECT_EQ(humanReadableFileSize(kB * kB * kB * kB), "1.00 TiB");
  EXPECT_EQ(humanReadableFileSize(kB * kB * kB * kB * 10 - 1), "9.99 TiB");
  EXPECT_EQ(humanReadableFileSize(kB * kB * kB * kB * 10), "10.0 TiB");
  EXPECT_EQ(humanReadableFileSize(kB * kB * kB * kB * 100 - 1), "99.9 TiB");
  EXPECT_EQ(humanReadableFileSize(kB * kB * kB * kB * 100), "100 TiB");
  EXPECT_EQ(humanReadableFileSize(kB * kB * kB * kB * kB - 1), "1023 TiB");
  EXPECT_EQ(humanReadableFileSize(kB * kB * kB * kB * kB), "1.00 PiB");
  EXPECT_EQ(humanReadableFileSize(kB * kB * kB * kB * kB * 10 - 1), "9.99 PiB");
  EXPECT_EQ(humanReadableFileSize(kB * kB * kB * kB * kB * 10), "10.0 PiB");
  EXPECT_EQ(humanReadableFileSize(kB * kB * kB * kB * kB * 100 - 1), "99.9 PiB");
  EXPECT_EQ(humanReadableFileSize(kB * kB * kB * kB * kB * 100), "100 PiB");
  EXPECT_EQ(humanReadableFileSize(kB * kB * kB * kB * kB * kB - 1), "1023 PiB");
  EXPECT_EQ(humanReadableFileSize(kB * kB * kB * kB * kB * kB), "1.00 EiB");
  EXPECT_EQ(
      humanReadableFileSize(kB * kB * kB * kB * kB * kB + kB * kB * kB * kB * kB * kB / 2),
      "1.50 EiB");
  EXPECT_EQ(
      humanReadableFileSize(kB * kB * kB * kB * kB * kB + kB * kB * kB * kB * kB * kB / 100 * 99),
      "1.99 EiB");
  EXPECT_EQ(humanReadableFileSize(kB * kB * kB * kB * kB * kB * 2), "2.00 EiB"); // 61 bits
  EXPECT_EQ(humanReadableFileSize(kB * kB * kB * kB * kB * kB * 3), "3.00 EiB");
  EXPECT_EQ(humanReadableFileSize(kB * kB * kB * kB * kB * kB * 4), "4.00 EiB"); // 62 bits
  EXPECT_EQ(humanReadableFileSize(kB * kB * kB * kB * kB * kB * 7), "7.00 EiB");
  // max limit with int64_t, or 63 bits
  EXPECT_EQ(humanReadableFileSize(0x7fffffffffffffff), "7.99 EiB");

  EXPECT_EQ(humanReadableFileSize(-(kB - 1)), "-1023 B");
  EXPECT_EQ(humanReadableFileSize(-kB), "-1.00 KiB");
  EXPECT_EQ(humanReadableFileSize(-kB * kB * kB), "-1.00 GiB");
}

TEST_F(StringsHelpersTester, StringStringMapHeterogeneousLookup) {
  // Test that StringStringMap supports heterogeneous lookup with string_view
  // This is the key feature enabled by std::less<> comparator

  StringStringMap m = {{"key1", "value1"}, {"key2", "value2"}, {"key3", "value3"}};

  // Test lookup with string_view (no temporary string created)
  string_view sv1 = "key1";
  string_view sv2 = "key2";
  string_view sv_missing = "missing";

  // find() with string_view
  auto it1 = m.find(sv1);
  EXPECT_NE(it1, m.end());
  EXPECT_EQ(it1->second, "value1");

  auto it2 = m.find(sv2);
  EXPECT_NE(it2, m.end());
  EXPECT_EQ(it2->second, "value2");

  auto it_missing = m.find(sv_missing);
  EXPECT_EQ(it_missing, m.end());

  // count() with string_view
  EXPECT_EQ(m.count(sv1), 1);
  EXPECT_EQ(m.count(sv_missing), 0);

  // Test lookup with const char* (also heterogeneous)
  auto it3 = m.find("key3");
  EXPECT_NE(it3, m.end());
  EXPECT_EQ(it3->second, "value3");

  // Test lookup with substring of another string (common use case)
  string fullString = "prefix_key1_suffix";
  string_view keyFromSubstring = string_view(fullString).substr(7, 4); // "key1"
  auto it_sub = m.find(keyFromSubstring);
  EXPECT_NE(it_sub, m.end());
  EXPECT_EQ(it_sub->second, "value1");
}

TEST_F(StringsHelpersTester, getValueTest) {
  using namespace helpers;
  StringStringMap m = {
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
      {"1234", "1234"},
      {"1KB", "1KB"},
      {"1mb", "1mb"},
      {"13TB", "13TB"},
      {"123GB", "123GB"},
      {"5EB", "5EB"},
      {"empty", ""},
      {"1B", "1B"},
  };
  bool boolValue = false;
  EXPECT_TRUE(getBool(m, "bool_true", boolValue));
  EXPECT_EQ(boolValue, true);
  EXPECT_TRUE(getBool(m, "bool_false", boolValue));
  EXPECT_EQ(boolValue, false);
  EXPECT_TRUE(getBool(m, "bool_0", boolValue));
  EXPECT_EQ(boolValue, false);
  EXPECT_FALSE(getBool(m, "nobool", boolValue));

  int intValue = 0;
  EXPECT_TRUE(getInt(m, "int", intValue));
  EXPECT_EQ(intValue, 1234567890);
  EXPECT_FALSE(getInt(m, "noint", intValue));

  int64_t int64Value = 0;
  EXPECT_TRUE(getInt64(m, "int64_pos", int64Value));
  EXPECT_EQ(int64Value, 1234567890);
  EXPECT_TRUE(getInt64(m, "int64_neg", int64Value));
  EXPECT_EQ(int64Value, -1234567890);
  EXPECT_FALSE(getInt64(m, "noint64", int64Value));

  uint64_t uint64Value = 0;
  EXPECT_TRUE(getUInt64(m, "uint64", uint64Value));
  EXPECT_EQ(uint64Value, 1234567890);
  EXPECT_FALSE(getUInt64(m, "uint64_neg", uint64Value));
  EXPECT_EQ(uint64Value, 0);
  EXPECT_FALSE(getUInt64(m, "nouint64", uint64Value));

  double doubleValue = 0;
  EXPECT_TRUE(getDouble(m, "double", doubleValue));
  EXPECT_EQ(doubleValue, -3.5);
  EXPECT_FALSE(getDouble(m, "double_bad", doubleValue));

  uint64_t byteSize = 0;
  EXPECT_TRUE(getByteSize(m, "1234", byteSize));
  EXPECT_EQ(byteSize, 1234);
  EXPECT_TRUE(getByteSize(m, "1KB", byteSize));
  EXPECT_EQ(byteSize, 1024);
  EXPECT_TRUE(getByteSize(m, "1mb", byteSize));
  EXPECT_EQ(byteSize, 1024 * 1024);
  EXPECT_TRUE(getByteSize(m, "13TB", byteSize));
  EXPECT_EQ(byteSize, 13ULL * 1024 * 1024 * 1024 * 1024);
  EXPECT_TRUE(getByteSize(m, "123GB", byteSize));
  EXPECT_EQ(byteSize, 123ULL * 1024 * 1024 * 1024);
  EXPECT_TRUE(getByteSize(m, "5EB", byteSize));
  EXPECT_EQ(byteSize, 5ULL * 1024 * 1024 * 1024 * 1024 * 1024);
  EXPECT_FALSE(getByteSize(m, "nobytesize", byteSize));
  EXPECT_EQ(byteSize, 0);
  EXPECT_FALSE(getByteSize(m, "empty", byteSize));
  EXPECT_EQ(byteSize, 0);
  EXPECT_TRUE(getByteSize(m, "1B", byteSize));
  EXPECT_EQ(byteSize, 1);
}

// Enhanced tests for getInt - Added for from_chars migration
TEST_F(StringsHelpersTester, getInt_EdgeCases) {
  using namespace helpers;
  StringStringMap m;
  int value = 0;

  // Valid negative
  m["neg"] = "-123";
  EXPECT_TRUE(getInt(m, "neg", value));
  EXPECT_EQ(value, -123);

  // Valid zero
  m["zero"] = "0";
  EXPECT_TRUE(getInt(m, "zero", value));
  EXPECT_EQ(value, 0);

  // Max int32
  m["max"] = "2147483647";
  EXPECT_TRUE(getInt(m, "max", value));
  EXPECT_EQ(value, 2147483647);

  // Min int32
  m["min"] = "-2147483648";
  EXPECT_TRUE(getInt(m, "min", value));
  EXPECT_EQ(value, -2147483648);

  // Partial parse - should fail with from_chars (more restrictive)
  m["partial"] = "123abc";
  EXPECT_FALSE(getInt(m, "partial", value));

  // Invalid - non-numeric
  m["invalid"] = "abc";
  EXPECT_FALSE(getInt(m, "invalid", value));

  // Invalid - overflow
  m["overflow"] = "2147483648";
  EXPECT_FALSE(getInt(m, "overflow", value));

  // Invalid - underflow
  m["underflow"] = "-2147483649";
  EXPECT_FALSE(getInt(m, "underflow", value));

  // Invalid - empty string
  m["empty"] = "";
  EXPECT_FALSE(getInt(m, "empty", value));

  // Invalid - missing field
  EXPECT_FALSE(getInt(m, "missing", value));
}

// Enhanced tests for getInt64 - Added for from_chars migration
TEST_F(StringsHelpersTester, getInt64_EdgeCases) {
  using namespace helpers;
  StringStringMap m;
  int64_t value = 0;

  // Max int64
  m["max"] = "9223372036854775807";
  EXPECT_TRUE(getInt64(m, "max", value));
  EXPECT_EQ(value, 9223372036854775807LL);

  // Min int64
  m["min"] = "-9223372036854775808";
  EXPECT_TRUE(getInt64(m, "min", value));
  EXPECT_EQ(value, (-9223372036854775807LL - 1));

  // Partial parse - should fail with from_chars (more restrictive)
  m["partial"] = "123abc";
  EXPECT_FALSE(getInt64(m, "partial", value));

  // Invalid - non-numeric
  m["invalid"] = "abc";
  EXPECT_FALSE(getInt64(m, "invalid", value));

  // Invalid - overflow
  m["overflow"] = "9223372036854775808";
  EXPECT_FALSE(getInt64(m, "overflow", value));

  // Invalid - underflow
  m["underflow"] = "-9223372036854775809";
  EXPECT_FALSE(getInt64(m, "underflow", value));

  // Invalid - empty string
  m["empty"] = "";
  EXPECT_FALSE(getInt64(m, "empty", value));

  // Invalid - missing field
  EXPECT_FALSE(getInt64(m, "missing", value));
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

TEST_F(StringsHelpersTester, parseNextUInt32) {
  using namespace vrs::helpers;
  uint32_t outInt = 0;
  const char* strInt = "123";
  parseNextUInt32(strInt, outInt);
  EXPECT_EQ(outInt, 123);

  const char* strWord = "vrs";
  EXPECT_EQ(parseNextUInt32(strWord, outInt), false);
}

TEST_F(StringsHelpersTester, parseNextUInt32_EdgeCases) {
  using namespace vrs::helpers;
  uint32_t value = 0;

  // Partial parse - should stop at first non-digit
  const char* readFrom = "123abc";
  EXPECT_TRUE(parseNextUInt32(readFrom, value));
  EXPECT_EQ(value, 123);
  EXPECT_EQ(string(readFrom), "abc");

  // Max uint32 value
  readFrom = "4294967295";
  EXPECT_TRUE(parseNextUInt32(readFrom, value));
  EXPECT_EQ(value, 4294967295U);

  // Empty string
  readFrom = "";
  EXPECT_FALSE(parseNextUInt32(readFrom, value));

  // Leading zeros
  readFrom = "000123";
  EXPECT_TRUE(parseNextUInt32(readFrom, value));
  EXPECT_EQ(value, 123);

  // Zero
  readFrom = "0";
  EXPECT_TRUE(parseNextUInt32(readFrom, value));
  EXPECT_EQ(value, 0);

  // Overflow - exceeds uint32 max
  readFrom = "4294967296";
  EXPECT_FALSE(parseNextUInt32(readFrom, value));
}

TEST_F(StringsHelpersTester, readBool) {
  using namespace vrs::helpers;
  bool value = false;

  // True values
  EXPECT_TRUE(readBool("1", value));
  EXPECT_TRUE(value);

  EXPECT_TRUE(readBool("true", value));
  EXPECT_TRUE(value);

  EXPECT_TRUE(readBool("TRUE", value));
  EXPECT_TRUE(value);

  EXPECT_TRUE(readBool("True", value));
  EXPECT_TRUE(value);

  EXPECT_TRUE(readBool("yes", value));
  EXPECT_TRUE(value);

  EXPECT_TRUE(readBool("on", value));
  EXPECT_TRUE(value);

  EXPECT_TRUE(readBool("anything", value));
  EXPECT_TRUE(value);

  // False values - "0"
  EXPECT_TRUE(readBool("0", value));
  EXPECT_FALSE(value);

  // False values - "false" variations
  EXPECT_TRUE(readBool("false", value));
  EXPECT_FALSE(value);

  EXPECT_TRUE(readBool("FALSE", value));
  EXPECT_FALSE(value);

  EXPECT_TRUE(readBool("False", value));
  EXPECT_FALSE(value);

  // False values - "off" variations
  EXPECT_TRUE(readBool("off", value));
  EXPECT_FALSE(value);

  EXPECT_TRUE(readBool("OFF", value));
  EXPECT_FALSE(value);

  EXPECT_TRUE(readBool("Off", value));
  EXPECT_FALSE(value);

  // False values - "no" variations
  EXPECT_TRUE(readBool("no", value));
  EXPECT_FALSE(value);

  EXPECT_TRUE(readBool("NO", value));
  EXPECT_FALSE(value);

  EXPECT_TRUE(readBool("No", value));
  EXPECT_FALSE(value);

  // Strings that start with false values but don't match exactly should be true
  EXPECT_TRUE(readBool("falsely", value)); // doesn't match "false" exactly
  EXPECT_TRUE(value);

  EXPECT_TRUE(readBool("offline", value)); // doesn't match "off" exactly
  EXPECT_TRUE(value);

  EXPECT_TRUE(readBool("nope", value)); // doesn't match "no" exactly
  EXPECT_TRUE(value);

  EXPECT_TRUE(readBool("none", value)); // doesn't match "no" exactly
  EXPECT_TRUE(value);

  // Empty string returns false (no value set)
  value = true;
  EXPECT_FALSE(readBool("", value));
  EXPECT_TRUE(value); // value unchanged

  // Test with string_view from substring
  string original = "enabled=1";
  string_view sv = original;
  sv = sv.substr(8); // "1"
  EXPECT_TRUE(readBool(sv, value));
  EXPECT_TRUE(value);
}

// Enhanced tests for getBool
TEST_F(StringsHelpersTester, getBool_EdgeCases) {
  using namespace helpers;
  StringStringMap m;
  bool value = false;

  // Various true values
  m["true1"] = "1";
  EXPECT_TRUE(getBool(m, "true1", value));
  EXPECT_TRUE(value);

  m["true_yes"] = "yes";
  EXPECT_TRUE(getBool(m, "true_yes", value));
  EXPECT_TRUE(value);

  m["true_anything"] = "anything";
  EXPECT_TRUE(getBool(m, "true_anything", value));
  EXPECT_TRUE(value);

  // Various false values
  m["false_0"] = "0";
  EXPECT_TRUE(getBool(m, "false_0", value));
  EXPECT_FALSE(value);

  m["false_str"] = "false";
  EXPECT_TRUE(getBool(m, "false_str", value));
  EXPECT_FALSE(value);

  m["false_upper"] = "FALSE";
  EXPECT_TRUE(getBool(m, "false_upper", value));
  EXPECT_FALSE(value);

  m["false_mixed"] = "FaLsE";
  EXPECT_TRUE(getBool(m, "false_mixed", value));
  EXPECT_FALSE(value);

  // Empty value should return false
  m["empty"] = "";
  EXPECT_FALSE(getBool(m, "empty", value));

  // Missing key should return false
  EXPECT_FALSE(getBool(m, "missing", value));

  // Heterogeneous lookup with string_view
  string_view sv = "true1";
  EXPECT_TRUE(getBool(m, sv, value));
  EXPECT_TRUE(value);
}

TEST_F(StringsHelpersTester, readUInt64) {
  using namespace vrs::helpers;
  uint64_t value = 0;
  EXPECT_TRUE(readUInt64("123", value));
  EXPECT_EQ(value, 123);
  EXPECT_TRUE(readUInt64("18446744073709551615", value));
  EXPECT_EQ(value, 18446744073709551615ULL);
  EXPECT_FALSE(readUInt64("18446744073709551616", value)); // bigger than max uint64_t
  EXPECT_EQ(value, 0);

  EXPECT_FALSE(readUInt64("-1", value));
  EXPECT_FALSE(readUInt64("123vrs", value));
  EXPECT_FALSE(readUInt64("vrs", value));
}

TEST_F(StringsHelpersTester, readByteSize) {
  using namespace vrs::helpers;
  uint64_t value = 0;

  // Plain numbers
  EXPECT_TRUE(readByteSize("0", value));
  EXPECT_EQ(value, 0);

  EXPECT_TRUE(readByteSize("1234", value));
  EXPECT_EQ(value, 1234);

  EXPECT_TRUE(readByteSize("999999999", value));
  EXPECT_EQ(value, 999999999);

  // Byte suffix
  EXPECT_TRUE(readByteSize("100B", value));
  EXPECT_EQ(value, 100);

  EXPECT_TRUE(readByteSize("100b", value));
  EXPECT_EQ(value, 100);

  // Kilobyte
  EXPECT_TRUE(readByteSize("1KB", value));
  EXPECT_EQ(value, 1024);

  EXPECT_TRUE(readByteSize("1kb", value));
  EXPECT_EQ(value, 1024);

  EXPECT_TRUE(readByteSize("10KB", value));
  EXPECT_EQ(value, 10 * 1024);

  // Megabyte
  EXPECT_TRUE(readByteSize("1MB", value));
  EXPECT_EQ(value, 1024ULL * 1024);

  EXPECT_TRUE(readByteSize("1mb", value));
  EXPECT_EQ(value, 1024ULL * 1024);

  EXPECT_TRUE(readByteSize("256MB", value));
  EXPECT_EQ(value, 256ULL * 1024 * 1024);

  // Gigabyte
  EXPECT_TRUE(readByteSize("1GB", value));
  EXPECT_EQ(value, 1024ULL * 1024 * 1024);

  EXPECT_TRUE(readByteSize("1gb", value));
  EXPECT_EQ(value, 1024ULL * 1024 * 1024);

  EXPECT_TRUE(readByteSize("8GB", value));
  EXPECT_EQ(value, 8ULL * 1024 * 1024 * 1024);

  // Terabyte
  EXPECT_TRUE(readByteSize("1TB", value));
  EXPECT_EQ(value, 1024ULL * 1024 * 1024 * 1024);

  EXPECT_TRUE(readByteSize("1tb", value));
  EXPECT_EQ(value, 1024ULL * 1024 * 1024 * 1024);

  // Exabyte
  EXPECT_TRUE(readByteSize("1EB", value));
  EXPECT_EQ(value, 1024ULL * 1024 * 1024 * 1024 * 1024);

  EXPECT_TRUE(readByteSize("5EB", value));
  EXPECT_EQ(value, 5ULL * 1024 * 1024 * 1024 * 1024 * 1024);

  // Invalid inputs
  EXPECT_FALSE(readByteSize("", value));
  EXPECT_EQ(value, 0);

  EXPECT_FALSE(readByteSize("abc", value));
  EXPECT_EQ(value, 0);

  EXPECT_FALSE(readByteSize("-1KB", value));
  EXPECT_EQ(value, 0);

  // Test with string_view
  string original = "size=512MB";
  string_view sv = original;
  sv = sv.substr(5); // "512MB"
  EXPECT_TRUE(readByteSize(sv, value));
  EXPECT_EQ(value, 512ULL * 1024 * 1024);
}

TEST_F(StringsHelpersTester, readInt) {
  using namespace vrs::helpers;
  int value = 0;

  // Valid positive
  EXPECT_TRUE(readInt("123", value));
  EXPECT_EQ(value, 123);

  // Valid negative
  EXPECT_TRUE(readInt("-456", value));
  EXPECT_EQ(value, -456);

  // Valid zero
  EXPECT_TRUE(readInt("0", value));
  EXPECT_EQ(value, 0);

  // Max int32
  EXPECT_TRUE(readInt("2147483647", value));
  EXPECT_EQ(value, 2147483647);

  // Min int32
  EXPECT_TRUE(readInt("-2147483648", value));
  EXPECT_EQ(value, -2147483648);

  // Invalid - overflow
  EXPECT_FALSE(readInt("2147483648", value));

  // Invalid - underflow
  EXPECT_FALSE(readInt("-2147483649", value));

  // Invalid - partial parse (trailing chars)
  EXPECT_FALSE(readInt("123abc", value));

  // Invalid - non-numeric
  EXPECT_FALSE(readInt("abc", value));

  // Invalid - empty string
  EXPECT_FALSE(readInt("", value));

  // Invalid - whitespace only
  EXPECT_FALSE(readInt("   ", value));

  // Invalid - leading whitespace
  EXPECT_FALSE(readInt(" 123", value));

  // Test with string_view from substring
  string original = "value=42";
  string_view sv = original;
  sv = sv.substr(6); // "42"
  EXPECT_TRUE(readInt(sv, value));
  EXPECT_EQ(value, 42);
}

TEST_F(StringsHelpersTester, readInt64) {
  using namespace vrs::helpers;
  int64_t value = 0;

  // Valid positive
  EXPECT_TRUE(readInt64("123", value));
  EXPECT_EQ(value, 123);

  // Valid negative
  EXPECT_TRUE(readInt64("-456", value));
  EXPECT_EQ(value, -456);

  // Valid zero
  EXPECT_TRUE(readInt64("0", value));
  EXPECT_EQ(value, 0);

  // Max int64
  EXPECT_TRUE(readInt64("9223372036854775807", value));
  EXPECT_EQ(value, 9223372036854775807LL);

  // Min int64
  EXPECT_TRUE(readInt64("-9223372036854775808", value));
  EXPECT_EQ(value, (-9223372036854775807LL - 1));

  // Invalid - overflow
  EXPECT_FALSE(readInt64("9223372036854775808", value));

  // Invalid - underflow
  EXPECT_FALSE(readInt64("-9223372036854775809", value));

  // Invalid - partial parse (trailing chars)
  EXPECT_FALSE(readInt64("123abc", value));

  // Invalid - non-numeric
  EXPECT_FALSE(readInt64("abc", value));

  // Invalid - empty string
  EXPECT_FALSE(readInt64("", value));

  // Test with string_view from substring
  string original = "offset=-9876543210";
  string_view sv = original;
  sv = sv.substr(7); // "-9876543210"
  EXPECT_TRUE(readInt64(sv, value));
  EXPECT_EQ(value, -9876543210LL);
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
  helpers::split(str, '_', actualTokens);
  EXPECT_EQ(actualTokens, expectedTokens);

  str = "hello elle is cool lol. le bol de lait";
  expectedTokens = {"he", "", "o e", "", "e is coo", " ", "o", ". ", "e bo", " de ", "ait"};
  helpers::split(str, 'l', actualTokens);
  EXPECT_EQ(actualTokens, expectedTokens);

  expectedTokens = {"he", "o e", "e is coo", " ", "o", ". ", "e bo", " de ", "ait"};
  helpers::split(str, 'l', actualTokens, true);
  EXPECT_EQ(actualTokens, expectedTokens);

  expectedTokens = {"he", "o e", "e is coo", "o", ".", "e bo", "de", "ait"};
  helpers::split(str, 'l', actualTokens, true, " ");
  EXPECT_EQ(actualTokens, expectedTokens);
}

TEST_F(StringsHelpersTester, splitViewsTest) {
  using namespace vrs::helpers;

  // Basic test - same behavior as split()
  string str =
      "Lorem ipsum dolor sit amet, consectetur adipiscing elit, "
      "sed do eiusmod tempor incididunt ut labore et dolore magna aliqua.";
  vector<string_view> expectedTokens{
      "Lorem ipsum dolor sit amet",
      " consectetur adipiscing elit",
      " sed do eiusmod tempor incididunt ut labore et dolore magna aliqua."};
  vector<string_view> actualTokens;

  EXPECT_EQ(splitViews(str, ',', actualTokens), 3);
  EXPECT_EQ(actualTokens, expectedTokens);

  // No match - entire string as single token
  expectedTokens = {str};
  EXPECT_EQ(splitViews(str, '_', actualTokens), 1);
  EXPECT_EQ(actualTokens, expectedTokens);

  // Multiple delimiters with empty tokens
  str = "hello elle is cool lol. le bol de lait";
  expectedTokens = {"he", "", "o e", "", "e is coo", " ", "o", ". ", "e bo", " de ", "ait"};
  EXPECT_EQ(splitViews(str, 'l', actualTokens), 11);
  EXPECT_EQ(actualTokens, expectedTokens);

  // Skip empty tokens
  expectedTokens = {"he", "o e", "e is coo", " ", "o", ". ", "e bo", " de ", "ait"};
  EXPECT_EQ(splitViews(str, 'l', actualTokens, true), 9);
  EXPECT_EQ(actualTokens, expectedTokens);

  // Skip empty + trim whitespace
  expectedTokens = {"he", "o e", "e is coo", "o", ".", "e bo", "de", "ait"};
  EXPECT_EQ(splitViews(str, 'l', actualTokens, true, " "), 8);
  EXPECT_EQ(actualTokens, expectedTokens);

  // Test that views point to the original string (zero allocation verification)
  string original = "one,two,three";
  splitViews(original, ',', actualTokens);
  EXPECT_EQ(actualTokens.size(), 3);
  EXPECT_EQ(actualTokens[0], "one");
  EXPECT_EQ(actualTokens[1], "two");
  EXPECT_EQ(actualTokens[2], "three");
  // Verify data pointers reference the original string
  EXPECT_EQ(actualTokens[0].data(), original.data());
  EXPECT_EQ(actualTokens[1].data(), original.data() + 4);
  EXPECT_EQ(actualTokens[2].data(), original.data() + 8);

  // Empty string
  EXPECT_EQ(splitViews("", ',', actualTokens), 1);
  EXPECT_EQ(actualTokens.size(), 1);
  EXPECT_EQ(actualTokens[0], "");

  // Empty string with skipEmpty
  EXPECT_EQ(splitViews("", ',', actualTokens, true), 0);
  EXPECT_EQ(actualTokens.size(), 0);

  // Single delimiter
  EXPECT_EQ(splitViews(",", ',', actualTokens), 2);
  EXPECT_EQ(actualTokens.size(), 2);
  EXPECT_EQ(actualTokens[0], "");
  EXPECT_EQ(actualTokens[1], "");

  // Single delimiter with skipEmpty
  EXPECT_EQ(splitViews(",", ',', actualTokens, true), 0);
  EXPECT_EQ(actualTokens.size(), 0);

  // Multiple delimiters
  EXPECT_EQ(splitViews(",,,", ',', actualTokens), 4);
  EXPECT_EQ(actualTokens.size(), 4);

  // Delimiter at start
  EXPECT_EQ(splitViews(",abc", ',', actualTokens), 2);
  EXPECT_EQ(actualTokens[0], "");
  EXPECT_EQ(actualTokens[1], "abc");

  // Delimiter at end
  EXPECT_EQ(splitViews("abc,", ',', actualTokens), 2);
  EXPECT_EQ(actualTokens[0], "abc");
  EXPECT_EQ(actualTokens[1], "");

  // Test with string_view input directly
  string_view sv = "a:b:c";
  EXPECT_EQ(splitViews(sv, ':', actualTokens), 3);
  EXPECT_EQ(actualTokens[0], "a");
  EXPECT_EQ(actualTokens[1], "b");
  EXPECT_EQ(actualTokens[2], "c");

  // Test trimming with different characters
  EXPECT_EQ(splitViews(" a , b , c ", ',', actualTokens, false, " "), 3);
  EXPECT_EQ(actualTokens[0], "a");
  EXPECT_EQ(actualTokens[1], "b");
  EXPECT_EQ(actualTokens[2], "c");

  // Test that clears previous output
  actualTokens = {"old", "values"};
  splitViews("new", ',', actualTokens);
  EXPECT_EQ(actualTokens.size(), 1);
  EXPECT_EQ(actualTokens[0], "new");
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
