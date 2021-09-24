// (c) Facebook, Inc. and its affiliates. Confidential and proprietary.

#pragma once

#include <vrs/DataLayoutConventions.h>
#include <vrs/DataPieces.h>
#include <vrs/DataReference.h>
#include <vrs/FileFormat.h>
#include <vrs/StreamPlayer.h>

namespace OVR {
namespace Vision {
namespace MontereyCamera {

using ::vrs::AutoDataLayout;
using ::vrs::AutoDataLayoutEnd;
using ::vrs::Bool;
using ::vrs::CurrentRecord;
using ::vrs::DataPieceArray;
using ::vrs::DataPieceValue;
using ::vrs::DataReference;
using ::vrs::Point3Df;
using ::vrs::Point4Df;
using ::vrs::DataLayoutConventions::ImageSpecType;
using ::vrs::FileFormat::LittleEndian;

constexpr uint32_t kStateVersion = 1;
constexpr uint32_t kConfigurationVersion = 5;
constexpr uint32_t kCalibrationDataSize = 23;

#pragma pack(push, 1)

struct VRSDataV2 {
  enum : uint32_t { kDataVersion = 2 };

  LittleEndian<double> captureTimestamp;
  LittleEndian<double> arrivalTimestamp;
  LittleEndian<uint64_t> frameCounter;
  LittleEndian<uint32_t> cameraUniqueId;
};

struct VRSDataV3 : VRSDataV2 {
  enum : uint32_t { kDataVersion = 3 };

  void upgradeFrom(uint32_t formatVersion) {
    if (formatVersion < kDataVersion) {
      streamId.set(0);
      gainHAL.set(0);
    }
  }

  LittleEndian<int32_t> streamId;
  LittleEndian<uint32_t> gainHAL;
};

struct VRSDataV4 : VRSDataV3 {
  enum : uint32_t { kDataVersion = 4 };

  void upgradeFrom(uint32_t formatVersion) {
    if (formatVersion < kDataVersion) {
      VRSDataV3::upgradeFrom(formatVersion);
      exposureDuration.set(0);
    }
  }

  LittleEndian<double> exposureDuration;
};

constexpr float kGainMultiplierConvertor = 16.0;

struct VRSDataV5 : VRSDataV4 {
  enum : uint32_t { kDataVersion = 5 };

  void upgradeFrom(uint32_t formatVersion) {
    if (formatVersion < kDataVersion) {
      VRSDataV4::upgradeFrom(formatVersion);
      gain.set(gainHAL.get() / kGainMultiplierConvertor);
    }
  }

  LittleEndian<float> gain;
};

struct VRSData : VRSDataV5 {
  enum : uint32_t { kDataVersion = 6 };

  bool canHandle(
      const CurrentRecord& record,
      void* imageData,
      uint32_t imageSize,
      DataReference& outDataReference) {
    uint32_t formatVersion = record.formatVersion;
    uint32_t payloadSize = record.recordSize;
    if ((formatVersion == kDataVersion && sizeof(VRSData) + imageSize == payloadSize) ||
        (formatVersion == VRSDataV5::kDataVersion &&
         sizeof(VRSDataV5) + imageSize == payloadSize) ||
        (formatVersion == VRSDataV4::kDataVersion &&
         sizeof(VRSDataV4) + imageSize == payloadSize) ||
        (formatVersion == VRSDataV3::kDataVersion &&
         sizeof(VRSDataV3) + imageSize == payloadSize) ||
        (formatVersion == VRSDataV2::kDataVersion &&
         sizeof(VRSDataV2) + imageSize == payloadSize)) {
      outDataReference.useRawData(this, payloadSize - imageSize, imageData, imageSize);
      return true;
    }
    return false;
  }

  void upgradeFrom(uint32_t formatVersion) {
    if (formatVersion < kDataVersion) {
      VRSDataV5::upgradeFrom(formatVersion);
      temperature.set(-1.0);
    }
  }

  LittleEndian<float> temperature;
};

const uint32_t kDataVersion = VRSData::kDataVersion;

struct VRSConfiguration {
  VRSConfiguration() {}

  LittleEndian<uint32_t> width;
  LittleEndian<uint32_t> height;
  LittleEndian<uint32_t> bytesPerPixels;
  LittleEndian<uint32_t> format;
  LittleEndian<uint32_t> cameraId;
  LittleEndian<uint16_t> cameraSerial;
  LittleEndian<float> calibration[kCalibrationDataSize];
};

#pragma pack(pop)

// The types & names of some of these fields are using the new DataLayout conventions
// for ImageContentBlocks. See VRS/DataLayoutConventions.h
class DataLayoutConfiguration : public AutoDataLayout {
 public:
  constexpr static uint32_t kConfigurationVersion = 5;

