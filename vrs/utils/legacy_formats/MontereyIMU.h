// Facebook Technologies, LLC Proprietary and Confidential.

#pragma once

#include <vrs/DataLayout.h>
#include <vrs/DataPieces.h>
#include <vrs/DataReference.h>
#include <vrs/FileFormat.h>
#include <vrs/StreamPlayer.h>

namespace OVR {
namespace Vision {

namespace MontereyIMU {

using ::vrs::AutoDataLayout;
using ::vrs::AutoDataLayoutEnd;
using ::vrs::Bool;
using ::vrs::CurrentRecord;
using ::vrs::DataPieceArray;
using ::vrs::DataPieceValue;
using ::vrs::DataReference;
using ::vrs::Matrix4Df;
using ::vrs::Point3Df;
using ::vrs::Point3Di;
using ::vrs::Point4Df;
using ::vrs::FileFormat::LittleEndian;

constexpr uint32_t kStateVersion = 1;
constexpr uint32_t kLengthOfSerial = 16;

#pragma pack(push, 1)

struct VRSDataV2 {
  static const uint32_t kDataVersion = 2;
  VRSDataV2() {}

  LittleEndian<double> appReadTimestamp;
  LittleEndian<uint32_t> sampleTimestamp;
  LittleEndian<double> sampleTimestampInSeconds;
  // temperature used to be in int32_t.
  // Updated to float and updated upgradeFrom methods accordingly.
  LittleEndian<float> temperature;
  LittleEndian<uint32_t> runningSampleCount;
  LittleEndian<uint32_t> numSamples;

  LittleEndian<int32_t> deprecatedAccel1X;
  LittleEndian<int32_t> deprecatedAccel1Y;
  LittleEndian<int32_t> deprecatedAccel1Z;
  LittleEndian<int32_t> deprecatedGyro1X;
  LittleEndian<int32_t> deprecatedGyro1Y;
  LittleEndian<int32_t> deprecatedGyro1Z;

  LittleEndian<int32_t> deprecatedAccel2X;
  LittleEndian<int32_t> deprecatedAccel2Y;
  LittleEndian<int32_t> deprecatedAccel2Z;
  LittleEndian<int32_t> deprecatedGyro2X;
  LittleEndian<int32_t> deprecatedGyro2Y;
  LittleEndian<int32_t> deprecatedGyro2Z;
};

struct VRSDataV3 : VRSDataV2 {
  static constexpr uint32_t kDataVersion = 3;

  void upgradeFrom(uint32_t formatVersion) {
    if (formatVersion < kDataVersion) {
      accel1X_f.set(deprecatedAccel1X.get() * 1e-4f);
      accel1Y_f.set(deprecatedAccel1Y.get() * 1e-4f);
      accel1Z_f.set(deprecatedAccel1Z.get() * 1e-4f);
      gyro1X_f.set(deprecatedGyro1X.get() * 1e-4f);
      gyro1Y_f.set(deprecatedGyro1Y.get() * 1e-4f);
      gyro1Z_f.set(deprecatedGyro1Z.get() * 1e-4f);
      accel2X_f.set(deprecatedAccel2X.get() * 1e-4f);
      accel2Y_f.set(deprecatedAccel2Y.get() * 1e-4f);
      accel2Z_f.set(deprecatedAccel2Z.get() * 1e-4f);
      gyro2X_f.set(deprecatedGyro2X.get() * 1e-4f);
      gyro2Y_f.set(deprecatedGyro2Y.get() * 1e-4f);
      gyro2Z_f.set(deprecatedGyro2Z.get() * 1e-4f);
    }
  }

  LittleEndian<float> accel1X_f;
  LittleEndian<float> accel1Y_f;
  LittleEndian<float> accel1Z_f;
  LittleEndian<float> gyro1X_f;
  LittleEndian<float> gyro1Y_f;
  LittleEndian<float> gyro1Z_f;

  LittleEndian<float> accel2X_f;
  LittleEndian<float> accel2Y_f;
  LittleEndian<float> accel2Z_f;
  LittleEndian<float> gyro2X_f;
  LittleEndian<float> gyro2Y_f;
  LittleEndian<float> gyro2Z_f;
};

struct VRSData : VRSDataV3 {
  static constexpr uint32_t kDataVersion = 4;
  bool canHandle(const CurrentRecord& record, DataReference& outDataReference) {
    uint32_t formatVersion = record.formatVersion;
    uint32_t payloadSize = record.recordSize;
    if ((formatVersion == kDataVersion && sizeof(VRSData) == payloadSize) ||
        (formatVersion == VRSDataV2::kDataVersion && sizeof(VRSDataV2) == payloadSize) ||
        (formatVersion == VRSDataV3::kDataVersion && sizeof(VRSDataV3) == payloadSize)) {
      outDataReference.useRawData(this, payloadSize);
      return true;
    }
    return false;
  }

