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

#include "ImageExtractor.h"

#include <fstream>
#include <thread>

#include <fmt/format.h>

#define DEFAULT_LOG_CHANNEL "ImageExtractor"
#include <logging/Log.h>
#include <logging/Verify.h>

using namespace std;
using namespace vrs;
using namespace utils;

namespace {

const bool kSupportGrey16Export = true;

bool writeRawImage(
    const ImageContentBlockSpec& imageSpec,
    const vector<uint8_t>& imageData,
    const string& folderPath,
    StreamId id,
    uint32_t imageCounter,
    double timestamp) {
  const auto& imageFormat = imageSpec.getImageFormat();
  string filenamePostfix;
  string extension;
  switch (imageFormat) {
    case ImageFormat::RAW: {
      extension = "raw";

      // encode image format into filename
      string pixelFormat = imageSpec.getPixelFormatAsString();
      uint32_t width = imageSpec.getWidth();
      uint32_t height = imageSpec.getHeight();
      uint32_t rawstride = imageSpec.getRawStride();

      filenamePostfix = "-" + pixelFormat + "-" + to_string(width) + "x" + to_string(height);
      if (rawstride > 0) {
        filenamePostfix += "-stride_" + to_string(rawstride);
      }
      break;
    }
    case ImageFormat::VIDEO: {
      extension = imageSpec.getCodecName();
      filenamePostfix += "#" + to_string(imageSpec.getKeyFrameIndex());
      break;
    }
    default:
      extension = toString(imageFormat);
      break;
  }

  string path = fmt::format(
      "{}/{}-{:05}-{:.3f}{}.{}",
      folderPath,
      id.getNumericName(),
      imageCounter,
      timestamp,
      filenamePostfix,
      extension);

  ofstream file(path, ios::binary);
  if (!file.is_open()) {
    XR_LOGE("Cannot open file {} for writing", path);
    return false;
  }
  fmt::print("Writing {}\n", path);
  if (!file.write((const char*)imageData.data(), imageData.size())) {
    XR_LOGE("Failed to write file {}", path);
    return false;
  }
  file.close();
  return true;
}

enum JobType { SaveToPng, EndQueue };

struct ImageJob {
  ImageJob(const string& folderPath, StreamId id, double timestamp, uint32_t imageCounter)
      : jobType{JobType::SaveToPng},
        folderPath{folderPath},
        id{id},
        timestamp{timestamp},
        imageCounter{imageCounter},
        frame{make_shared<PixelFrame>()} {}
  ImageJob(JobType jobType, const string& folderPath) : jobType{jobType}, folderPath{folderPath} {}

  void saveAsPng() {
    shared_ptr<PixelFrame> normalFrame;
    PixelFrame::normalizeFrame(frame, normalFrame, kSupportGrey16Export);
    string path = fmt::format(
        "{}/{}-{:05}-{:.3f}.png", folderPath, id.getNumericName(), imageCounter, timestamp);
    fmt::print("Writing {}\n", path);
    normalFrame->writeAsPng(path);
  }

  JobType jobType;

  const string& folderPath;
  StreamId id;
  double timestamp;
  uint32_t imageCounter;
  shared_ptr<PixelFrame> frame;
};

class ImageProcessor {
 public:
  static ImageProcessor& get() {
    static ImageProcessor sImageProcessor;
    return sImageProcessor;
  }

  JobQueue<unique_ptr<ImageJob>>& getImageQueue() {
    return imageQueue_;
  }

  void startThreadPool() {
    unique_lock<mutex> locker(mutex_);
    while (threadPool_.size() < thread::hardware_concurrency()) {
      threadPool_.emplace_back(&ImageProcessor::saveImagesThreadActivity, this);
    }
  }

  void endThreadPool() {
    unique_lock<mutex> locker(mutex_);
    if (threadPool_.size() > 0) {
      JobQueue<unique_ptr<ImageJob>>& imageQueue = getImageQueue();
      string fakePath;
      imageQueue.sendJob(make_unique<ImageJob>(JobType::EndQueue, fakePath));
      for (auto& thread : threadPool_) {
        if (thread.joinable()) {
          thread.join();
        }
      }
      threadPool_.clear();
    }
  }

  void saveImagesThreadActivity() {
    unique_ptr<ImageJob> job;
    while (imageQueue_.waitForJob(job)) {
      switch (job->jobType) {
        case JobType::SaveToPng:
          job->saveAsPng();
          break;
        case JobType::EndQueue:
          imageQueue_.endQueue();
          break;
      }
      job.reset();
    }
  }

 private:
  mutex mutex_;
  deque<thread> threadPool_;
  JobQueue<unique_ptr<ImageJob>> imageQueue_;
};

} // namespace

namespace vrs {
namespace utils {

ImageExtractor::ImageExtractor(
    const string& folderPath,
    uint32_t& counter,
    const bool extractImagesRaw)
    : folderPath_{folderPath}, imageFileCounter_{counter}, extractImagesRaw_(extractImagesRaw) {
  ImageProcessor::get().startThreadPool();
}

ImageExtractor::~ImageExtractor() {
  ImageProcessor::get().endThreadPool();
}

bool ImageExtractor::onImageRead(const CurrentRecord& record, size_t, const ContentBlock& ib) {
  JobQueue<unique_ptr<ImageJob>>& imageQueue = ImageProcessor::get().getImageQueue();
  while (imageQueue.getQueueSize() > 2 * thread::hardware_concurrency()) {
    this_thread::sleep_for(chrono::milliseconds(50));
  }

  imageFileCounter_++;
  imageCounter_++;
  const StreamId id = record.streamId;
  auto format = ib.image().getImageFormat();

  if (!extractImagesRaw_ && format == ImageFormat::RAW) {
    unique_ptr<ImageJob> job =
        make_unique<ImageJob>(folderPath_, id, record.timestamp, imageCounter_);
    if (PixelFrame::readRawFrame(job->frame, record.reader, ib.image())) {
      imageQueue.sendJob(move(job));
      return true;
    }
  } else if (!extractImagesRaw_ && format == ImageFormat::VIDEO) {
    unique_ptr<ImageJob> job =
        make_unique<ImageJob>(folderPath_, id, record.timestamp, imageCounter_);
    if (tryToDecodeFrame(*job->frame, record, ib) == 0) {
      imageQueue.sendJob(move(job));
      return true;
    }
  } else {
    vector<uint8_t> imageData;
    imageData.resize(ib.getBlockSize());
    int readStatus = record.reader->read(imageData.data(), ib.getBlockSize());
    if (readStatus != 0) {
      XR_LOGW(
          "{} - {} record @ {}: Failed read image data ({}).",
          id.getNumericName(),
          toString(record.recordType),
          record.timestamp,
          errorCodeToMessage(readStatus));
      return false;
    }
    writeRawImage(ib.image(), imageData, folderPath_, id, imageCounter_, record.timestamp);
    return true;
  }
  XR_LOGW("Could not convert image for {}, format: {}", id.getName(), ib.asString());
  return false;
}

bool ImageExtractor::onUnsupportedBlock(
    const CurrentRecord& record,
    size_t,
    const ContentBlock& cb) {
  // the image was not decoded, probably because the image spec are incomplete
  if (cb.getContentType() == ContentType::IMAGE) {
    imageCounter_++;
    XR_LOGW("Image skipped for {}, content: {}", record.streamId.getName(), cb.asString());
  }
  return false;
}

} // namespace utils
} // namespace vrs
