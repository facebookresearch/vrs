// Facebook Technologies, LLC Proprietary and Confidential.

#pragma once

#include <vrs/DataLayout.h>
#include <vrs/DataPieces.h>
#include <vrs/DataReference.h>
#include <vrs/FileFormat.h>
#include <vrs/StreamPlayer.h>

namespace OVR {
namespace Vision {

namespace MontereyController {

using ::vrs::AutoDataLayout;
using ::vrs::AutoDataLayoutEnd;
using ::vrs::Bool;
using ::vrs::CurrentRecord;
using ::vrs::DataPieceArray;
using ::vrs::DataPieceValue;
using ::vrs::DataReference;
using ::vrs::Matrix4Df;
using ::vrs::Point2Di;
using ::vrs::Point3Df;
using ::vrs::Point4Df;
using ::vrs::FileFormat::LittleEndian;

constexpr uint32_t kStateVersion = 1;
constexpr const uint32_t kLengthOfSerial = 16;

#pragma pack(push, 1)

struct VRSData {
  static constexpr uint32_t kDataVersion = 1;

  LittleEndian<double> appReadTimestamp;
  LittleEndian<uint32_t> sampleTimestamp;
  LittleEndian<double> sampleTimestampInSeconds;

  LittleEndian<float> accelXFloat;
  LittleEndian<float> accelYFloat;
  LittleEndian<float> accelZFloat;
  LittleEndian<float> gyroXFfloat;
  LittleEndian<float> gyroYFfloat;
  LittleEndian<float> gyroZFfloat;

  LittleEndian<uint32_t> controllerType;
  LittleEndian<uint8_t> touch;
  LittleEndian<uint8_t> gesture;
  LittleEndian<uint32_t> touchX;
  LittleEndian<uint32_t> touchY;
  LittleEndian<float> temperature;

  LittleEndian<bool> buttonTrigger;
  LittleEndian<bool> buttonBack;
  LittleEndian<bool> buttonHome;
  LittleEndian<bool> buttonTouch;

  bool canHandle(const CurrentRecord& record, DataReference& outDataReference) {
    uint32_t payloadSize = record.recordSize;
    if (record.formatVersion == kDataVersion && sizeof(VRSData) == payloadSize) {
      // The data was written with padding or the vtable pointer:
      // read everything, and we will ignore the padding/vtable data
      outDataReference.useObject(*this);
      return true;
    }
    return false;
  }

  void upgradeFrom(uint32_t /* formatVersion */) {
    // Nothing to do yet
  }
};

struct VRSConfigurationV1 {
  static constexpr uint32_t kConfigurationVersion = 1;
  VRSConfigurationV1() {}

  LittleEndian<uint8_t> serial[60];
  LittleEndian<uint16_t> serialLength;
  LittleEndian<uint8_t> config[5066];
  LittleEndian<uint16_t> configLength;
  LittleEndian<float> accelFactor;
  LittleEndian<float> gyroFactor;
  LittleEndian<double> rate;
  LittleEndian<float> accelOffset[3];
  LittleEndian<float> gyroOffset[3];
  LittleEndian<float> accelMatrix[4][4];
  LittleEndian<float> gyroMatrix[4][4];
  LittleEndian<float> temperature;
  LittleEndian<float> imuPosition[3];
  LittleEndian<float> modelPoints[64][9];
  LittleEndian<bool> hasImuPosition;
  LittleEndian<uint8_t> numOfModelPoints;
  LittleEndian<uint32_t> controllerType;

  bool canHandle(const CurrentRecord& record, DataReference& outDataReference) {
    uint32_t formatVersion = record.formatVersion;
    uint32_t payloadSize = record.recordSize;
    if ((formatVersion == kConfigurationVersion && sizeof(VRSConfigurationV1) == payloadSize)) {
      outDataReference.useObject(*this);
      return true;
    }
    return false;
  }
};

struct VRSConfiguration : VRSConfigurationV1 {
  static constexpr uint32_t kConfigurationVersion = 2;

  VRSConfiguration() : VRSConfigurationV1() {}

  void reset() {
    memset(serial, 0, sizeof(serial));
    memset(config, 0, sizeof(config));
    serialLength.set(0);
    configLength.set(0);
    accelFactor.set(0);
    gyroFactor.set(0);
    rate.set(0);
    for (int i = 0; i < 3; i++) {
      accelOffset[i].set(0);
      gyroOffset[i].set(0);
      imuPosition[i].set(0);
    }
    for (int i = 0; i < 4; i++) {
      for (int j = 0; j < 4; j++) {
        accelMatrix[i][j].set(0);
        gyroMatrix[i][j].set(0);
      }
    }
    temperature.set(0);
    for (int i = 0; i < 64; i++) {
      for (int j = 0; j < 9; j++) {
        modelPoints[i][j].set(0);
      }
    }
    hasImuPosition.set(false);
    numOfModelPoints.set(0);
    imuType.set(0);
  }

  bool isSet() {
    return numOfModelPoints.get() != 0;
  }

