// Copyright (c) Facebook, Inc. and its affiliates. Confidential and proprietary.

#pragma once

#include <vrs/DataLayout.h>
#include <vrs/DataPieces.h>
#include <vrs/FileFormat.h>

namespace OVR {
namespace Vision {

namespace Cv1SyncPulse {

using ::vrs::AutoDataLayout;
using ::vrs::AutoDataLayoutEnd;
using ::vrs::DataPieceValue;
using ::vrs::FileFormat::LittleEndian;

constexpr uint32_t kStateVersion = 1;
constexpr uint32_t kConfigurationVersion = 1;
constexpr uint32_t kDataVersion = 1;

#pragma pack(push, 1)

struct VRSData {
  VRSData() {}

  LittleEndian<double> appReadTimestamp;
  LittleEndian<double> cameraTime;
  LittleEndian<double> cameraTimestamp;
  LittleEndian<double> sampleTimestamp;
  LittleEndian<uint64_t> cameraFrameCount;
  LittleEndian<uint8_t> cameraPattern;
  LittleEndian<uint8_t> numIMUSamples;
};

#pragma pack(pop)

struct DataLayoutData : public AutoDataLayout {
  enum : uint32_t { kVersion = Cv1SyncPulse::kDataVersion };

  DataPieceValue<double> appReadTimestamp{"app_read_timestamp"};
  DataPieceValue<double> cameraTime{"camera_time"};
  DataPieceValue<double> cameraTimestamp{"camera_timestamp"};
  DataPieceValue<double> sampleTimestamp{"sample_timestamp"};
  DataPieceValue<uint64_t> cameraFrameCount{"camera_frame_count"};
  DataPieceValue<uint8_t> cameraPattern{"camera_pattern"};
  DataPieceValue<uint8_t> numIMUSamples{"num_imu_sample"};

  AutoDataLayoutEnd endLayout;
};

} // namespace Cv1SyncPulse
} // namespace Vision
} // namespace OVR
