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

#pragma once

#include <memory>

#include <vrs/utils/PixelFrame.h>
#include <vrs/utils/VideoRecordFormatStreamPlayer.h>

namespace vrs {
namespace utils {

class ImageExtractor : public utils::VideoRecordFormatStreamPlayer {
 public:
  ImageExtractor(const string& folderPath, uint32_t& counter, const bool extractImagesRaw)
      : folderPath_{folderPath}, imageFileCounter_{counter}, extractImagesRaw_(extractImagesRaw) {}
  bool onImageRead(const CurrentRecord& record, size_t, const ContentBlock& ib) override;
  bool onUnsupportedBlock(const CurrentRecord& record, size_t, const ContentBlock& cb) override;

 protected:
  const string& folderPath_;
  std::shared_ptr<PixelFrame> inputFrame_;
  std::shared_ptr<PixelFrame> processedFrame_;
  uint32_t& imageFileCounter_;
  uint32_t imageCounter_ = 0;
  const bool extractImagesRaw_;
};

} // namespace utils
} // namespace vrs
