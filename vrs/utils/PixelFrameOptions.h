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

namespace vrs::utils {

class PixelFrame;

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
  Camera, ///< Visual data (regular image)
  ObjectClassSegmentation, ///< Segmentation data, one value per object class.
  ObjectIdSegmentation, ///< Segmentation data, one value per object instance.
  Depth, ///< Depth information
};

struct NormalizeOptions {
  NormalizeOptions() = default;
  explicit NormalizeOptions(ImageSemantic semantic) : semantic{semantic} {}
  NormalizeOptions(ImageSemantic semantic, float min, float max)
      : semantic{semantic}, min{min}, max{max} {}

  ImageSemantic semantic{ImageSemantic::Undefined};
  bool speedOverPrecision{false}; // prefer speed (for display?) or precision (to save to disk?)
  float min{0};
  float max{0};
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
