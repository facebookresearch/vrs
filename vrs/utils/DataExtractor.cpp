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

#include "DataExtractor.h"

#include <fmt/format.h>

#define DEFAULT_LOG_CHANNEL "DataExtractor"
#include <logging/Log.h>
#include <logging/Verify.h>

#include <vrs/helpers/FileMacros.h>
#include <vrs/helpers/Rapidjson.hpp>
#include <vrs/os/Utils.h>
#include <vrs/utils/PixelFrame.h>
#include <vrs/utils/RecordFileInfo.h>

const char* kReadMeContent =
#include "DataExtractorReadMe.hpp"
    ;

namespace vrs::utils {

using namespace std;
using namespace vrs;

DataExtractor::DataExtractorStreamPlayer::DataExtractorStreamPlayer(
    ofstream& output,
    const string& outputFolder)
    : output_{output}, outputFolder_{outputFolder} {}

bool DataExtractor::DataExtractorStreamPlayer::writeImage(
    const CurrentRecord& record,
    const ImageContentBlockSpec& spec,
    const vector<uint8_t>& imageData) {
  const auto& imageFormat = spec.getImageFormat();
  if (!XR_VERIFY(
          imageFormat == vrs::ImageFormat::JPG || imageFormat == vrs::ImageFormat::PNG ||
          imageFormat == vrs::ImageFormat::RAW || imageFormat == vrs::ImageFormat::VIDEO)) {
    return false; // unsupported formats
  }
  string filenamePostfix;
  string extension;
  switch (imageFormat) {
    case vrs::ImageFormat::JPG: {
      extension = ".jpg";
      break;
    }
    case vrs::ImageFormat::PNG: {
      extension = ".png";
      break;
    }
    case vrs::ImageFormat::RAW: {
      extension = ".raw";

      // encode image format into filename
      string pixelFormat = spec.getPixelFormatAsString();
      uint32_t width = spec.getWidth();
      uint32_t height = spec.getHeight();
      uint32_t rawstride = spec.getRawStride();

      filenamePostfix = "-" + pixelFormat + "-" + to_string(width) + "x" + to_string(height);
      if (rawstride > 0) {
        filenamePostfix += "-stride_" + to_string(rawstride);
      }
      break;
    }
    case vrs::ImageFormat::VIDEO: {
      extension = "." + spec.getCodecName();
      filenamePostfix += "#" + to_string(spec.getKeyFrameIndex());
      break;
    }
    default:
      filenamePostfix.clear();
      extension.clear();
  }

  if (imageCounter_ <= 1) {
    os::makeDirectories(outputFolder_);
  }
  string filename = fmt::format(
      "{}-{:05}-{:.3f}{}{}",
      record.streamId.getNumericName(),
      imageCounter_,
      record.timestamp,
      filenamePostfix,
      extension);

  ofstream file(os::pathJoin(outputFolder_, filename), ios::binary);
  if (!file.is_open()) {
    XR_LOGE("Cannot open file {} for writing", filename);
    return false;
  }
  cout << "Writing " << filename << endl;
  if (!file.write(reinterpret_cast<const char*>(imageData.data()), imageData.size())) {
    XR_LOGE("Failed to write file {}", filename);
    return false;
  }
  file.close();
  wroteImage(filename);
  return true;
}

bool DataExtractor::DataExtractorStreamPlayer::onDataLayoutRead(
    const CurrentRecord& record,
    size_t blockIndex,
    DataLayout& dl) {
  JsonFormatProfileSpec profile(JsonFormatProfile::Public);
  profile.type = false;
  blocks_.emplace_back(dl.asJson(profile));
  return true;
}

bool DataExtractor::DataExtractorStreamPlayer::onImageRead(
    const vrs::CurrentRecord& record,
    size_t blockIndex,
    const vrs::ContentBlock& imageBlock) {
  imageCounter_++;
  auto format = imageBlock.image().getImageFormat();

  if (format == vrs::ImageFormat::JPG || format == vrs::ImageFormat::PNG) {
    vector<uint8_t> imageData;
    imageData.resize(imageBlock.getBlockSize());
    if (!XR_VERIFY(record.reader->read(imageData.data(), imageBlock.getBlockSize()) == 0)) {
      return false;
    }
    if (writeImage(record, imageBlock.image(), imageData)) {
      return true;
    }
  } else if (format == vrs::ImageFormat::RAW) {
    if (PixelFrame::readRawFrame(inputFrame_, record.reader, imageBlock.image())) {
      PixelFrame::normalizeFrame(inputFrame_, processedFrame_, true);
      writePngImage(record);
      return true;
    }
  } else if (format == vrs::ImageFormat::VIDEO) {
    if (!inputFrame_) {
      inputFrame_ = make_shared<PixelFrame>(imageBlock.image());
    }
    if (tryToDecodeFrame(*inputFrame_, record, imageBlock) == 0) {
      PixelFrame::normalizeFrame(inputFrame_, processedFrame_, true);
      writePngImage(record);
      return true;
    }
  }
  XR_LOGW(
      "Could not convert image for {}, format: {}",
      record.streamId.getName(),
      imageBlock.asString());
  return false;
}

bool DataExtractor::DataExtractorStreamPlayer::onCustomBlockRead(
    const CurrentRecord& record,
    size_t blockIndex,
    const ContentBlock& contentBlock) {
  size_t size = contentBlock.getBlockSize();
  vector<uint8_t> data(size);
  if (size > 0 && size != ContentBlock::kSizeUnknown) {
    XR_VERIFY(record.reader->read(data) == 0);
  }
  using namespace fb_rapidjson;
  JDocument doc;
  doc.SetObject();
  JValue custom;
  custom.SetObject();
  JsonWrapper json{custom, doc.GetAllocator()};
  json.addMember("size", static_cast<uint64_t>(size));
  doc.AddMember("custom", custom, doc.GetAllocator());
  blocks_.emplace_back(jDocumentToJsonString(doc));
  return true;
}

bool DataExtractor::DataExtractorStreamPlayer::onUnsupportedBlock(
    const CurrentRecord& record,
    size_t,
    const ContentBlock& contentBlock) {
  XR_LOGW(
      "Unsupported block: {} {} @ {:.6f}: {}, {}.",
      record.streamId.getNumericName(),
      toString(record.recordType),
      record.timestamp,
      contentBlock.asString(),
      (contentBlock.getBlockSize() != ContentBlock::kSizeUnknown)
          ? to_string(contentBlock.getBlockSize()) + " bytes"
          : "unknown size");
  JDocument doc;
  doc.SetObject();
  JsonWrapper json{doc, doc.GetAllocator()};
  json.addMember("unsupported_block", contentBlock.asString());
  blocks_.emplace_back(jDocumentToJsonString(doc));
  return true;
}

int DataExtractor::DataExtractorStreamPlayer::recordReadComplete(
    RecordFileReader& /*reader*/,
    const IndexRecord::RecordInfo& recordInfo) {
  using namespace fb_rapidjson;
  {
    JDocument doc;
    doc.SetObject();
    JsonWrapper json{doc, doc.GetAllocator()};
    json.addMember("stream", recordInfo.streamId.getNumericName());
    json.addMember("type", Record::typeName(recordInfo.recordType));
    json.addMember("timestamp", recordInfo.timestamp);
    output_ << jDocumentToJsonString(doc) << endl;
  }
  output_ << "{\"content\":[";
  bool first = true;
  for (const string& block : blocks_) {
    if (first) {
      first = false;
    } else {
      output_ << ',';
    }
    output_ << block;
  }
  output_ << "]}" << endl;
  blocks_.clear();

  return output_.fail() ? FAILURE : SUCCESS;
}

void DataExtractor::DataExtractorStreamPlayer::writePngImage(const CurrentRecord& record) {
  if (imageCounter_ <= 1) {
    os::makeDirectories(outputFolder_);
  }
  string filename = fmt::format(
      "{}-{:05}-{:.3f}.png", record.streamId.getNumericName(), imageCounter_, record.timestamp);
  cout << "Writing " << filename << endl;
  processedFrame_->writeAsPng(os::pathJoin(outputFolder_, filename));
  wroteImage(filename);
}

void DataExtractor::DataExtractorStreamPlayer::wroteImage(const string& filename) {
  blocks_.emplace_back(fmt::format("{{\"image\":\"{}\"}}", filename));
}

int DataExtractor::extractAll(const string& vrsFilePath, const string& outputFolder) {
  RecordFileReader reader;
  IF_ERROR_LOG_AND_RETURN(reader.openFile(vrsFilePath));
  DataExtractor extractor(reader, outputFolder);
  for (auto id : reader.getStreams()) {
    extractor.extract(id);
  }
  IF_ERROR_LOG_AND_RETURN(extractor.createOutput());
  IF_ERROR_LOG_AND_RETURN(reader.readAllRecords());
  return extractor.completeOutput();
}

DataExtractor::DataExtractor(RecordFileReader& reader, const string& outputFolder)
    : reader_{reader}, outputFolder_{outputFolder} {}

void DataExtractor::extract(StreamId id) {
  auto extractor = make_unique<DataExtractorStreamPlayer>(
      output_, os::pathJoin(outputFolder_, id.getNumericName()));
  reader_.setStreamPlayer(id, extractor.get());
  extractors_[id] = move(extractor);
}

int DataExtractor::createOutput() {
  if (!os::pathExists(outputFolder_)) {
    IF_ERROR_LOG_AND_RETURN(os::makeDirectories(outputFolder_));
  } else if (!os::isDir(outputFolder_)) {
    XR_LOGE("Can't output data at {}", outputFolder_);
    return FAILURE;
  }
  string readmePath = os::pathJoin(outputFolder_, "ReadMe.md");
  ofstream readme(readmePath, ofstream::out | ofstream::trunc);
  if (readme.fail()) {
    XR_LOGE("Couldn't create file {}", readmePath);
    return FAILURE;
  }

  RecordFileInfo::Details details =
      RecordFileInfo::Details::Everything | RecordFileInfo::Details::UsePublicNames;
  const auto ids = getStreams();
  readme << kReadMeContent << endl << "```" << endl;
  RecordFileInfo::printOverview(readme, reader_, ids, details);
  readme << endl << "```" << endl;

  string path = os::pathJoin(outputFolder_, "metadata.jsons");
  output_.open(path, ofstream::out | ofstream::trunc);
  if (output_.fail()) {
    XR_LOGE("Couldn't create file {}", path);
    return FAILURE;
  }
  output_ << RecordFileInfo::jsonOverview(reader_, ids, details) << endl;
  return SUCCESS;
}

int DataExtractor::completeOutput() {
  if (!output_.is_open()) {
    return SUCCESS;
  }
  bool fail = output_.fail();
  output_.close();
  if (fail || output_.fail()) {
    XR_LOGE("Failed to export data without errors...");
    return FAILURE;
  }
  return SUCCESS;
}

set<StreamId> DataExtractor::getStreams() const {
  set<StreamId> ids;
  for (const auto& iter : extractors_) {
    ids.insert(iter.first);
  }
  return ids;
}

DataExtractor::~DataExtractor() {
  completeOutput();
}

} // namespace vrs::utils
