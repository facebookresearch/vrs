// Copyright (c) Facebook, Inc. and its affiliates. Confidential and proprietary.

#pragma once

#include <cstdint>

#include <vrs/DataLayout.h>
#include <vrs/DataPieces.h>

namespace aria {

struct MotionSensorConfigurationLayout : public vrs::AutoDataLayout {
  static constexpr uint32_t kVersion = 2;

  // Index to preserve order of IMU streams.
  vrs::DataPieceValue<std::uint32_t> streamIndex{"stream_index"};

  // Type, version, serial of *entire* HMD device
  vrs::DataPieceString deviceType{"device_type"};
  vrs::DataPieceString deviceVersion{"device_version"};
  vrs::DataPieceString deviceSerial{"device_serial"};

  // Index of the IMU in the calibration JSON
  vrs::DataPieceValue<std::uint64_t> deviceId{"device_id"};

  vrs::DataPieceString sensorModel{"sensor_model"};

  // Nominal IMU rate [Hz]. NaN if not available.
  vrs::DataPieceValue<double> nominalRateHz{"nominal_rate"};

  vrs::DataPieceValue<vrs::Bool> hasAccelerometer{"has_accelerometer"};
  vrs::DataPieceValue<vrs::Bool> hasGyroscope{"has_gyroscope"};
  vrs::DataPieceValue<vrs::Bool> hasMagnetometer{"has_magnetometer"};

  // Calibration (as JSON) of the *all* device sensors
  vrs::DataPieceString factoryCalibration{"factory_calibration"};
  vrs::DataPieceString onlineCalibration{"online_calibration"};

  vrs::DataPieceString description{"description"};

  vrs::AutoDataLayoutEnd end;
}; // class MotionSensorConfigurationLayout

struct MotionDataLayout : public vrs::AutoDataLayout {
  static constexpr uint32_t kVersion = 2;

  // Indication which data values are valid in this data sample.
  vrs::DataPieceValue<vrs::Bool> accelValid{"accelerometer_valid"};
  vrs::DataPieceValue<vrs::Bool> gyroValid{"gyroscope_valid"};
  vrs::DataPieceValue<vrs::Bool> magValid{"magnetometer_valid"};

  // Set to NaN if not available.
  vrs::DataPieceValue<double> temperature{"temperature_deg_c"};

  // Sample timestamp in the hardware clock domain
  vrs::DataPieceValue<std::int64_t> captureTimestampNs{"capture_timestamp_ns"};
  // Arrival timestamp in the host clock domain. Set to -1 if not available.
  vrs::DataPieceValue<std::int64_t> arrivalTimestampNs{"arrival_timestamp_ns"};

  // Accelerometer reading in [m/s^2], if accelerometer_valid is true.
  vrs::DataPieceArray<float> accelMSec2{"accelerometer", 3};
  // Gyroscope reading in [rad/s], if gyroscope_valid is true.
  vrs::DataPieceArray<float> gyroRadSec{"gyroscope", 3};
  // Magnetometer reading in [T], if magnetometer_valid is true.
  vrs::DataPieceArray<float> magTesla{"magnetometer", 3};

  vrs::AutoDataLayoutEnd end;
}; // struct MotionDataLayout

struct MotionStateLayout : public vrs::AutoDataLayout {
  static constexpr uint32_t kVersion = 2;
  vrs::AutoDataLayoutEnd end;
}; // struct MotionStateLayout

} // namespace aria
