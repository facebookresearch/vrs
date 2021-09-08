// Facebook Technologies, LLC Proprietary and Confidential.

#include <gtest/gtest.h>

#include <vrs/utils/Strings.h>

struct StringUtilsTester : testing::Test {};

using namespace vrs::utils;

TEST_F(StringUtilsTester, strcasecmpTest) {
  EXPECT_EQ(str::strcasecmp("hello", "Hello"), 0);
  EXPECT_EQ(str::strcasecmp("hello", "HELLO"), 0);
  EXPECT_EQ(str::strcasecmp("hellO", "HELLO"), 0);
  EXPECT_GT(str::strcasecmp("hello", "bye"), 0);
  EXPECT_LT(str::strcasecmp("bye", "hello"), 0);
}

TEST_F(StringUtilsTester, strncasecmpTest) {
  EXPECT_EQ(str::strncasecmp("hello New-York", "Hello Paris", 6), 0);
  EXPECT_LT(str::strncasecmp("hello New-York", "Hello Paris", 7), 0);
  EXPECT_GT(str::strncasecmp("hello New-York", "Hello ", 7), 0);
}

TEST_F(StringUtilsTester, trim) {
  EXPECT_STREQ(str::trim("").c_str(), "");
  EXPECT_STREQ(str::trim(" ").c_str(), "");
  EXPECT_STREQ(str::trim("\t").c_str(), "");
  EXPECT_STREQ(str::trim(" \t ").c_str(), "");
  EXPECT_STREQ(str::trim(" he l\tlo ").c_str(), "he l\tlo");
  EXPECT_STREQ(str::trim(" hello").c_str(), "hello");
  EXPECT_STREQ(str::trim("hello ").c_str(), "hello");
  EXPECT_STREQ(str::trim("hello\t").c_str(), "hello");

  EXPECT_STREQ(str::trim(" hello ", " ").c_str(), "hello");
  EXPECT_STREQ(str::trim(" hello ", "").c_str(), " hello ");
  EXPECT_STREQ(str::trim("hello\r", " \t\n\r").c_str(), "hello");
  EXPECT_STREQ(str::trim("\n", " \t\n\r").c_str(), "");
  EXPECT_STREQ(str::trim(" ", " \t\n\r").c_str(), "");
  EXPECT_STREQ(str::trim("\t", " \t\n\r").c_str(), "");
  EXPECT_STREQ(str::trim("\n", " \t\n\r").c_str(), "");
  EXPECT_STREQ(str::trim("\r", " \t\n\r").c_str(), "");
  EXPECT_STREQ(str::trim("\rhello \t\n\rhello", " \t\n\r").c_str(), "hello \t\n\rhello");
}

TEST_F(StringUtilsTester, startsWith) {
  EXPECT_TRUE(str::startsWith("hello", ""));
  EXPECT_TRUE(str::startsWith("hello", "h"));
  EXPECT_TRUE(str::startsWith("hello", "he"));
  EXPECT_TRUE(str::startsWith("hello", "hel"));
  EXPECT_TRUE(str::startsWith("hello", "hell"));
  EXPECT_TRUE(str::startsWith("hello", "hello"));
  EXPECT_FALSE(str::startsWith("hello", "helloo"));
  EXPECT_TRUE(str::startsWith("hello", "H"));
  EXPECT_TRUE(str::startsWith("hello", "hE"));
  EXPECT_TRUE(str::startsWith("hello", "hEl"));
  EXPECT_TRUE(str::startsWith("hello", "HELL"));
  EXPECT_TRUE(str::startsWith("hello", "HELLo"));
  EXPECT_TRUE(str::startsWith("hello", "HELLO"));
  EXPECT_TRUE(str::startsWith("", ""));
  EXPECT_FALSE(str::startsWith("", "a"));
  EXPECT_FALSE(str::startsWith("ba", "a"));
}

TEST_F(StringUtilsTester, endsWith) {
  EXPECT_TRUE(str::endsWith("hello", ""));
  EXPECT_TRUE(str::endsWith("hello", "o"));
  EXPECT_TRUE(str::endsWith("hello", "lo"));
  EXPECT_TRUE(str::endsWith("hello", "llo"));
  EXPECT_TRUE(str::endsWith("hello", "ello"));
  EXPECT_TRUE(str::endsWith("hello", "hello"));
  EXPECT_FALSE(str::endsWith("hello", "hhello"));
  EXPECT_TRUE(str::endsWith("hello", "O"));
  EXPECT_TRUE(str::endsWith("hello", "LO"));
  EXPECT_TRUE(str::endsWith("hello", "LLO"));
  EXPECT_TRUE(str::endsWith("hello", "ELlO"));
  EXPECT_TRUE(str::endsWith("hello", "HElLO"));
  EXPECT_TRUE(str::endsWith("", ""));
  EXPECT_FALSE(str::endsWith("", "a"));
  EXPECT_FALSE(str::endsWith("ba", "b"));
}
