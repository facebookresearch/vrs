// Facebook Technologies, LLC Proprietary and Confidential.

#include <gtest/gtest.h>

#include <vrs/test/helpers/OsTestProcess.h>

class TestedToolProcess : public vrs::os::OsTestProcess {
 public:
  TestedToolProcess() : OsTestProcess("testedtool") {}
};

TEST(TestProcessTest, RunTestedTool) {
  TestedToolProcess testedtool;
  EXPECT_TRUE(testedtool.start("0"));
  EXPECT_EQ(testedtool.runProcess(), 0);

  EXPECT_TRUE(testedtool.start("1"));
  EXPECT_EQ(testedtool.runProcess(), 1);
}
