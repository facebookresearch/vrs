// Copyright (c) Facebook, Inc. and its affiliates. Confidential and proprietary.

#pragma once

#include <vrs/DataLayout.h>
#include <vrs/DataPieces.h>
#include <vrs/FileFormat.h>

namespace OVR {
namespace Vision {

namespace MontereySyncPulse {

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
  LittleEndian<uint32_t> syncTimestamp;
  LittleEndian<uint32_t> cameraPattern;
  LittleEndian<uint32_t> syncCount;
  LittleEndian<double> syncTimestampDouble;
};

#pragma pack(pop)

struct DataLayoutData : public AutoDataLayout {
  enum : uint32_t { kVersion = kDataVersion };

  DataPieceValue<double> appReadTimestamp{"app_read_timestamp"};
  DataPieceValue<uint32_t> syncTimestamp{"sync_timestamp"};
  DataPieceValue<uint32_t> cameraPattern{"camera_pattern"};
  DataPieceValue<uint32_t> syncCount{"sync_count"};
  DataPieceValue<double> syncTimestampDouble{"sync_timestamp"};

  AutoDataLayoutEnd endLayout;
};

// Original Proto0 PulseSync format, deprecated & deleted in D5354713
struct Proto0SyncPulseDataLayoutData : public AutoDataLayout {
  enum : uint32_t { kVersion = 1 };

  DataPieceValue<double> appReadTimestamp{"app_read_timestamp"};
  DataPieceValue<uint32_t> syncTimestamp{"sync_timestamp"};
  DataPieceValue<uint32_t> cameraPattern{"camera_pattern"};
  DataPieceValue<uint32_t> syncCount{"sync_count"};

  AutoDataLayoutEnd endLayout;
};

} // namespace MontereySyncPulse
} // namespace Vision
} // namespace OVR
