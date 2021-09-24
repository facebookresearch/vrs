// (c) Facebook, Inc. and its affiliates. Confidential and proprietary.

#pragma once

#include <memory>

#include <vrs/Compressor.h>
#include <vrs/IndexRecord.h>
#include <vrs/StreamPlayer.h>
#include <vrs/gaia/UploadMetadata.h>

#include "CopyHelpers.h"
#include "FilteredVRSFileReader.h"

namespace vrs::utils {

class ImageFilter {
 public:
  ImageFilter() {}
  virtual ~ImageFilter() = default;
  /// Definition for accept format function
  /// @param imageSpec: image details used to determine if filter can be used
  virtual bool accept(const ImageContentBlockSpec& imageSpec) = 0;
  /// Definition for an image filter function.
  /// @param header: descriptor of the record containing the image.
  /// @param blockIndex: the index of the content type block in the record.
  /// @param contentBlock: description of the content block, containing the format of the image.
  /// @param inputFrame: raw pixel data of the image to process.
  /// @param outputFrame: allocated pixel data for the output image. The pixel format must be the
  /// exact same as that of the input image.
  virtual void filter(
      const IndexRecord::RecordInfo& recordInfo,
      size_t blockIndex,
      const ContentBlock& contentBlock,
      const std::vector<uint8_t>& inputFrame,
      std::vector<uint8_t>& outputFrame) = 0;

  /// Override to tell how many threads should be used to process images
  /// By default, returns 1 (single threaded operation).
  virtual size_t getThreadCount();
};

/// Filter images of a VRS file & create a copy with the same metadata, but with filtered images.
/// The streams & records of the source file may be filtered by the FilteredVRSFileReader that
/// specifies the source.
/// Depending on the image filter's thread count, will run the filter single or multithreaded.
/// @param imageFilter: the image filter to apply. It's the same filter for every frame, but since
/// you have access to the details of the record, you can easily choose to apply a different filter
/// to different streams.
/// @param filteredReader: the source file.
/// @param throttledWriter: a throttled writer, that will allow prevent over-using memory.
/// @param pathToCopy: path to the output file.
/// @param copyParams: optional parameters derived from the VRS file copy operations.
/// The following parameters will be used:
/// compressionPoolSize, compressionPreset, maxChunkSizeMB, outRecordCopiedCount
/// @param uploadMetadata: optional. When specified, the data is streamed-up to Gaia.
/// The file path is used as a temporary location during the upload, and auto deleted on the way.
/// @return A status error code, 0 means no error.
int filterImages(
    ImageFilter& imageFilter,
    FilteredVRSFileReader& filteredReader,
    ThrottledWriter& throttledWriter,
    const std::string& pathToCopy,
    CopyOptions& copyOptions,
    std::unique_ptr<UploadMetadata>&& uploadMetadata = nullptr);

} // namespace vrs::utils
