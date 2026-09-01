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

#include <cstdint>
#include <memory>

#include <vrs/ForwardDefinitions.h>
#include <vrs/os/Platform.h>

#if IS_VRS_FB_INTERNAL()
#include <vrs/utils/PixelFrameOptions_fb.h>
#endif

namespace vrs::utils {

class PixelFrame;

#if IS_VRS_OSS_CODE()
/// Open-source builds have no Meta-only fields; these keep NormalizeOptions and
/// NormalizeOptionsConfig a uniform type across the OSS and Meta-internal trees.
/// The Meta-internal definitions live in PixelFrameOptions_fb.h.
struct MetaNormalizeOptions {};
struct MetaNormalizeOptionsConfig {};
#endif

// When additional compression options are needed, use this struct instead of overloading the API
struct CompressionOptions {
  uint16_t maxCompressionThreads{0}; // max compression threads, or 0 to let encoder decide.

  /// jxl specific options

  /// jxlQualityIsButteraugliDistance: if false, quality is a percentage, 100% being lossless.
  /// If true, quality is a Butteraugli distance (Google "Butteraugli" for details), where
  /// Butteraugli distance 0 is lossless, and 15 is the worst Butteraugli distance supported.
  /// 99.99% ~ Butteraugli 0.1, 99% ~ Butteraugli 0.2, 95.5% ~ Butteraugli 0.5, 90% ~ Butteraugli 1
  bool jxlQualityIsButteraugliDistance{false};
  /// jxlEffort: Sets encoder effort/speed level without affecting decoding speed.
  /// Valid values are, from faster to slower speed: 1:lightning 2:thunder 3:falcon
  /// 4:cheetah 5:hare 6:wombat 7:squirrel 8:kitten 9:tortoise.
  int jxlEffort{3};
};

enum class ImageSemantic : uint16_t {
  Undefined,
  Image, ///< Visual data ready to display as is
  Camera [[deprecated("Use Image instead")]] = Image,
  ObjectClassSegmentation, ///< Segmentation data, one value per object class.
  ObjectIdSegmentation, ///< Segmentation data, one value per object instance.
  Depth, ///< Depth information
  BuildSpecific, ///< Rendering selected by build-private NormalizeOptions fields.
};

/// Parameters describing how a PixelFrame should be normalized.
///
/// "Normalizing" means converting a frame from its on-disk representation into a
/// form suitable for visualization or re-encoding/compression. Different streams require
/// different treatments (e.g. plain camera images, depth, segmentation, ...),
/// and this struct exists so that those alternate normalization methods can be
/// selected and tuned without overloading the normalization API: callers fill
/// in a NormalizeOptions, and the normalization code branches on its fields.
///
/// To add a new normalization use case:
///  - Extend ImageSemantic and/or add fields below.
///  - Implement NormalizeOptions construction in PixelFrame::captureNormalizeOptionsConfig and/or
///    PixelFrame::getStreamNormalizeOptions.
///  - Make PixelFrame::normalizeFrame and friends do normalization as needed.
struct NormalizeOptions {
  NormalizeOptions() = default;
  explicit NormalizeOptions(ImageSemantic semantic) : semantic{semantic} {}
  NormalizeOptions(ImageSemantic semantic, float min, float max)
      : semantic{semantic}, min{min}, max{max} {}

  ImageSemantic semantic{ImageSemantic::Undefined};
  bool speedOverPrecision{false}; // prefer speed (for display?) or precision (to save to disk?)
  float min{0};
  float max{0};
  MetaNormalizeOptions meta{}; // Meta-only fields (empty in open source)
};

/// Per-stream normalization parameters captured from a stream's configuration record, to build its
/// NormalizeOptions. Obtain one via PixelFrame::captureNormalizeOptionsConfig() while reading a
/// configuration record, keep it per stream, and pass it to
/// PixelFrame::getStreamNormalizeOptions(). The open-source part is empty for now -- it is the
/// placeholder a future configuration-record-driven use case will populate (see
/// captureNormalizeOptionsConfig); Meta-only parameters live in the meta field.
struct NormalizeOptionsConfig {
  MetaNormalizeOptionsConfig meta{}; // Meta-only fields (empty in open source)
};

/// Options for resizing (downscaling or upscaling) images
struct ResizeOptions {
  enum class Mode {
    None, ///< No resizing
    Ratio, ///< Resize by a ratio (e.g., 0.5 for half size, 2.0 for double size)
    Resolution ///< Resize to a specific resolution (supports proportional when only width or height
               ///< provided)
  };

  Mode mode{Mode::None};
  float ratio{1.0f}; ///< Resize ratio (used when mode == Ratio)
  uint32_t targetWidth{0}; ///< Target width (used when mode == Resolution)
  uint32_t targetHeight{0}; ///< Target height (used when mode == Resolution)
  uint32_t widthAlignment{1}; ///< Width alignment requirement (1 = no alignment, 2 = even, etc.)
  uint32_t heightAlignment{1}; ///< Height alignment requirement (1 = no alignment, 2 = even, etc.)

  ResizeOptions() = default;

  /// Create resize options with a ratio
  /// @param ratio: resize ratio (0.5 for half size, 2.0 for double size, etc.)
  static ResizeOptions withRatio(float ratio);

  /// Create resize options with target resolution
  /// If both width and height are provided, resize to exact dimensions
  /// If only width is provided (height=0), height is calculated proportionally
  /// If only height is provided (width=0), width is calculated proportionally
  static ResizeOptions withResolution(uint32_t width, uint32_t height);

  /// Compute target dimensions based on source dimensions and scaling options
  /// @param sourceWidth: original image width
  /// @param sourceHeight: original image height
  /// @param outTargetWidth: computed target width (set on success)
  /// @param outTargetHeight: computed target height (set on success)
  /// @return true if valid target dimensions were computed, false otherwise
  bool computeTargetDimensions(
      uint32_t sourceWidth,
      uint32_t sourceHeight,
      uint32_t& outTargetWidth,
      uint32_t& outTargetHeight) const;

  /// Tell if resizing a particular PixelFormat is supported
  /// @param pixelFormat: the pixel format to check
  /// @return true if resizing is supported, false otherwise
  static bool canResize(PixelFormat pixelFormat);

  /// Resize a PixelFrame according to the resize options
  /// @param sourceFrame: the source frame to resize
  /// @return a new resized frame, or nullptr if resizing failed or is not needed
  std::unique_ptr<PixelFrame> resize(const PixelFrame& sourceFrame) const;
};

} // namespace vrs::utils
