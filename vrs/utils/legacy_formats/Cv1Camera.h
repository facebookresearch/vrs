// (c) Facebook, Inc. and its affiliates. Confidential and proprietary.

#pragma once

#include <vrs/DataLayoutConventions.h>
#include <vrs/DataPieces.h>
#include <vrs/FileFormat.h>

namespace OVR {
namespace Vision {

namespace Cv1Camera {

using ::vrs::AutoDataLayout;
using ::vrs::AutoDataLayoutEnd;
using ::vrs::DataPieceValue;
using ::vrs::DataLayoutConventions::ImageSpecType;
using ::vrs::FileFormat::LittleEndian;

constexpr uint32_t kStateVersion = 1;
constexpr uint32_t kConfigurationVersion = 2;
constexpr uint32_t kDataVersion = 1;

#pragma pack(push, 1)

struct VRSConfiguration {
  VRSConfiguration() {}

  LittleEndian<ImageSpecType> width;
  LittleEndian<ImageSpecType> height;
  LittleEndian<ImageSpecType> bytesPerPixels;
  LittleEndian<ImageSpecType> format;
};

struct VRSData {
  VRSData() {}

  LittleEndian<double> exposureTime;
  LittleEndian<double> arrivalTime;
  LittleEndian<uint64_t> frameCounter;
  LittleEndian<uint32_t> cameraUniqueId;
};

#pragma pack(pop)

class DataLayoutConfiguration : public AutoDataLayout {
 public:
  constexpr static uint32_t kConfigurationVersion = Cv1Camera::kConfigurationVersion;

  DataPieceValue<ImageSpecType> width{::vrs::DataLayoutConventions::kImageWidth};
  DataPieceValue<ImageSpecType> height{::vrs::DataLayoutConventions::kImageHeight};
  DataPieceValue<ImageSpecType> bytesPerPixels{::vrs::DataLayoutConventions::kImageBytesPerPixel};
  DataPieceValue<ImageSpecType> format{::vrs::DataLayoutConventions::kImagePixelFormat};

  AutoDataLayoutEnd endLayout;
};

class DataLayoutData : public AutoDataLayout {
 public:
  constexpr static uint32_t kDataVersion = Cv1Camera::kDataVersion;

  DataPieceValue<double> exposureTime{"exposure_timestamp"};
  DataPieceValue<double> arrivalTimestamp{"arrival_timestamp"};
  DataPieceValue<uint64_t> frameCounter{"frame_counter"};
  DataPieceValue<uint32_t> cameraUniqueId{"camera_unique_id"};

  AutoDataLayoutEnd endLayout;
};

} // namespace Cv1Camera
} // namespace Vision
} // namespace OVR
