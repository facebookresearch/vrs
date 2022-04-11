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

#include "ImageExtractor.h"

#include <fstream>
#include <iostream>

#include <fmt/format.h>

#define DEFAULT_LOG_CHANNEL "ImageExtractor"
#include <logging/Log.h>
#include <logging/Verify.h>

using namespace std;
using namespace vrs;

using utils::PixelFrame;

namespace {

const bool kSupportGrey16Export = true;

void writePngImage(
    PixelFrame& image,
    const string& folderPath,
    StreamId id,
    uint32_t imageCounter,
    double timeStamp) {
  string path = fmt::format(
      "{}/{}-{:05}-{:.3f}.png", folderPath, id.getNumericName(), imageCounter, timeStamp);
  cout << "Writing " << path << endl;
  image.writeAsPng(path);
}

bool writeImage(
    const ImageContentBlockSpec& imageContectBlockSpec,
    const vector<uint8_t>& imageData,
    const string& folderPath,
    StreamId id,
    uint32_t imageCounter,
    double timeStamp) {
  const auto& imageFormat = imageContectBlockSpec.getImageFormat();
  if (!XR_VERIFY(
          imageFormat == ImageFormat::JPG || imageFormat == ImageFormat::PNG ||
          imageFormat == ImageFormat::RAW || imageFormat == ImageFormat::VIDEO)) {
    return false; // unsupported formats
  }
  string filenamePostfix;
  string extension;
  switch (imageFormat) {
    case ImageFormat::JPG: {
      extension = ".jpg";
      break;
    }
    case ImageFormat::PNG: {
      extension = ".png";
      break;
    }
    case ImageFormat::RAW: {
      extension = ".raw";

      // encode image format into filename
      string pixelFormat = imageContectBlockSpec.getPixelFormatAsString();
      uint32_t width = imageContectBlockSpec.getWidth();
      uint32_t height = imageContectBlockSpec.getHeight();
      uint32_t rawstride = imageContectBlockSpec.getRawStride();

      filenamePostfix = "-" + pixelFormat + "-" + to_string(width) + "x" + to_string(height);
      if (rawstride > 0) {
        filenamePostfix += "-stride_" + to_string(rawstride);
      }
      break;
    }
    case ImageFormat::VIDEO: {
      extension = "." + imageContectBlockSpec.getCodecName();
      filenamePostfix += "#" + to_string(imageContectBlockSpec.getKeyFrameIndex());
      break;
    }
    default:
      filenamePostfix.clear();
      extension.clear();
  }

  string path = fmt::format(
      "{}/{}-{:05}-{:.3f}{}{}",
      folderPath,
      id.getNumericName(),
      imageCounter,
      timeStamp,
      filenamePostfix,
      extension);

  ofstream file(path, ios::binary);
  if (!file.is_open()) {
    XR_LOGE("Cannot open file {} for writing", path);
    return false;
  }
  cout << "Writing " << path << endl;
  if (!file.write((const char*)imageData.data(), imageData.size())) {
    XR_LOGE("Failed to write file {}", path);
    return false;
  }
  file.close();
  return true;
}

} // namespace

namespace vrs {
namespace utils {

bool ImageExtractor::onImageRead(const CurrentRecord& record, size_t, const ContentBlock& ib) {
  imageFileCounter_++;
  imageCounter_++;
  const StreamId id = record.streamId;
  auto format = ib.image().getImageFormat();

  if (format == ImageFormat::JPG || format == ImageFormat::PNG ||
      (extractImagesRaw_ && (format == ImageFormat::RAW || format == ImageFormat::VIDEO))) {
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
    if (writeImage(ib.image(), imageData, folderPath_, id, imageCounter_, record.timestamp)) {
      return true;
    }
  } else if (format == ImageFormat::RAW) {
    if (PixelFrame::readRawFrame(inputFrame_, record.reader, ib.image())) {
      PixelFrame::normalizeFrame(inputFrame_, processedFrame_, kSupportGrey16Export);
      writePngImage(*processedFrame_, folderPath_, id, imageCounter_, record.timestamp);
      return true;
    }
  } else if (format == ImageFormat::VIDEO) {
    if (!inputFrame_) {
      inputFrame_ = make_shared<PixelFrame>(ib.image());
    }
    if (tryToDecodeFrame(*inputFrame_, record, ib) == 0) {
      PixelFrame::normalizeFrame(inputFrame_, processedFrame_, kSupportGrey16Export);
      writePngImage(*processedFrame_, folderPath_, id, imageCounter_, record.timestamp);
      return true;
    }
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
