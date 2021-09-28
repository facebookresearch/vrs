// Copyright (c) Facebook, Inc. and its affiliates. Confidential and proprietary.

#pragma once

#include <vrs/DataLayout.h>
#include <vrs/DataPieces.h>
#include <vrs/FileFormat.h>

namespace OVR {
namespace Vision {
namespace Cv1IMU {

using ::vrs::AutoDataLayout;
using ::vrs::AutoDataLayoutEnd;
using ::vrs::DataPieceValue;
using ::vrs::Point3Df;
using ::vrs::FileFormat::LittleEndian;

constexpr uint32_t kStateVersion = 1;
constexpr const uint32_t kConfigurationVersion = 1;
constexpr const uint32_t kDataVersion = 1;

#pragma pack(push, 1)

struct VRSData {
  VRSData() {}

  LittleEndian<double> appReadTimestamp;
  LittleEndian<double> sampleTimestamp;
  LittleEndian<float> temperature;
  LittleEndian<uint64_t> runningSampleCount;
  LittleEndian<uint8_t> numSamples;
};

struct VRSIMUSample {
  VRSIMUSample() {}

  LittleEndian<float> accelX;
  LittleEndian<float> accelY;
  LittleEndian<float> accelZ;
  LittleEndian<float> gyroX;
  LittleEndian<float> gyroY;
  LittleEndian<float> gyroZ;
};

#pragma pack(pop)

// This definition assumes that there are two IMU samples/data frame
// which is normally the case, but not necessarily always.
// Records with a single sample will be automatically skipped.
struct DataLayoutData : public AutoDataLayout {
  enum : uint32_t { kVersion = Cv1IMU::kDataVersion };

  DataPieceValue<double> appReadTimestamp{"app_read_timestamp"};
  DataPieceValue<double> sampleTimestamp{"sample_timestamp"};

  DataPieceValue<float> temperature{"temperature"};
  DataPieceValue<uint64_t> runningSampleCount{"running_sample_count"};
  DataPieceValue<uint8_t> numSamples{"num_samples"};

  DataPieceValue<Point3Df> accel1{"acceleration_1"};
  DataPieceValue<Point3Df> gyro1{"gyro_1"};

  DataPieceValue<Point3Df> accel2{"acceleration_2"};
  DataPieceValue<Point3Df> gyro2{"gyro_2"};

  AutoDataLayoutEnd endLayout;
};

} // namespace Cv1IMU
} // namespace Vision
} // namespace OVR
