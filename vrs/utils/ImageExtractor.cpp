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

#include <vrs/helpers/Throttler.h>

using namespace std;
using namespace vrs;
using namespace vrs::utils;

namespace vrs::utils {

string ImageNamer::namePngImage(StreamId id, uint32_t imageCounter, double timestamp) {
  return fmt::format("{}-{:05}-{:.3f}.png", id.getNumericName(), imageCounter, timestamp);
}

string ImageNamer::nameRawImage(
    const ImageContentBlockSpec& imageSpec,
    StreamId id,
    uint32_t imageCounter,
    double timestamp) {
  return fmt::format(
      "{}-{:05}-{:.3f}{}",
      id.getNumericName(),
      imageCounter,
      timestamp,
      getRawImageFormatAsString(imageSpec));
}

string ImageNamer::getRawImageFormatAsString(const ImageContentBlockSpec& imageSpec) {
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
  return fmt::format("{}.{}", filenamePostfix, extension);
}

} // namespace vrs::utils

namespace {

Throttler& getThrottler() {
  static Throttler sThrottler;
  return sThrottler;
}

const bool kSupportGrey16Export = true;

bool writeRawImage(const string& path, const vector<uint8_t>& imageData) {
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
  explicit ImageJob(const string&& path)
      : jobType{JobType::SaveToPng}, path{path}, frame{make_shared<PixelFrame>()} {}
  ImageJob() : jobType{EndQueue} {}

  void saveAsPng() {
    shared_ptr<PixelFrame> normalFrame;
    PixelFrame::normalizeFrame(frame, normalFrame, kSupportGrey16Export);
    fmt::print("Writing {}\n", path);
    normalFrame->writeAsPng(path);
  }

  JobType jobType;

  const string path;
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
    if (!threadPool_.empty()) {
      imageQueue_.sendJob(make_unique<ImageJob>());
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

ImageNamer& getDefaultImageNamer() {
  static ImageNamer sDefaultImageNamer;
  return sDefaultImageNamer;
}

} // namespace

namespace vrs::utils {

ImageExtractor::ImageExtractor(const string& folderPath, uint32_t& counter, bool extractImagesRaw)
    : ImageExtractor(getDefaultImageNamer(), folderPath, counter, extractImagesRaw) {}

ImageExtractor::ImageExtractor(
    ImageNamer& imageNamer,
    const string& folderPath,
    uint32_t& counter,
    bool extractImagesRaw)
    : imageNamer_{imageNamer},
      folderPath_{folderPath},
      imageFileCounter_{counter},
      extractImagesRaw_(extractImagesRaw) {
  ImageProcessor::get().startThreadPool();
}

ImageExtractor::~ImageExtractor() {
  ImageProcessor::get().endThreadPool();
}

bool ImageExtractor::onDataLayoutRead(const CurrentRecord& r, size_t idx, DataLayout& dl) {
  return imageNamer_.onDataLayoutRead(r, idx, dl, *this);
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

  if (!extractImagesRaw_ && (format == ImageFormat::RAW || format == ImageFormat::VIDEO)) {
    string filename = imageNamer_.namePngImage(id, imageCounter_, record.timestamp);
    unique_ptr<ImageJob> job = make_unique<ImageJob>(fmt::format("{}/{}", folderPath_, filename));
    if ((format == ImageFormat::RAW &&
         PixelFrame::readRawFrame(job->frame, record.reader, ib.image())) ||
        (format == ImageFormat::VIDEO && tryToDecodeFrame(*job->frame, record, ib) == 0)) {
      imageQueue.sendJob(std::move(job));
      return true;
    }
  } else {
    vector<uint8_t> imageData;
    imageData.resize(ib.getBlockSize());
    int readStatus = record.reader->read(imageData.data(), ib.getBlockSize());
    if (readStatus != 0) {
      THROTTLED_LOGW(
          record.fileReader,
          "{} - {} record @ {}: Failed read image data ({}).",
          id.getNumericName(),
          toString(record.recordType),
          record.timestamp,
          errorCodeToMessage(readStatus));
      return false;
    }
    string filename = imageNamer_.nameRawImage(ib.image(), id, imageCounter_, record.timestamp);
    string filepath = fmt::format("{}/{}", folderPath_, filename);
    writeRawImage(filepath, imageData);
    return true;
  }
  THROTTLED_LOGW(
      record.fileReader, "Could not convert image for {}, format: {}", id.getName(), ib.asString());
  return false;
}

bool ImageExtractor::onUnsupportedBlock(const CurrentRecord& rec, size_t, const ContentBlock& cb) {
  // the image was not decoded, probably because the image spec are incomplete
  if (cb.getContentType() == ContentType::IMAGE) {
    imageCounter_++;
    THROTTLED_LOGW(
        rec.fileReader, "Image skipped for {}, content: {}", rec.streamId.getName(), cb.asString());
  }
  return false;
}

} // namespace vrs::utils
