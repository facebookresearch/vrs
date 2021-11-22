// Copyright (c) Facebook, Inc. and its affiliates. Confidential and proprietary.

#pragma once

#include <cstdint>

#include <vrs/DataLayout.h>
#include <vrs/DataPieces.h>

// Note: The VRS stream type for time sync data is vrs::RecordableTypeId::TimeRecordableClass.

namespace aria {

struct TimeSyncConfigurationLayout : public vrs::AutoDataLayout {
  static constexpr uint32_t kVersion = 1;

  vrs::DataPieceValue<std::uint32_t> streamId{"stream_id"};

  // Sample rate for time data [Hz]
  vrs::DataPieceValue<double> sampleRateHz{"sample_rate_hz"};

  vrs::AutoDataLayoutEnd endLayout;
};

struct TimeSyncDataLayout : public vrs::AutoDataLayout {
  static constexpr uint32_t kVersion = 1;

  // The capture timestamp in nanoseconds using a monotonic clock, same clock that
  // is used for the VRS records timestamps
  vrs::DataPieceValue<std::int64_t> monotonicTimestampNs{"monotonic_timestamp_ns"};

  // The real time clock or wall clock in nanoseconds
  vrs::DataPieceValue<std::int64_t> realTimestampNs{"real_timestamp_ns"};

  vrs::AutoDataLayoutEnd endLayout;
};

} // namespace aria
