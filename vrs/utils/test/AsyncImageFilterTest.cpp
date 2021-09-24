// (c) Facebook, Inc. and its affiliates. Confidential and proprietary.

#include <gtest/gtest.h>

#include <TestDataDir/TestDataDir.h>

#include <test_helpers/GTestMacros.h>
#include <vrs/RecordFileReader.h>
#include <vrs/os/Utils.h>
#include <vrs/utils/AsyncImageFilter.h>
#include <vrs/utils/Validation.h>

using namespace std;
using namespace vrs;
using namespace vrs::utils;

namespace {

class FilterJob {
 public:
  FilterJob(
      size_t _recordIndex,
      StreamId _streamId,
      const ImageContentBlockSpec& _imageSpec,
      std::vector<uint8_t>&& _pixels)
      : recordIndex{_recordIndex},
        streamId{_streamId},
        imageSpec{_imageSpec},
        pixels{move(_pixels)} {}

  FilterJob(const FilterJob&) = delete;
  FilterJob& operator=(const FilterJob&) = delete;

  void performJob() {
    if (imageSpec.getBytesPerPixel() != 1) {
      return;
    }
    this_thread::sleep_for(chrono::milliseconds(rand() % 10)); // shuffle processing speed
    if ((streamId.getInstanceId() - 1) == 0) {
      // invert bw pixels first stream
      for (auto& p : pixels) {
        p = ~p;
      }
    }
    // vertical-flip odd streams
    if (((streamId.getInstanceId() - 1) & 1) != 0 && imageSpec.getWidth() > 1) {
      for (uint32_t h = 0; h < imageSpec.getHeight(); h++) {
        uint8_t* left = pixels.data() + h * imageSpec.getStride();
        uint8_t* right = left + imageSpec.getWidth() - 1;
        while (left < right) {
          uint8_t v = *left;
          *left++ = *right;
          *right-- = v;
        }
      }
    }
    // horizontal-flip stream with second lsb bit set
    if (((streamId.getInstanceId() - 1) & 2) != 0 && imageSpec.getHeight() > 1) {
      const size_t lineLength = imageSpec.getWidth();
      vector<uint8_t> line(lineLength);
      uint32_t top = 0;
      uint32_t bottom = imageSpec.getHeight() - 1;
      while (top < bottom) {
        uint8_t* topPixels = pixels.data() + top * imageSpec.getStride();
        uint8_t* bottomPixels = pixels.data() + bottom * imageSpec.getStride();
        memcpy(line.data(), topPixels, lineLength);
        memcpy(topPixels, bottomPixels, lineLength);
        memcpy(bottomPixels, line.data(), lineLength);
        top++, bottom--;
      }
    }
  }

  size_t recordIndex;
  StreamId streamId;
  ImageContentBlockSpec imageSpec;
  std::vector<uint8_t> pixels;
};

using FilterJobQueue = JobQueue<unique_ptr<FilterJob>>;

class CompressionWorker {
 public:
  CompressionWorker(FilterJobQueue& workQueue, FilterJobQueue& resultsQueue)
      : workQueue_{workQueue},
        resultsQueue_{resultsQueue},
        thread_(&CompressionWorker::threadActivity, this) {}
  ~CompressionWorker() {
    if (thread_.joinable()) {
      thread_.join();
    }
  }

 private:
  // Workers take a job from the work pile, "perform" the job, send the job to the results pile...
  void threadActivity() {
    while (!workQueue_.hasEnded()) {
      unique_ptr<FilterJob> job;
      if (workQueue_.waitForJob(job, 1 /* second */)) {
        job->performJob();
        resultsQueue_.sendJob(move(job));
      }
    }
  }

  FilterJobQueue& workQueue_;
  FilterJobQueue& resultsQueue_;
  thread thread_;
};

void testImageFilter(const size_t kWorkersCount) {
  Recordable::resetNewInstanceIds();

  const std::string kSourceFile =
      string(coretech::getTestDataDir()) + "/VRS_Files/ConstellationTelemetryMinimalSlam.vrs";
  const std::string outputFile =
      os::getTempFolder() + "AsyncImagesTest-" + std::to_string(kWorkersCount) + ".vrs";

  FilteredVRSFileReader filteredReader(kSourceFile);

  // Setup background processing workers
  FilterJobQueue workQueue, resultQueue;
  deque<CompressionWorker> workers;
  for (size_t workerIndex = 0; workerIndex < kWorkersCount; workerIndex++) {
    workers.emplace_back(workQueue, resultQueue);
  }

  AsyncImageFilter imageFilter(filteredReader);
  ASSERT_EQ(imageFilter.createOutputFile(outputFile), 0);
  bool allRead = false;
  size_t recordIndex;
  ImageContentBlockSpec imageSpec;
  std::vector<uint8_t> frame;
  unique_ptr<FilterJob> result;
  do {
    // fill the queue
    while (!allRead && imageFilter.getPendingCount() < 2 * kWorkersCount) {
      if (imageFilter.getNextImage(recordIndex, imageSpec, frame)) {
        workQueue.sendJob(make_unique<FilterJob>(
            recordIndex, imageFilter.getRecordInfo(recordIndex)->streamId, imageSpec, move(frame)));
      } else {
        allRead = true;
      }
    }
    // wait process results
    if (imageFilter.getPendingCount() > 0 && resultQueue.waitForJob(result, 1)) {
      imageFilter.writeProcessedImage(result->recordIndex, move(result->pixels));
      result.reset();
    }
  } while (!allRead || imageFilter.getPendingCount() > 0);

  // quit workers
  workQueue.endQueue();
  workers.clear();

  ASSERT_EQ(imageFilter.closeFile(), 0);

  FilteredVRSFileReader outputReader(outputFile);
  ASSERT_EQ(outputReader.openFile(), 0);
  EXPECT_EQ(outputReader.reader.getStreams().size(), 13);
  EXPECT_EQ(outputReader.reader.getIndex().size(), 911);
  EXPECT_EQ(outputReader.reader.getTags().size(), 6);
  EXPECT_EQ(outputReader.reader.getStreams().size(), filteredReader.reader.getStreams().size());
  EXPECT_EQ(outputReader.reader.getIndex().size(), filteredReader.reader.getIndex().size());
  EXPECT_EQ(outputReader.reader.getTags().size(), filteredReader.reader.getTags().size());

  // Hard to prove the images have been filtered properly...
  // We have visually verified that the images were as expected, and we can now use a content
  // checksum to prove the filter worked as it used to.
  EXPECT_EQ(checkRecords(outputReader, {false}, CheckType::Checksum), "e2a2d5467d9065a0");

  remove(outputFile.c_str());
}

} // namespace

struct AsyncImageFilterTest : testing::Test {
  AsyncImageFilterTest() {}
};

TEST_F(AsyncImageFilterTest, ANDROID_DISABLED(AsyncFilterTest)) {
  testImageFilter(12);
}
