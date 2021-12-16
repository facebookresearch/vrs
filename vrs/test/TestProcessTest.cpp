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
