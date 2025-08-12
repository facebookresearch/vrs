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

#include <memory>

#include <ocean/base/Frame.h>

#include <vrs/RecordFormat.h>

namespace vrs::utils {

/// Convert VRS pixel format to Ocean pixel format.
///
/// Provides comprehensive mapping from VRS pixel formats to their Ocean equivalents,
/// supporting formats used in both normalization and resizing operations.
///
/// @param targetPixelFormat: The VRS pixel format to convert
/// @return The corresponding Ocean pixel format, or FORMAT_UNDEFINED if unsupported
Ocean::FrameType::PixelFormat vrsToOceanPixelFormat(vrs::PixelFormat targetPixelFormat);

/// Create an Ocean::Frame from a VRS image specification and data buffer with proper
/// plane initialization for multi-plane formats like YUV_I420_SPLIT.
///
/// This function handles the complex logic of calculating plane addresses and padding
/// elements for different pixel formats, ensuring Ocean::Frame objects are correctly
/// initialized for formats with multiple planes.
///
/// @param imageSpec: The VRS image specification describing the pixel format and layout
/// @param data: Pointer to the raw image data buffer
/// @param oceanPixelFormat: The Ocean pixel format to use (for single-plane formats)
/// @return A unique_ptr to an Ocean::Frame with properly initialized planes, or nullptr on failure
std::unique_ptr<Ocean::Frame> createOceanFrameFromVRS(
    const ImageContentBlockSpec& imageSpec,
    const uint8_t* data,
    Ocean::FrameType::PixelFormat oceanPixelFormat);

} // namespace vrs::utils