  bool canHandle(const CurrentRecord& record, DataReference& outDataReference) {
    uint32_t formatVersion = record.formatVersion;
    uint32_t payloadSize = record.recordSize;
    if ((formatVersion == kConfigurationVersion && sizeof(VRSConfiguration) == payloadSize)) {
      outDataReference.useObject(*this);
      return true;
    }
    return VRSConfigurationV1::canHandle(record, outDataReference);
  }

  void upgradeFrom(uint32_t formatVersion) {
    if (formatVersion < kConfigurationVersion) {
      imuType.set(0);
    }
  }

  LittleEndian<uint32_t> imuType;
};

#pragma pack(pop)

static const uint32_t kConfigurationVersion = VRSConfiguration::kConfigurationVersion;

class DataLayoutConfigurationV1 : public AutoDataLayout {
 public:
  enum : uint32_t { kVersion = 1 };

  DataPieceArray<uint8_t> serial{"serial", 60};
  DataPieceValue<uint16_t> serialLength{"serial_length"};
  DataPieceArray<uint8_t> config{"configuration", 5066};
  DataPieceValue<uint16_t> configLength{"configuration_length"};
  DataPieceValue<float> accelFactor{"acceleration_factor"};
  DataPieceValue<float> gyroFactor{"gyro_factor"};
  DataPieceValue<double> rate{"rate"};
  DataPieceValue<Point3Df> accelOffset{"acceleration_offset"};
  DataPieceValue<Point3Df> gyroOffset{"gyro_offset"};
  DataPieceValue<Matrix4Df> accelMatrix{"acceleration_matrix"};
  DataPieceValue<Matrix4Df> gyroMatrix{"gyro_matrix"};
  DataPieceValue<float> temperature{"temperature"};
  DataPieceValue<Point3Df> imuPosition{"imu_position"};
  DataPieceArray<Point3Df> modelPoints{"model_points", 64 * 3}; // was [64][9] float
  DataPieceValue<Bool> hasImuPosition{"has_imu_position"};
  DataPieceValue<uint8_t> numOfModelPoints{"number_of_model_points"};
  DataPieceValue<uint32_t> controllerType{"controller_type"};

  AutoDataLayoutEnd endLayout;
};

class DataLayoutConfiguration : public AutoDataLayout {
 public:
  enum : uint32_t { kVersion = 2 };

  // v1
  DataPieceArray<uint8_t> serial{"serial", 60};
  DataPieceValue<uint16_t> serialLength{"serial_length"};
  DataPieceArray<uint8_t> config{"configuration", 5066};
  DataPieceValue<uint16_t> configLength{"configuration_length"};
  DataPieceValue<float> accelFactor{"acceleration_factor"};
  DataPieceValue<float> gyroFactor{"gyro_factor"};
  DataPieceValue<double> rate{"rate"};
  DataPieceValue<Point3Df> accelOffset{"acceleration_offset"};
  DataPieceValue<Point3Df> gyroOffset{"gyro_offset"};
  DataPieceValue<Matrix4Df> accelMatrix{"acceleration_matrix"};
  DataPieceValue<Matrix4Df> gyroMatrix{"gyro_matrix"};
  DataPieceValue<float> temperature{"temperature"};
  DataPieceValue<Point3Df> imuPosition{"imu_position"};
  DataPieceArray<Point3Df> modelPoints{"model_points", 64 * 3}; // was [64][9] float
  DataPieceValue<Bool> hasImuPosition{"has_imu_position"};
  DataPieceValue<uint8_t> numOfModelPoints{"number_of_model_points"};
  DataPieceValue<uint32_t> controllerType{"controller_type"};
  // added in v2
  DataPieceValue<uint32_t> imuType{"imu_type"};

  AutoDataLayoutEnd endLayout;
};

class DataLayoutData : public AutoDataLayout {
 public:
  enum : uint32_t { kVersion = 1 };

  DataPieceValue<double> appReadTimestamp{"app_read_timestamp"};
  DataPieceValue<uint32_t> sampleTimestamp{"sample_timestamp"};
  DataPieceValue<double> sampleTimestampInSeconds{"sample_timestamp_in_seconds"};

  DataPieceValue<Point3Df> accelFloat{"acceleration"};
  DataPieceValue<Point3Df> gyroXFfloat{"gyro"};

  DataPieceValue<uint32_t> controllerType{"controller_type"};
  DataPieceValue<uint8_t> touch{"touch"};
  DataPieceValue<uint8_t> gesture{"gesture"};
  DataPieceValue<uint32_t> touchX{"touch_x"};
  DataPieceValue<uint32_t> touchY{"touch_y"};
  DataPieceValue<float> temperature{"temperature"};

  DataPieceValue<Bool> buttonTrigger{"trigger_button"};
  DataPieceValue<Bool> buttonBack{"back_button"};
  DataPieceValue<Bool> buttonHome{"home_button"};
  DataPieceValue<Bool> buttonTouch{"touch_button"};

  AutoDataLayoutEnd endLayout;
};

} // namespace MontereyController
} // namespace Vision
} // namespace OVR
