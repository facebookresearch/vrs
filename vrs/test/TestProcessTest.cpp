// (c) Facebook, Inc. and its affiliates. Confidential and proprietary.

#include <gtest/gtest.h>

#include <vrs/test/helpers/TestProcess.h>

class TestedToolProcess : public vrs::test::TestProcess {
 public:
  TestedToolProcess() : TestProcess("testedtool") {}
};

TEST(TestProcessTest, RunTestedTool) {
  TestedToolProcess testedtool;
  EXPECT_TRUE(testedtool.start("0"));
  EXPECT_EQ(testedtool.runProcess(), 0);

  EXPECT_TRUE(testedtool.start("1"));
  EXPECT_EQ(testedtool.runProcess(), 1);
}
