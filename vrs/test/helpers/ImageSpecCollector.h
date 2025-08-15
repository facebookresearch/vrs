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

#pragma once

#include <vrs/RecordFileReader.h>
#include <vrs/utils/PixelFrame.h>
#include <vrs/utils/VideoRecordFormatStreamPlayer.h>

namespace vrs {
namespace test {

struct InputOutpuSpecs {
  ImageContentBlockSpec inputSpec;
  ImageContentBlockSpec outputSpec;
};

inline ImageContentBlockSpec operator+(
    const ImageContentBlockSpec& lhs,
    const ImageContentBlockSpec& rhs) {
  return {lhs.getImageFormat(), rhs.getPixelFormat(), rhs.getWidth(), rhs.getHeight()};
}

class ImageSpecCollector : public utils::VideoRecordFormatStreamPlayer {
 public:
  explicit ImageSpecCollector(RecordFileReader& reader);
  bool onImageRead(const CurrentRecord& record, size_t blockIndex, const ContentBlock& cb) override;

  // Original image specs from content block metadata
  map<StreamId, ImageContentBlockSpec> imageSpecs;

  // Decoded image specs from actual decoded image data
  map<StreamId, ImageContentBlockSpec> decodedImageSpecs;
};

std::map<StreamId, InputOutpuSpecs> getImageProcessing(
    RecordFileReader& inputReader,
    RecordFileReader& outputReader);

} // namespace test
} // namespace vrs
