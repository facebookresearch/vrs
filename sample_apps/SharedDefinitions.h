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

#include <vrs/DataLayout.h>
#include <vrs/DataLayoutConventions.h>
#include <vrs/DataPieces.h>

namespace vrs_sample_apps {

using namespace vrs; // we should not do this in a header, but for readability, we do in this sample

// This file contains definitions shared between our reader & writer code

constexpr const char* kSampleFileName = "sample_file.vrs";

// DataLayout definitions: these are groups of data fields that will store in different records.

/// Sample metadata for configuration records of an image stream.
struct CameraStreamConfig : public AutoDataLayout {
  using ImageSpecType = datalayout_conventions::ImageSpecType;

  // Spec of a raw image, stored in data records (controlled by most recent config record)
  DataPieceValue<ImageSpecType> width{datalayout_conventions::kImageWidth};
  DataPieceValue<ImageSpecType> height{datalayout_conventions::kImageHeight};
  // Prefer to specify a storage type when storing an enum, to make the storage format explicit.
  DataPieceEnum<PixelFormat, ImageSpecType> pixelFormat{datalayout_conventions::kImagePixelFormat};

  // Additional configuration information for the camera
  DataPieceVector<float> cameraCalibration{"camera_calibration"};

  AutoDataLayoutEnd endLayout;
};

/// Sample metadata for data records of an image stream.
struct CameraStreamData : public AutoDataLayout {
  // Additional data provided with each frame
  DataPieceValue<uint64_t> exposureTime{"exposure_time"};
  DataPieceValue<float> exposure{"exposure"};
  DataPieceValue<uint64_t> frameCounter{"frame_counter"};
  DataPieceValue<float> cameraTemperature{"camera_temperature"};

  AutoDataLayoutEnd endLayout;
};

/// Sample metadata for configuration records of a metadata stream.
struct MotionStreamConfig : public AutoDataLayout {
  DataPieceValue<double> motionStreamParam{"some_motion_stream_param"};

  AutoDataLayoutEnd endLayout;
};

/// Sample metadata for data records of a metadata stream.
struct MotionStreamData : public AutoDataLayout {
  DataPieceVector<Matrix3Dd> motionData{"motion_data"};

  AutoDataLayoutEnd endLayout;
};

// Some constants for our audio streams
const uint8_t kNumChannels = 1;
const uint32_t kSampleRate = 44100;
const size_t kAudioBlockSize = 256;

// Number of data records we will create in each of our test file streams
const size_t kDataRecordCount = 100;

// Definition of the Recordable Class flavors we will use for our sample streams
constexpr const char* kCameraStreamFlavor = "sample/camera";
constexpr const char* kAudioStreamFlavor = "sample/audio";
constexpr const char* kMotionStreamFlavor = "sample/motion";

// Some arbitrary definitions to play with
#define CALIBRATION_VALUES \
  { 23, 53, 343, 3, 12, 8 }
constexpr double kMotionValue = 25;

} // namespace vrs_sample_apps