  DataPieceValue<ImageSpecType> width{::vrs::DataLayoutConventions::kImageWidth};
  DataPieceValue<ImageSpecType> height{::vrs::DataLayoutConventions::kImageHeight};
  DataPieceValue<ImageSpecType> bytesPerPixels{::vrs::DataLayoutConventions::kImageBytesPerPixel};
  DataPieceValue<ImageSpecType> format{::vrs::DataLayoutConventions::kImagePixelFormat};
  DataPieceValue<uint32_t> cameraId{"camera_id"};
  DataPieceValue<uint16_t> cameraSerial{"camera_serial"};
  DataPieceArray<float> calibration{"camera_calibration", kCalibrationDataSize};

  AutoDataLayoutEnd endLayout;
};

class DataLayoutDataV2 : public AutoDataLayout {
 public:
  constexpr static uint32_t kDataVersion = 2;

  DataPieceValue<double> captureTimestamp{"capture_timestamp"};
  DataPieceValue<double> arrivalTimestamp{"arrival_timestamp"};
  DataPieceValue<uint64_t> frameCounter{"frame_counter"};
  DataPieceValue<uint32_t> cameraUniqueId{"camera_unique_id"};

  AutoDataLayoutEnd endLayout;
};

class DataLayoutDataV3 : public AutoDataLayout {
 public:
  constexpr static uint32_t kDataVersion = 3;
  // v2
  DataPieceValue<double> captureTimestamp{"capture_timestamp"};
  DataPieceValue<double> arrivalTimestamp{"arrival_timestamp"};
  DataPieceValue<uint64_t> frameCounter{"frame_counter"};
  DataPieceValue<uint32_t> cameraUniqueId{"camera_unique_id"};
  // v3
  DataPieceValue<int32_t> streamId{"stream_id", 0};
  DataPieceValue<uint32_t> gainHAL{"gain_hal", 0};

  AutoDataLayoutEnd endLayout;
};

class DataLayoutDataV4 : public AutoDataLayout {
 public:
  constexpr static uint32_t kDataVersion = 4;
  // v2
  DataPieceValue<double> captureTimestamp{"capture_timestamp"};
  DataPieceValue<double> arrivalTimestamp{"arrival_timestamp"};
  DataPieceValue<uint64_t> frameCounter{"frame_counter"};
  DataPieceValue<uint32_t> cameraUniqueId{"camera_unique_id"};
  // v3
  DataPieceValue<int32_t> streamId{"stream_id", 0};
  DataPieceValue<uint32_t> gainHAL{"gain_hal", 0};
  // v4
  DataPieceValue<double> exposureDuration{"exposure_duration", 0};

  AutoDataLayoutEnd endLayout;
};

class DataLayoutDataV5 : public AutoDataLayout {
 public:
  constexpr static uint32_t kDataVersion = 5;
  // v2
  DataPieceValue<double> captureTimestamp{"capture_timestamp"};
  DataPieceValue<double> arrivalTimestamp{"arrival_timestamp"};
  DataPieceValue<uint64_t> frameCounter{"frame_counter"};
  DataPieceValue<uint32_t> cameraUniqueId{"camera_unique_id"};
  // v3
  DataPieceValue<int32_t> streamId{"stream_id", 0};
  DataPieceValue<uint32_t> gainHAL{"gain_hal", 0};
  // v4
  DataPieceValue<double> exposureDuration{"exposure_duration", 0};
  // v5
  DataPieceValue<float> gain{"gain", 0}; // complex default value: force calling a method

  DataLayoutDataV5() {
    captureTimestamp.setUnit("s");
    arrivalTimestamp.setUnit("s");
    exposureDuration.setUnit("s");
    gain.setRange(0, 10);
  }

  float getGain() {
    if (gain.isAvailable()) {
      return gain.get();
    }
    return gainHAL.get() / kGainMultiplierConvertor;
  }

  AutoDataLayoutEnd endLayout;
};

class DataLayoutData : public AutoDataLayout {
 public:
  constexpr static uint32_t kDataVersion = 6;
  // v2
  DataPieceValue<double> captureTimestamp{"capture_timestamp"};
  DataPieceValue<double> arrivalTimestamp{"arrival_timestamp"};
  DataPieceValue<uint64_t> frameCounter{"frame_counter"};
  DataPieceValue<uint32_t> cameraUniqueId{"camera_unique_id"};
  // v3
  DataPieceValue<int32_t> streamId{"stream_id", 0};
  DataPieceValue<uint32_t> gainHAL{"gain_hal", 0};
  // v4
  DataPieceValue<double> exposureDuration{"exposure_duration", 0};
  // v5
  DataPieceValue<float> gain{"gain", 0}; // complex default value: force calling a method
 public:
  float getGain() {
    if (gain.isAvailable()) {
      return gain.get();
    }
    return gainHAL.get() / kGainMultiplierConvertor;
  }
  // v6
  DataPieceValue<float> temperature{"temperature", -1};

  AutoDataLayoutEnd endLayout;
};

} // namespace MontereyCamera
} // namespace Vision
} // namespace OVR
