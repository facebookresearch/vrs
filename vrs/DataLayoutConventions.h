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

#include "DataLayout.h"
#include "DataPieces.h"
#include "ForwardDefinitions.h"
#include "RecordFormat.h"

namespace vrs {

/// Name and type conventions used to map ContentBlockReader needs to DataLayout data
namespace DataLayoutConventions {

/// Convention to specify the size of the content block following
constexpr const char* kNextContentBlockSize = "next_content_block_size";
using ContentBlockSizeType = uint32_t;

/// DataLayout only containing the size of the content block following.
class NextContentBlockSizeSpec : public AutoDataLayout {
 public:
  DataPieceValue<ContentBlockSizeType> nextContentBlockSize{kNextContentBlockSize};

  AutoDataLayoutEnd end;
};

/// DataLayout convention name for the image width.
constexpr const char* kImageWidth = "image_width";
/// DataLayout convention name for the image height.
constexpr const char* kImageHeight = "image_height";
/// DataLayout convention name for the image stride.
constexpr const char* kImageStride = "image_stride";
/// DataLayout convention name for the pixel format specification (see vrs::ImageFormat).
constexpr const char* kImagePixelFormat = "image_pixel_format";
/// DataLayout convention name for the number of bytes per pixel, deprecated.
/// Deprecated because less specific than kImagePixelFormat, which you should use instead.
constexpr const char* kImageBytesPerPixel = "image_bytes_per_pixel";
/// DataLayout convention name for video codec name.
constexpr const char* kImageCodecName = "image_codec_name";
/// DataLayout convention video codec quality setting.
constexpr const char* kImageCodecQuality = "image_codec_quality";

/// Data type to use for the kImageXXX fields above.
using ImageSpecType = uint32_t;

/// \brief DataLayout definitions use to describe what's in an image content block.
///
/// These names and types are a convention that enables us to find image block spec within
/// a DataLayout block, which is either before the image content block in the same record,
/// or in the last Configuration record. Note that once a configuration *location* is found,
/// the next time around, the same *location* will be used again. Since RecordFormats are
/// static for a single record, this won't create any surprise, but if you use different
/// configuration record formats, this might make the search fail (the last record doesn't
/// include the spec), or return the values for a record that is no longer the last one,
/// though it is guarantied to be the last one of that format.
///
/// Note that the values used are *not* static, so that if the configuration changes,
/// the lastest value is used, without having to search each time we have a new image block.
class ImageSpec : public AutoDataLayout {
 public:
  DataPieceValue<ImageSpecType> width{kImageWidth};
  DataPieceValue<ImageSpecType> height{kImageHeight};
  DataPieceValue<ImageSpecType> stride{kImageStride};
  DataPieceEnum<PixelFormat, ImageSpecType> pixelFormat{kImagePixelFormat};

  // For video encoding
  DataPieceString codecName{kImageCodecName};
  DataPieceValue<ImageSpecType> codecQuality{kImageCodecQuality};

  // Deprecated fields
  DataPieceValue<ImageSpecType> bytesPerPixels{kImageBytesPerPixel};
  // for legacy data compatibility
  DataPieceValue<uint8_t> bytesPerPixels8{kImageBytesPerPixel};

  AutoDataLayoutEnd end;

  /// Helper method to determine the image content block based on available values.
  /// Will interpret legacy specifications, when a pixel format wasn't specified.
  /// imageFormat: image format expected, RAW or VIDEO.
  /// blockSize: content block size, required for VIDEO blocks.
  /// return An image content type on success, or an empty content block on failure.
  ContentBlock getImageContentBlock(
      const ImageContentBlockSpec& base,
      size_t blockSize = ContentBlock::kSizeUnknown);
};

/// DataLayout convention name for the stream's most recent video keyframe timestamp.
constexpr const char* kImageKeyFrameTimeStamp = "image_key_frame_timestamp";
/// DataLayout convention name for video key frame index since the last key frame.
/// 0 = this frame is a key frame, 1 = the previous frame was the last seen key frame, etc...
constexpr const char* kImageKeyFrameIndex = "image_key_frame_index";

/// DataLayout definitions use to describe a video image content block.
class VideoFrameSpec : public AutoDataLayout {
 public:
  DataPieceValue<double> keyFrameTimestamp{kImageKeyFrameTimeStamp};
  DataPieceValue<ImageSpecType> keyFrameIndex{kImageKeyFrameIndex};

  bool hasVideoSpec() const {
    return isMapped() && keyFrameTimestamp.isAvailable() && keyFrameIndex.isAvailable();
  }

  AutoDataLayoutEnd end;
};

/// DataLayout convention name for the audio sample format (see vrs::AudioSampleFormat).
constexpr const char* kAudioSampleFormat = "audio_sample_format";
/// DataLayout convention name for the padded number of bytes per sample.
constexpr const char* kAudioSampleStride = "audio_sample_stride";
/// DataLayout convention name for the audio channel count: mono = 1, stereo = 2, etc.
constexpr const char* kAudioChannelCount = "audio_channel_count";
/// DataLayout convention name for the sample rate (samples per seconde).
constexpr const char* kAudioSampleRate = "audio_sample_rate";
/// DataLayout convention name for the number of samples in the content block.
constexpr const char* kAudioSampleCount = "audio_sample_count";

/// \brief DataLayout definitions use to describe what's in an audio content block.
///
/// These names and types are a convention that enables us to find audio block spec within
/// a DataLayout block, which is either before the audio content block in the same record,
/// or in the last Configuration record. Note that once a configuration *location* is found,
/// the next time around, the same *location* will be used again.
///
/// Note that the values used are *not* static, so that if the configuration changes,
/// the lastest value is used, without having to search each time we have a new image block.
class AudioSpec : public AutoDataLayout {
 public:
  DataPieceEnum<AudioSampleFormat, uint8_t> sampleType{kAudioSampleFormat};
  DataPieceValue<uint8_t> sampleStride{kAudioSampleStride};
  DataPieceValue<uint8_t> channelCount{kAudioChannelCount};
  DataPieceValue<uint32_t> sampleRate{kAudioSampleRate};
  DataPieceValue<uint32_t> sampleCount{kAudioSampleCount};

  AutoDataLayoutEnd end;
};

} // namespace DataLayoutConventions
} // namespace vrs
