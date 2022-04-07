// (c) Meta Platforms, Inc. and affiliates. Confidential and proprietary.

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
