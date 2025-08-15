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

#include "ImageSpecCollector.h"

#include <gtest/gtest.h>

namespace vrs {
namespace test {

ImageSpecCollector::ImageSpecCollector(RecordFileReader& reader) {
  for (auto id : reader.getStreams()) {
    reader.setStreamPlayer(id, this);
    auto config = reader.getRecord(id, Record::Type::CONFIGURATION, 0);
    if (config != nullptr) {
      reader.readRecord(*config);
    }
    auto data = reader.getRecord(id, Record::Type::DATA, 0);
    if (data != nullptr) {
      reader.readRecord(*data);
    }
  }
}

bool ImageSpecCollector::onImageRead(
    const CurrentRecord& record,
    size_t blockIndex,
    const ContentBlock& cb) {
  if (record.recordType == Record::Type::DATA) {
    // Collect original content block image spec
    imageSpecs[record.streamId] = cb.image();

    // Decode the image and collect the decoded image spec
    utils::PixelFrame frame;
    if (frame.readFrame(record.reader, cb)) {
      decodedImageSpecs[record.streamId] = frame.getSpec();
    } else {
      // If decoding fails, use the original spec as fallback
      decodedImageSpecs[record.streamId] = cb.image();
    }
  }
  return false;
}

std::map<StreamId, InputOutpuSpecs> getImageProcessing(
    RecordFileReader& inputReader,
    RecordFileReader& outputReader) {
  // Collect image specs from both files
  ImageSpecCollector inputCollector(inputReader);
  ImageSpecCollector outputCollector(outputReader);

  // Convert to sorted vectors for easy comparison
  std::map<StreamId, InputOutpuSpecs> processingSpecs;

  // All input & output specs should be present, or the images are not readable
  EXPECT_TRUE(inputCollector.decodedImageSpecs.size() == inputCollector.imageSpecs.size());
  EXPECT_TRUE(outputCollector.decodedImageSpecs.size() == outputCollector.imageSpecs.size());

  if (inputCollector.decodedImageSpecs.size() == inputCollector.imageSpecs.size() &&
      outputCollector.decodedImageSpecs.size() == outputCollector.imageSpecs.size()) {
    for (const auto& [streamId, inputSpec] : inputCollector.imageSpecs) {
      auto decodedIt = inputCollector.decodedImageSpecs.find(streamId);
      EXPECT_TRUE(decodedIt != inputCollector.decodedImageSpecs.end());
      const auto& inputDecodedSpec = decodedIt->second;

      auto outputSpecIt = outputCollector.imageSpecs.find(streamId);
      if (outputSpecIt != outputCollector.imageSpecs.end()) {
        // the stream is present in the output file
        auto outputDecodedIt = outputCollector.decodedImageSpecs.find(streamId);
        EXPECT_TRUE(outputDecodedIt != outputCollector.decodedImageSpecs.end());
        const auto& outputSpec = outputSpecIt->second;
        const auto& outputDecodedSpec = outputDecodedIt->second;
        processingSpecs[streamId] = {inputSpec + inputDecodedSpec, outputSpec + outputDecodedSpec};
      } else {
        // the stream is missing in the output file
        processingSpecs[streamId] = {inputSpec + inputDecodedSpec, {}};
      }
    }
  }

  return processingSpecs;
}

} // namespace test
} // namespace vrs
