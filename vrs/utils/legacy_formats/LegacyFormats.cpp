// Copyright (c) Facebook, Inc. and its affiliates. Confidential and proprietary.

#include <vrs/utils/legacy_formats/LegacyFormats.h>

#include <vrs/DataLayout.h>

#include <vrs/utils/legacy_formats/Cv1Camera.h>
#include <vrs/utils/legacy_formats/Cv1IMU.h>
#include <vrs/utils/legacy_formats/Cv1SyncPulse.h>
#include <vrs/utils/legacy_formats/DepthSensingLegacyData.h>
#include <vrs/utils/legacy_formats/FaceCameraOV9762.h>
#include <vrs/utils/legacy_formats/Generic3AxisSensor.h>
#include <vrs/utils/legacy_formats/MontereyCamera.h>
#include <vrs/utils/legacy_formats/MontereyController.h>
#include <vrs/utils/legacy_formats/MontereyIMU.h>
#include <vrs/utils/legacy_formats/MontereySyncPulse.h>

using namespace std;
using namespace vrs;

namespace OVR {
namespace Vision {

void LegacyFormats::install() {
  static bool sInstalled = false;
  if (!sInstalled) {
    RecordFormatRegistrar::getInstance().registerProvider(
        std::unique_ptr<LegacyFormatsProvider>(new LegacyFormats()));
    sInstalled = true;
  }
}

void LegacyFormats::registerLegacyRecordFormats(RecordableTypeId id) {
  switch (id) {
    case RecordableTypeId::Cv1Camera: {
      Cv1Camera::DataLayoutConfiguration CurrentCv1CameraDataLayoutConfiguration;
      addLegacyRecordFormat(
          id,
          Record::Type::CONFIGURATION,
          Cv1Camera::DataLayoutConfiguration::kConfigurationVersion,
          ContentBlock(ContentType::DATA_LAYOUT),
          {&CurrentCv1CameraDataLayoutConfiguration});

      ContentBlock rawImage(ImageFormat::RAW);
      Cv1Camera::DataLayoutData currentCv1CameraDataLayoutData;
      addLegacyRecordFormat(
          id,
          Record::Type::DATA,
          Cv1Camera::DataLayoutData::kDataVersion,
          ContentBlock(ContentType::DATA_LAYOUT) + rawImage,
          {&currentCv1CameraDataLayoutData});
    } break;

    case RecordableTypeId::Cv1IMU: {
      Cv1IMU::DataLayoutData currentCv1IMUDataLayoutData;
      addLegacyRecordFormat(
          id,
          Record::Type::DATA,
          Cv1IMU::DataLayoutData::kVersion,
          ContentBlock(ContentType::DATA_LAYOUT),
          {&currentCv1IMUDataLayoutData});
    } break;

    case RecordableTypeId::Cv1SyncPulse: {
      Cv1SyncPulse::DataLayoutData currentCv1SyncPulseDataLayoutData;
      addLegacyRecordFormat(
          id,
          Record::Type::DATA,
          Cv1SyncPulse::DataLayoutData::kVersion,
          ContentBlock(ContentType::DATA_LAYOUT),
          {&currentCv1SyncPulseDataLayoutData});
    } break;

    case RecordableTypeId::Proto0CameraHAL:
    case RecordableTypeId::Proto0CameraHALConstellation:
    case RecordableTypeId::Proto0CameraHALSlam:
    case RecordableTypeId::MontereyCameraSlamDML:
    case RecordableTypeId::MontereyCameraConstellationDML: {
      MontereyCamera::DataLayoutConfiguration CurrentMontereyCameraDataLayoutConfiguration;
      addLegacyRecordFormat(
          id,
          Record::Type::CONFIGURATION,
          MontereyCamera::DataLayoutConfiguration::kConfigurationVersion,
          ContentBlock(ContentType::DATA_LAYOUT),
          {&CurrentMontereyCameraDataLayoutConfiguration});

      ContentBlock rawImage(ImageFormat::RAW);
      MontereyCamera::DataLayoutData currentMontereyCameraDataLayoutData;
      addLegacyRecordFormat(
          id,
          Record::Type::DATA,
          MontereyCamera::DataLayoutData::kDataVersion,
          ContentBlock(ContentType::DATA_LAYOUT) + rawImage,
          {&currentMontereyCameraDataLayoutData});
      MontereyCamera::DataLayoutDataV5 v5MontereyCameraDataLayoutData;
      addLegacyRecordFormat(
          id,
          Record::Type::DATA,
          MontereyCamera::DataLayoutDataV5::kDataVersion,
          ContentBlock(ContentType::DATA_LAYOUT) + rawImage,
          {&v5MontereyCameraDataLayoutData});
      MontereyCamera::DataLayoutDataV4 v4MontereyCameraDataLayoutData;
      addLegacyRecordFormat(
          id,
          Record::Type::DATA,
          MontereyCamera::DataLayoutDataV4::kDataVersion,
          ContentBlock(ContentType::DATA_LAYOUT) + rawImage,
          {&v4MontereyCameraDataLayoutData});
      MontereyCamera::DataLayoutDataV3 v3MontereyCameraDataLayoutData;
      addLegacyRecordFormat(
          id,
          Record::Type::DATA,
          MontereyCamera::DataLayoutDataV3::kDataVersion,
          ContentBlock(ContentType::DATA_LAYOUT) + rawImage,
          {&v3MontereyCameraDataLayoutData});
      MontereyCamera::DataLayoutDataV2 v2MontereyCameraDataLayoutData;
      addLegacyRecordFormat(
          id,
          Record::Type::DATA,
          MontereyCamera::DataLayoutDataV2::kDataVersion,
          ContentBlock(ContentType::DATA_LAYOUT) + rawImage,
          {&v2MontereyCameraDataLayoutData});
    } break;

    case RecordableTypeId::Proto0IMUHAL:
    case RecordableTypeId::MontereyIMUDML: {
      MontereyIMU::DataLayoutConfiguration currentMontereyIMUDataLayoutConfiguration;
      addLegacyRecordFormat(
          id,
          Record::Type::CONFIGURATION,
          MontereyIMU::DataLayoutConfiguration::kVersion,
          ContentBlock(ContentType::DATA_LAYOUT),
          {&currentMontereyIMUDataLayoutConfiguration});
      MontereyIMU::DataLayoutConfigurationV4 v4IMUDataLayoutConfiguration;
      addLegacyRecordFormat(
          id,
          Record::Type::CONFIGURATION,
          MontereyIMU::DataLayoutConfigurationV4::kVersion,
          ContentBlock(ContentType::DATA_LAYOUT),
          {&v4IMUDataLayoutConfiguration});

      MontereyIMU::DataLayoutData currentMontereyIMUDataLayoutData;
      addLegacyRecordFormat(
          id,
          Record::Type::DATA,
          MontereyIMU::DataLayoutData::kVersion,
          ContentBlock(ContentType::DATA_LAYOUT),
          {&currentMontereyIMUDataLayoutData});
      MontereyIMU::DataLayoutDataV3 v3IMUDataLayoutData;
      addLegacyRecordFormat(
          id,
          Record::Type::DATA,
          MontereyIMU::DataLayoutDataV3::kVersion,
          ContentBlock(ContentType::DATA_LAYOUT),
          {&v3IMUDataLayoutData});
      MontereyIMU::DataLayoutDataV2 v2IMUDataLayoutData;
      addLegacyRecordFormat(
          id,
          Record::Type::DATA,
          MontereyIMU::DataLayoutDataV2::kVersion,
          ContentBlock(ContentType::DATA_LAYOUT),
          {&v2IMUDataLayoutData});
    } break;

    case RecordableTypeId::Proto0ControllerHAL:
    case RecordableTypeId::MontereyControllerDML: {
      MontereyController::DataLayoutConfiguration currentMontereyControllerDataLayoutConfiguration;
      addLegacyRecordFormat(
          id,
          Record::Type::CONFIGURATION,
          MontereyController::DataLayoutConfiguration::kVersion,
          ContentBlock(ContentType::DATA_LAYOUT),
          {&currentMontereyControllerDataLayoutConfiguration});
      MontereyController::DataLayoutConfigurationV1 v1DataLayoutConfiguration;
      addLegacyRecordFormat(
          id,
          Record::Type::CONFIGURATION,
          MontereyController::DataLayoutConfigurationV1::kVersion,
          ContentBlock(ContentType::DATA_LAYOUT),
          {&v1DataLayoutConfiguration});

      MontereyController::DataLayoutData currentMontereyControllerDataLayoutData;
      addLegacyRecordFormat(
          id,
          Record::Type::DATA,
          MontereyController::DataLayoutData::kVersion,
          ContentBlock(ContentType::DATA_LAYOUT),
          {&currentMontereyControllerDataLayoutData});
    } break;

    case RecordableTypeId::Proto0SyncPulseHAL:
    case RecordableTypeId::Proto0SyncPulseDML: {
      MontereySyncPulse::Proto0SyncPulseDataLayoutData proto0SyncPulseDataLayoutData;
      addLegacyRecordFormat(
          id,
          Record::Type::DATA,
          MontereySyncPulse::Proto0SyncPulseDataLayoutData::kVersion,
          ContentBlock(ContentType::DATA_LAYOUT),
          {&proto0SyncPulseDataLayoutData});
    } break;

    case RecordableTypeId::Proto0ControllerSyncPulseHAL:
    case RecordableTypeId::MontereySyncPulseDML: {
      MontereySyncPulse::DataLayoutData currentMontereySyncPulseDataLayoutData;
      addLegacyRecordFormat(
          id,
          Record::Type::DATA,
          MontereySyncPulse::DataLayoutData::kVersion,
          ContentBlock(ContentType::DATA_LAYOUT),
          {&currentMontereySyncPulseDataLayoutData});
    } break;

    case RecordableTypeId::Generic3AxisSensor: {
      Generic3AxisSensor::DataLayoutConfiguration currentGeneric3AxisSensorDataLayoutConfiguration;
      addLegacyRecordFormat(
          id,
          Record::Type::CONFIGURATION,
          Generic3AxisSensor::DataLayoutConfiguration::kVersion,
          ContentBlock(ContentType::DATA_LAYOUT),
          {&currentGeneric3AxisSensorDataLayoutConfiguration});
      Generic3AxisSensor::DataLayoutConfigurationV1 v1Generic3AxisSensorDataLayoutConfiguration;
      addLegacyRecordFormat(
          id,
          Record::Type::CONFIGURATION,
          Generic3AxisSensor::DataLayoutConfigurationV1::kVersion,
          ContentBlock(ContentType::DATA_LAYOUT),
          {&v1Generic3AxisSensorDataLayoutConfiguration});

      Generic3AxisSensor::DataLayoutData currentGeneric3AxisSensorDataLayoutData;
      addLegacyRecordFormat(
          id,
          Record::Type::DATA,
          Generic3AxisSensor::DataLayoutData::kVersion,
          ContentBlock(ContentType::DATA_LAYOUT),
          {&currentGeneric3AxisSensorDataLayoutData});
    } break;

    case RecordableTypeId::FaceCameraOV9762: {
      FaceCameraOV9762::DataLayoutConfiguration currentFaceCameraOV9762DataLayoutConfiguration;
      addLegacyRecordFormat(
          id,
          Record::Type::CONFIGURATION,
          FaceCameraOV9762::DataLayoutConfiguration::kVersion,
          ContentBlock(ContentType::DATA_LAYOUT),
          {&currentFaceCameraOV9762DataLayoutConfiguration});
      FaceCameraOV9762::DataLayoutConfigurationLegacy legacyFaceCameraOV9762DataLayoutConfiguration;
      addLegacyRecordFormat(
          id,
          Record::Type::CONFIGURATION,
          FaceCameraOV9762::DataLayoutConfigurationLegacy::kVersion,
          ContentBlock(ContentType::DATA_LAYOUT),
          {&legacyFaceCameraOV9762DataLayoutConfiguration});

      ContentBlock rawImage(ImageFormat::RAW);
      FaceCameraOV9762::DataLayoutData currentFaceCameraOV9762DataLayoutData;
      addLegacyRecordFormat(
          id,
          Record::Type::DATA,
          FaceCameraOV9762::DataLayoutData::kVersion,
          ContentBlock(ContentType::DATA_LAYOUT) + rawImage,
          {&currentFaceCameraOV9762DataLayoutData});
      FaceCameraOV9762::DataLayoutDataLegacy legacyFaceCameraOV9762DataLayoutData;
      addLegacyRecordFormat(
          id,
          Record::Type::DATA,
          FaceCameraOV9762::DataLayoutDataLegacy::kVersion,
          ContentBlock(ContentType::DATA_LAYOUT) + rawImage,
          {&legacyFaceCameraOV9762DataLayoutData});
    } break;

    case RecordableTypeId::DepthSensing: {
      DepthSensingCamera::LegacyConfiguration legacyDepthSensingCameraLegacyConfiguration;
      addLegacyRecordFormat(
          id,
          Record::Type::CONFIGURATION,
          DepthSensingCamera::LegacyConfiguration::kVersion,
          ContentBlock(ContentType::DATA_LAYOUT),
          {&legacyDepthSensingCameraLegacyConfiguration});

      ContentBlock rawImage(ImageFormat::RAW);
      DepthSensingCamera::LegacyData legacyDepthSensingCameraLegacyData;
      addLegacyRecordFormat(
          id,
          Record::Type::DATA,
          DepthSensingCamera::LegacyData::kVersion,
          ContentBlock(ContentType::DATA_LAYOUT) + rawImage,
          {&legacyDepthSensingCameraLegacyData});
    } break;

    default: // do nothing
      break;
  }
}

} // namespace Vision
} // namespace OVR
