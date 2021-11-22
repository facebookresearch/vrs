// Copyright (c) Facebook, Inc. and its affiliates. Confidential and proprietary.

#pragma once

#include <cstdint>

#include <vrs/DataLayout.h>
#include <vrs/DataPieces.h>

namespace aria {

struct GpsConfigurationLayout : public vrs::AutoDataLayout {
  static constexpr uint32_t kVersion = 1;

  vrs::DataPieceValue<std::uint32_t> streamId{"stream_id"};

  // Sample rate [Hz]
  vrs::DataPieceValue<double> sampleRateHz{"sample_rate_hz"};

  vrs::AutoDataLayoutEnd end;
};

struct GpsDataLayout : public vrs::AutoDataLayout {
  static constexpr uint32_t kVersion = 1;

  // Timestamp of capturing this sample, in nanoseconds.
  vrs::DataPieceValue<std::int64_t> captureTimestampNs{"capture_timestamp_ns"};

  // UTC time in milliseconds.
  vrs::DataPieceValue<std::int64_t> utcTimeMs{"utc_time_ms"};

  // GPS fix data: This is the calculated positional information from raw satellite data.
  // Provider is typically "gps"
  vrs::DataPieceString provider{"provider"};
  // Latitude in degrees
  vrs::DataPieceValue<float> latitude{"latitude"};
  // Longitude in degrees
  vrs::DataPieceValue<float> longitude{"longitude"};
  // Altitude in meters
  vrs::DataPieceValue<float> altitude{"altitude"};

  // Horizontal accuracy in meters
  vrs::DataPieceValue<float> accuracy{"accuracy"};

  // Speed over ground [m/s]
  vrs::DataPieceValue<float> speed{"speed"};

  // Raw data: This is the raw numerical data from each satellite. Each element is a string
  // representing the raw data sentence from one satellite. Can be parsed by any standard GPS
  // library.
  vrs::DataPieceVector<std::string> rawData{"raw_data"};

  vrs::AutoDataLayoutEnd end;
};

} // namespace aria