  void upgradeFrom(uint32_t formatVersion) {
    if (formatVersion < kDataVersion) {
      VRSDataV3::upgradeFrom(formatVersion);
      // temperature used to be in int32_t.
      // Updated to float and updated upgradeFrom methods accordingly
      LittleEndian<int32_t> intTemperature;
      memcpy((char*)&intTemperature, (char*)&temperature, sizeof(temperature));
      // Scale the temperature since it used to be stored in centiDegrees.
      temperature.set(intTemperature.get() / 100.f);
    }
  }
};

struct VRSConfigurationV4 {
  static constexpr uint32_t kConfigurationVersion = 4;

  VRSConfigurationV4() {}

  bool canHandle(const CurrentRecord& record, DataReference& outDataReference) {
    const uint32_t formatVersion = record.formatVersion;
    const uint32_t payloadSize = record.recordSize;
    if (formatVersion == kConfigurationVersion && sizeof(VRSConfigurationV4) == payloadSize) {
      outDataReference.useObject(*this);
      return true;
    }
    return false;
  }

  LittleEndian<float> accelFactor;
  LittleEndian<float> gyroFactor;
  LittleEndian<double> rate;
  LittleEndian<float> accelOffset[3];
  LittleEndian<float> gyroOffset[3];
  LittleEndian<float> accelMatrix[4][4];
  LittleEndian<float> gyroMatrix[4][4];
  LittleEndian<float> temperature;
  LittleEndian<float> accelTemperatureCoefficientsX;
  LittleEndian<float> accelTemperatureCoefficientsY;
  LittleEndian<float> accelTemperatureCoefficientsZ;
  LittleEndian<uint8_t> serial[kLengthOfSerial];
};

struct VRSConfiguration : VRSConfigurationV4 {
  static constexpr uint32_t kConfigurationVersion = 5;

  VRSConfiguration() : VRSConfigurationV4() {}

  /**
   * @brief Checks whether the record format version and payload size is compatible with
   *        this version of the configuration.
   * @param[in] record The record of the stream player to check for compatibility.
   * @param[out] outDataReference The data reference will be set to the correct version. E.g. when
   *                              the stream player is version 4 it will set the reference to the
   *                              version 4 of this object. Afterwards reading in the data
   *                              'upgradeFrom(4)' should be called.
   *                              If the function returns false this is not set.
   * @return True if the record is compatible.
   */
  bool canHandle(const CurrentRecord& record, DataReference& outDataReference) {
    const uint32_t formatVersion = record.formatVersion;
    const uint32_t payloadSize = record.recordSize;
    if (formatVersion == kConfigurationVersion && sizeof(VRSConfiguration) == payloadSize) {
      outDataReference.useObject(*this);
      return true;
    }
    return VRSConfigurationV4::canHandle(record, outDataReference);
  }

  void upgradeFrom(uint32_t formatVersion) {
    if (formatVersion < kConfigurationVersion) {
      accelerometerPositionInDevice[0].set(0.016570f);
      accelerometerPositionInDevice[1].set(0.034500f);
      accelerometerPositionInDevice[2].set(-0.058074f);
    }
  }

  LittleEndian<float> accelerometerPositionInDevice[3];
};

#pragma pack(pop)

constexpr uint32_t kConfigurationVersion = VRSConfiguration::kConfigurationVersion;

struct DataLayoutDataV2 : public AutoDataLayout {
  enum : uint32_t { kVersion = 2 };

  DataPieceValue<double> appReadTimestamp{"app_read_timestamp"};
  DataPieceValue<uint32_t> sampleTimestamp{"sample_timestamp"};
  DataPieceValue<double> sampleTimestampInSeconds{"sample_timestamp_in_seconds"};

  DataPieceValue<int32_t> temperature{"temperature"};
  DataPieceValue<uint32_t> runningSampleCount{"running_sample_count"};
  DataPieceValue<uint32_t> numSamples{"num_samples"};

  DataPieceValue<Point3Di> accel1{"acceleration_1"};
  DataPieceValue<Point3Di> gyro1{"gyro_1"};

  DataPieceValue<Point3Di> accel2{"acceleration_2"};
  DataPieceValue<Point3Di> gyro2{"gyro_2"};

  AutoDataLayoutEnd endLayout;
};

struct DataLayoutDataV3 : public AutoDataLayout {
  enum : uint32_t { kVersion = 3 };

