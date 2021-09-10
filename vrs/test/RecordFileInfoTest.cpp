//  Facebook Technologies, LLC Proprietary and Confidential.

#include <random>

#include <gtest/gtest.h>

#include <vrs/RecordFileInfo.h>
#include <vrs/helpers/EnumStringConverter.h>

using namespace std;
using namespace vrs;

namespace {
struct RecordFileInfoTester : testing::Test {};
} // namespace

TEST_F(RecordFileInfoTester, MakePrintable) {
  using namespace RecordFileInfo;
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

TEST_F(RecordFileInfoTester, HumanReadableDurationTest) {
  using namespace vrs::RecordFileInfo;
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
