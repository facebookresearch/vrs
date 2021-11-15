// Copyright (c) Facebook, Inc. and its affiliates. Confidential and proprietary.

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
  using ImageSpecType = DataLayoutConventions::ImageSpecType;

  // Spec of a raw image, stored in data records (controlled by most recent config record)
  DataPieceValue<ImageSpecType> width{DataLayoutConventions::kImageWidth};
  DataPieceValue<ImageSpecType> height{DataLayoutConventions::kImageHeight};
  // Prefer to specify a storage type when storing an enum, to make the storage format explicit.
  DataPieceEnum<PixelFormat, ImageSpecType> pixelFormat{DataLayoutConventions::kImagePixelFormat};

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