  // V2
  DataPieceValue<double> appReadTimestamp{"app_read_timestamp"};
  DataPieceValue<uint32_t> sampleTimestamp{"sample_timestamp"};
  DataPieceValue<double> sampleTimestampInSeconds{"sample_timestamp_in_seconds"};

  DataPieceValue<int32_t> temperature{"temperature"};
  DataPieceValue<uint32_t> runningSampleCount{"running_sample_count"};
  DataPieceValue<uint32_t> numSamples{"num_samples"};

  DataPieceValue<Point3Di> deprecatedAccel1{"acceleration_1"};
  DataPieceValue<Point3Di> deprecatedGyro1{"gyro_1"};

  DataPieceValue<Point3Di> deprecatedAccel2{"acceleration_2"};
  DataPieceValue<Point3Di> deprecatedGyro2{"gyro_2"};

  // V3
  DataPieceValue<Point3Df> accel1{"acceleration_1"};
  DataPieceValue<Point3Df> gyro1{"gyro_1"};

  DataPieceValue<Point3Df> accel2{"acceleration_2"};
  DataPieceValue<Point3Df> gyro2{"gyro_2"};

  AutoDataLayoutEnd endLayout;
};

struct DataLayoutData : public AutoDataLayout {
  enum : uint32_t { kVersion = 4 };

  // V2
  DataPieceValue<double> appReadTimestamp{"app_read_timestamp"};
  DataPieceValue<uint32_t> sampleTimestamp{"sample_timestamp"};
  DataPieceValue<double> sampleTimestampInSeconds{"sample_timestamp_in_seconds"};

  DataPieceValue<float> temperature{"temperature"}; // *** switched from int32_t to float ****
  DataPieceValue<uint32_t> runningSampleCount{"running_sample_count"};
  DataPieceValue<uint32_t> numSamples{"num_samples"};

  DataPieceValue<Point3Di> deprecatedAccel1{"acceleration_1"};
  DataPieceValue<Point3Di> deprecatedGyro1{"gyro_1"};

  DataPieceValue<Point3Di> deprecatedAccel2{"acceleration_2"};
  DataPieceValue<Point3Di> deprecatedGyro2{"gyro_2"};

  // V3
  DataPieceValue<Point3Df> accel1{"acceleration_1"};
  DataPieceValue<Point3Df> gyro1{"gyro_1"};

  DataPieceValue<Point3Df> accel2{"acceleration_2"};
  DataPieceValue<Point3Df> gyro2{"gyro_2"};

  // V4: changed the type of temperature!

  AutoDataLayoutEnd endLayout;
};

struct DataLayoutConfigurationV4 : public AutoDataLayout {
  enum : uint32_t { kVersion = 4 };

  DataPieceValue<float> accelFactor{"acceleration_factor"};
  DataPieceValue<float> gyroFactor{"gyro_factor"};
  DataPieceValue<double> rate{"rate"};
  DataPieceValue<Point3Df> accelOffset{"acceleration_offset"};
  DataPieceValue<Point3Df> gyroOffset{"gyro_offset"};
  DataPieceValue<Matrix4Df> accelMatrix{"acceleration_matrix"};
  DataPieceValue<Matrix4Df> gyroMatrix{"gyro_matrix"};
  DataPieceValue<float> temperature{"temperature"};
  DataPieceValue<Point3Df> accelTemperatureCoefficients{"acceleration_temperature_coefficients"};
  DataPieceArray<uint8_t> serial{"serial_number", kLengthOfSerial};

  AutoDataLayoutEnd endLayout;
};

struct DataLayoutConfiguration : public AutoDataLayout {
  enum : uint32_t { kVersion = 5 };

  // v4
  DataPieceValue<float> accelFactor{"acceleration_factor"};
  DataPieceValue<float> gyroFactor{"gyro_factor"};
  DataPieceValue<double> rate{"rate"};
  DataPieceValue<Point3Df> accelOffset{"acceleration_offset"};
  DataPieceValue<Point3Df> gyroOffset{"gyro_offset"};
  DataPieceValue<Matrix4Df> accelMatrix{"acceleration_matrix"};
  DataPieceValue<Matrix4Df> gyroMatrix{"gyro_matrix"};
  DataPieceValue<float> temperature{"temperature"};
  DataPieceValue<Point3Df> accelTemperatureCoefficients{"acceleration_temperature_coefficients"};
  DataPieceArray<uint8_t> serial{"serial_number", kLengthOfSerial};
  // added in v5
  DataPieceValue<Point3Df> accelerometerPositionInDevice{"accelerometer_position_in_device"};

  AutoDataLayoutEnd endLayout;
};

} // namespace MontereyIMU
} // namespace Vision
} // namespace OVR
