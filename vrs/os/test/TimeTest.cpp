// Facebook Technologies, LLC Proprietary and Confidential.

#include <memory>

#include <gtest/gtest.h>

#include <vrs/os/Time.h>

struct OsTimeTest : testing::Test {};

TEST_F(OsTimeTest, osTimeGetCurrentTimeSecTest) {
  // naive monotony test. Values should always grow.
  double lastTime = vrs::os::getTimestampSec();
  size_t zeroGapCount = 0;
  size_t negativeGapCount = 0; // should never ever happen!
  size_t positiveGapCount = 0;
  double minNonZeroGap = 1;
  double maxNonZeroGap = 0;
  const double kTestDurationSec = 1; // 1 sec
  double end = lastTime + kTestDurationSec;
  while (lastTime < end) {
    double now = vrs::os::getTimestampSec();
    double gap = now - lastTime;
    if (gap < 0) {
      negativeGapCount++;
    } else if (gap > 0) {
      positiveGapCount++;
      if (gap < minNonZeroGap) {
        minNonZeroGap = gap;
      }
      if (gap > maxNonZeroGap) {
        maxNonZeroGap = gap;
      }
    } else {
      zeroGapCount++; // we'll accept that for Windows... :-(
    }
    lastTime = now;
  }
  EXPECT_EQ(negativeGapCount, 0); // never OK to have a negative gap
  EXPECT_GT(positiveGapCount, 100);
  const double kRequiredMinNonZeroGapSec = 0.001; // 1 ms
  EXPECT_GT(kRequiredMinNonZeroGapSec, minNonZeroGap);
}

TEST_F(OsTimeTest, osTimeGetCurrentTimeSecSinceEpoch) {
  // merely make sure that the implementation isn't completely busted...
  int64_t now = vrs::os::getCurrentTimeSecSinceEpoch();
  const int64_t lastTestUpdateTime = 1590453570;
  const int64_t jan1_2030 = 1672560000;
  EXPECT_GT(now, lastTestUpdateTime);
  EXPECT_GT(jan1_2030, now);
}
