// (c) Facebook, Inc. and its affiliates. Confidential and proprietary.

#pragma once

#include <vrs/DataLayout.h>
#include <vrs/DataPieces.h>
#include <vrs/DataReference.h>
#include <vrs/FileFormat.h>
#include <vrs/StreamPlayer.h>

namespace OVR {
namespace Vision {

/**
 * This data format is for some generic 3 axis sensor. The data is already converted from some
 * internal representation to a floating point number. This can be used for data collected with a
 * mobile phone where internal representations vary a lot or are not available.
 * The type supports a correction/calibration model of the form:
 *   y = A * x + b + dT * c
 * where A is a 3x3 matrix, b is an additive bias (3x1), c is a temperature correction model (3x1)
 * and dT is the change in temperature with respect to a given calibration temperature.
 */
namespace Generic3AxisSensor {

using ::vrs::AutoDataLayout;
using ::vrs::AutoDataLayoutEnd;
using ::vrs::CurrentRecord;
using ::vrs::DataPieceValue;
using ::vrs::DataReference;
using ::vrs::Matrix3Df;
using ::vrs::Point3Dd;
using ::vrs::Point3Df;
using ::vrs::FileFormat::LittleEndian;

/**
 * This enum maps the integer `sensorType` in the configuration to a specific sensor.
 * Keep in sync with Management/Hardware/IHardware.h.
 */
enum class SensorType : int32_t {
  UNKNOWN = 0,
  PACIFIC_HMD_ACCELEROMETER,
  PACIFIC_HMD_GYROSCOPE,
  PACIFIC_HMD_MAGNETOMETER,
  PACIFIC_CONTROLLER_ACCELEROMETER,
  PACIFIC_CONTROLLER_GYROSCOPE,
  PACIFIC_CONTROLLER_MAGNETOMETER,

  MONTEREY_CAMERA,
  MONTEREY_IMU_HMD,
  MONTEREY_IMU_CONTROLLER_L,
  MONTEREY_IMU_CONTROLLER_R,

  CV1_CAMERA,
  DK2_CAMERA,
  TUZI_CAMERA,
  MIPI_QCOM_CAMERA,
  IDS_CAMERA,

  MONTEREY_MAGNETOMETER,

  // Non-Oculus sensors. We leave enough space for additions in IHardware.h
  UNSPECIFIED_GYROSCOPE = 1000,
  UNSPECIFIED_ACCELEROMETER,
  ATTITUDE_MEASUREMENT, ///< i.e. pre-filtered data from iPhones.
  UNSPECIFIED_MAGNETOMETER,
};

#pragma pack(push, 1)

struct VRSData {
  static constexpr uint32_t kDataVersion = 1;

  VRSData() {}

  bool canHandle(const CurrentRecord& record, DataReference& outDataReference) {
    if (record.formatVersion == kDataVersion && sizeof(VRSData) == record.recordSize) {
      outDataReference.useObject(*this);
      return true;
    }
    return false;
  }

  void upgradeFrom(uint32_t /*formatVersion*/) {}

  LittleEndian<double> arrivalTimestamp;
  LittleEndian<double> sampleTimestamp;
  LittleEndian<double> temperatureInCelsius;

  LittleEndian<double> measurement[3];
};

struct VRSConfigurationV1 {
  static constexpr uint32_t kConfigurationVersion = 1;

  VRSConfigurationV1() {}

  bool canHandle(const CurrentRecord& record, DataReference& outDataReference) {
    if (record.formatVersion == kConfigurationVersion &&
        sizeof(VRSConfigurationV1) == record.recordSize) {
      outDataReference.useObject(*this);
      return true;
    }
    return false;
  }

  /**
   * Use SensorType cast to integer as value here.
   */
  LittleEndian<int32_t> sensorType;

  /**
   * We assume the measurement calibration consists of a correction matrix, bias and temperature
   * correction:
   * m_real = correctionMatrix * (m_measured - bias - (T - T_cal) * temperatureCoefficients)
   * or something similar. It is up to the user of this data format to decide what to put as values
   * in here.
   */
  LittleEndian<float> bias[3];
  LittleEndian<float> correctionMatrix[3][3];
  LittleEndian<float> temperatureCoefficients[3];
  LittleEndian<float> calibrationTemperature;
};

struct VRSConfiguration : VRSConfigurationV1 {
  static const uint32_t kConfigurationVersion = 2;

  VRSConfiguration() : VRSConfigurationV1() {}

  bool canHandle(const CurrentRecord& record, DataReference& outDataReference) {
    if (record.formatVersion == kConfigurationVersion &&
        sizeof(VRSConfiguration) == record.recordSize) {
      outDataReference.useObject(*this);
      return true;
    }
    return VRSConfigurationV1::canHandle(record, outDataReference);
  }

  void upgradeFrom(uint32_t formatVersion) {
    if (formatVersion < kConfigurationVersion) {
      instanceId.set(0);
    }
  }

  /// A unique identifier to differentiate measurements from the same sensor type on a device.
  LittleEndian<uint32_t> instanceId;
};

#pragma pack(pop)

static constexpr uint32_t kDataVersion = VRSData::kDataVersion;
static constexpr uint32_t kConfigurationVersion = VRSConfiguration::kConfigurationVersion;
static const uint32_t kStateVersion = 1;

struct DataLayoutConfiguration : public AutoDataLayout {
  enum : uint32_t { kVersion = VRSConfiguration::kConfigurationVersion };

  DataPieceValue<int32_t> sensorType{"sensor_type"};
  DataPieceValue<Point3Df> bias{"bias"};
  DataPieceValue<Matrix3Df> correctionMatrix{"correction_matrix"};
  DataPieceValue<Point3Df> temperatureCoefficients{"temperature_coefficients"};
  DataPieceValue<float> calibrationTemperature{"calibration_temperature"};
  DataPieceValue<uint32_t> instanceId{"instance_id"};

  AutoDataLayoutEnd endLayout;
};

struct DataLayoutConfigurationV1 : public AutoDataLayout {
  enum : uint32_t { kVersion = VRSConfigurationV1::kConfigurationVersion };

  DataPieceValue<int32_t> sensorType{"sensor_type"};
  DataPieceValue<Point3Df> bias{"bias"};
  DataPieceValue<Matrix3Df> correctionMatrix{"correction_matrix"};
  DataPieceValue<Point3Df> temperatureCoefficients{"temperature_coefficients"};
  DataPieceValue<float> calibrationTemperature{"calibration_temperature"};

  AutoDataLayoutEnd endLayout;
};

struct DataLayoutData : public AutoDataLayout {
  enum : uint32_t { kVersion = VRSData::kDataVersion };

  DataPieceValue<double> arrivalTimestamp{"arrival_time_stamp"};
  DataPieceValue<double> sampleTimestamp{"sample_timestamp"};
  DataPieceValue<double> temperatureInCelsius{"temperature_in_celcius"};
  DataPieceValue<Point3Dd> measurement{"measurement"};

  AutoDataLayoutEnd endLayout;
};

} // namespace Generic3AxisSensor
} // namespace Vision
} // namespace OVR
