// Copyright (c) Facebook, Inc. and its affiliates. Confidential and proprietary.

#pragma once

#include <cstdint>

#include <vrs/DataLayout.h>
#include <vrs/DataPieces.h>

namespace aria {

struct BarometerConfigurationLayout : public vrs::AutoDataLayout {
  static constexpr uint32_t kVersion = 1;

  vrs::DataPieceValue<std::uint32_t> streamId{"stream_id"};
  vrs::DataPieceString sensorModelName{"sensor_model_name"};

  // Sample rate for temperature and pressure data (in unit of Hz)
  vrs::DataPieceValue<double> sampleRate{"sample_rate"};

  vrs::AutoDataLayoutEnd end;
};

struct BarometerDataLayout : public vrs::AutoDataLayout {
  static constexpr uint32_t kVersion = 1;

  // Timestamp of data capture in board clock, in unit of nanoseconds.
  vrs::DataPieceValue<std::int64_t> captureTimestampNs{"capture_timestamp_ns"};

  // Temperature in Celcius degree.
  vrs::DataPieceValue<double> temperature{"temperature"};

  // Pressure in Pascal.
  vrs::DataPieceValue<double> pressure{"pressure"};

  // Relative altitude in meters.
  vrs::DataPieceValue<double> altitude{"altitude"};

  vrs::AutoDataLayoutEnd end;
};

} // namespace aria
