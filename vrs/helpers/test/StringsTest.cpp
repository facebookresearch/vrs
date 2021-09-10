// Facebook Technologies, LLC Proprietary and Confidential.

#include <gtest/gtest.h>

#include <vrs/helpers/Strings.h>

struct StringsHelpersTester : testing::Test {};

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
  EXPECT_TRUE(helpers::startsWith("hello", ""));
  EXPECT_TRUE(helpers::startsWith("hello", "h"));
  EXPECT_TRUE(helpers::startsWith("hello", "he"));
  EXPECT_TRUE(helpers::startsWith("hello", "hel"));
  EXPECT_TRUE(helpers::startsWith("hello", "hell"));
  EXPECT_TRUE(helpers::startsWith("hello", "hello"));
  EXPECT_FALSE(helpers::startsWith("hello", "helloo"));
  EXPECT_TRUE(helpers::startsWith("hello", "H"));
  EXPECT_TRUE(helpers::startsWith("hello", "hE"));
  EXPECT_TRUE(helpers::startsWith("hello", "hEl"));
  EXPECT_TRUE(helpers::startsWith("hello", "HELL"));
  EXPECT_TRUE(helpers::startsWith("hello", "HELLo"));
  EXPECT_TRUE(helpers::startsWith("hello", "HELLO"));
  EXPECT_TRUE(helpers::startsWith("", ""));
  EXPECT_FALSE(helpers::startsWith("", "a"));
  EXPECT_FALSE(helpers::startsWith("ba", "a"));
}

TEST_F(StringsHelpersTester, endsWith) {
  EXPECT_TRUE(helpers::endsWith("hello", ""));
  EXPECT_TRUE(helpers::endsWith("hello", "o"));
  EXPECT_TRUE(helpers::endsWith("hello", "lo"));
  EXPECT_TRUE(helpers::endsWith("hello", "llo"));
  EXPECT_TRUE(helpers::endsWith("hello", "ello"));
  EXPECT_TRUE(helpers::endsWith("hello", "hello"));
  EXPECT_FALSE(helpers::endsWith("hello", "hhello"));
  EXPECT_TRUE(helpers::endsWith("hello", "O"));
  EXPECT_TRUE(helpers::endsWith("hello", "LO"));
  EXPECT_TRUE(helpers::endsWith("hello", "LLO"));
  EXPECT_TRUE(helpers::endsWith("hello", "ELlO"));
  EXPECT_TRUE(helpers::endsWith("hello", "HElLO"));
  EXPECT_TRUE(helpers::endsWith("", ""));
  EXPECT_FALSE(helpers::endsWith("", "a"));
  EXPECT_FALSE(helpers::endsWith("ba", "b"));
}
