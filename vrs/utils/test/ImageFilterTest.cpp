// Copyright (c) Facebook, Inc. and its affiliates. Confidential and proprietary.

#include <thread>

#include <gtest/gtest.h>

#include <TestDataDir/TestDataDir.h>

#include <test_helpers/GTestMacros.h>
#include <vrs/RecordFileReader.h>
#include <vrs/os/Utils.h>
#include <vrs/utils/ImageFilter.h>
#include <vrs/utils/Validation.h>

using namespace std;
using namespace vrs;
using namespace vrs::utils;

namespace {
class TestImageFilter : public ImageFilter {
 public:
  TestImageFilter(size_t threadCount) : threadCount_{threadCount} {}

  // This function specifies which image formats the filter can be applied to
  bool accept(const vrs::ImageContentBlockSpec& imageSpec) override {
    if (imageSpec.getPixelFormat() == vrs::PixelFormat::GREY8) {
      return true;
    }
    return false;
  }

  // This function is the image filter applied to each frame: replace with your own!
  void filter(
      const vrs::IndexRecord::RecordInfo& /*recordInfo*/,
      size_t /*blockIndex*/,
      const vrs::ContentBlock& contentBlock,
      const std::vector<uint8_t>& inputFrame,
      std::vector<uint8_t>& outputFrame) override {
    size_t blockSize = contentBlock.getBlockSize();

    vector<size_t> counter(256);
    for (auto v : inputFrame) {
      counter[v]++;
    }
    const size_t kBucketCount = 3;
    vector<uint8_t> mapping(256);
    size_t currentTotal = 0;
    const size_t totalCount = blockSize;
    uint8_t bucket = 0;
    uint8_t bucketValue = 0;
    size_t bucketLimit = (bucket + 1) * totalCount / kBucketCount;
    for (size_t k = 0; k < 256; k++) {
      mapping[k] = bucketValue;
      currentTotal += counter[k];
      while (currentTotal >= bucketLimit) {
        bucket++;
        bucketValue = bucket * 255 / (kBucketCount - 1);
        bucketLimit = (bucket + 1) * totalCount / kBucketCount;
      }
    }
    for (size_t i = 0; i < blockSize; i++) {
      outputFrame[i] = mapping[inputFrame[i]];
    }
  }

  size_t getThreadCount() override {
    return threadCount_;
  }

 private:
  size_t threadCount_;
};

void testImageFilter(size_t threadCount) {
  Recordable::resetNewInstanceIds();

  const std::string kSourceFile =
      string(coretech::getTestDataDir()) + "/VRS_Files/InsideOutCameraSync.vrs";
  const std::string outputFile =
      os::getTempFolder() + "filteredImagesTest-" + std::to_string(threadCount) + ".vrs";

  TestImageFilter imageFilter(threadCount);

  // Setup the file source
  // If you use the filtering capabilities of FilteredVRSFileReader, be aware that you'll only
  // save the parts of the streams you actually process:
  // - If you limit the time range, your output file will only include this time range.
  // - If you limit to certain streams, your output file will only include those streams.
  FilteredVRSFileReader filteredReader(kSourceFile);
  ASSERT_EQ(filteredReader.openFile(), 0);

  CopyOptions options(false); // override compression setting, threadings, chunking, etc.
  ThrottledWriter throttledWriter(options);

  // Run the filter
  int statusCode = filterImages(imageFilter, filteredReader, throttledWriter, outputFile, options);
  EXPECT_EQ(statusCode, 0);

  FilteredVRSFileReader outputReader(outputFile);
  ASSERT_EQ(outputReader.openFile(), 0);
  EXPECT_EQ(outputReader.reader.getStreams().size(), 6);
  EXPECT_EQ(outputReader.reader.getIndex().size(), 983);
  EXPECT_EQ(outputReader.reader.getTags().size(), 7);
  EXPECT_EQ(outputReader.reader.getStreams().size(), filteredReader.reader.getStreams().size());
  EXPECT_EQ(outputReader.reader.getIndex().size(), filteredReader.reader.getIndex().size());
  EXPECT_EQ(outputReader.reader.getTags().size(), filteredReader.reader.getTags().size());

  // Hard to prove the images have been filtered properly...
  // We have verified that the images were as expected, and we can now use a content checksum to
  // prove the filter worked as it used to.
  EXPECT_EQ(
      checkRecords(outputReader, options, CheckType::Checksums),
      "FileTags: ae58a91fc6c0afb5\n"
      "1014-1 VRS tags: 2d452eb6874dd79f\n"
      "1014-1 User tags: ef46db3751d8e999\n"
      "1014-1 Headers: d362c453bf1c9b10\n"
      "1014-1 Payload: e68f758aad67e744\n"
      "1015-1 VRS tags: 8fd1514a19bf94f1\n"
      "1015-1 User tags: ef46db3751d8e999\n"
      "1015-1 Headers: 852ad50348123eb8\n"
      "1015-1 Payload: 5888110820d1d6c4\n"
      "1016-1 VRS tags: a4f84fc3b38879c2\n"
      "1016-1 User tags: ef46db3751d8e999\n"
      "1016-1 Headers: 8801cb1e10f20e72\n"
      "1016-1 Payload: 050f81bd31fa6274\n"
      "1016-2 VRS tags: a4f84fc3b38879c2\n"
      "1016-2 User tags: ef46db3751d8e999\n"
      "1016-2 Headers: 5fd0bdc50bedd5da\n"
      "1016-2 Payload: 7a54b2ccb25b9d38\n"
      "1016-3 VRS tags: a4f84fc3b38879c2\n"
      "1016-3 User tags: ef46db3751d8e999\n"
      "1016-3 Headers: 87bccbeb07d1fe4f\n"
      "1016-3 Payload: 5446413ad2556a92\n"
      "1016-4 VRS tags: a4f84fc3b38879c2\n"
      "1016-4 User tags: ef46db3751d8e999\n"
      "1016-4 Headers: 37ad421c8a6a2fae\n"
      "1016-4 Payload: 58651c8a4c6f5371\n"
      "1ba31c1af162f554");

  remove(outputFile.c_str());
}
} // namespace

struct ImageFilterTest : testing::Test {
  ImageFilterTest() {}
};

TEST_F(ImageFilterTest, ANDROID_DISABLED(ImageFilterST)) {
  testImageFilter(1);
}

TEST_F(ImageFilterTest, ANDROID_DISABLED(ImageFilter2)) {
  testImageFilter(2);
}

TEST_F(ImageFilterTest, ANDROID_DISABLED(ImageFilterMT)) {
  unsigned hwt = thread::hardware_concurrency();
  if (hwt > 2) {
    testImageFilter(hwt);
  }
}
