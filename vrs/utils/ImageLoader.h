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
#include <utility>

#include <vrs/ForwardDefinitions.h>
#include <vrs/utils/ImageIndexer.h>
#include <vrs/utils/PixelFrame.h>

namespace vrs::utils {

enum class ImageLoadType {
  Raw, // load the bytes as-is, minimum processing
  Decode, // decode the image if it's compressed, returning a PixelFrame of type RAW
  Normalize8, // decode the image, if it's compressed, and normalize it to grey8, or rgb8
  Normalize16, // decode the image, if it's compressed, and normalize it to grey8, grey16, or rgb8
};

/// Load an image from a buffer.
/// @param data: pointer to the data.
/// @param length: length of the data.
/// @param outFrame: on exit, the image read.
/// @param imageRef: an image reference. Note: dataOffset is ignored since the data was already read
/// @param loadType: how to load the image.
/// @return True on success.
bool loadImage(
    const void* data,
    size_t length,
    PixelFrame& outFrame,
    const DirectImageReference& imageRef,
    ImageLoadType loadType = ImageLoadType::Raw);

template <typename T>
inline bool loadImage(
    const vector<T>& data,
    PixelFrame& outFrame,
    const DirectImageReference& imageRef,
    ImageLoadType loadType = ImageLoadType::Raw) {
  return loadImage(data.data(), data.size() * sizeof(T), outFrame, imageRef, loadType);
}

/// Load an image directly from an open file, without having to parse the file.
/// @param file: an open file containing the data.
/// @param outFrame: on exit, the image read.
/// @param imageRef: an image reference.
/// @param loadType: how to load the image.
/// @return True on success.
bool loadImage(
    FileHandler& file,
    PixelFrame& outFrame,
    const DirectImageReference& imageRef,
    ImageLoadType loadType = ImageLoadType::Raw);

} // namespace vrs::utils
