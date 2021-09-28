// Copyright (c) Facebook, Inc. and its affiliates. Confidential and proprietary.

#include "StreamId.h"

#include <map>
#include <string>

#include <fmt/format.h>

using namespace std;

namespace vrs {

namespace {

const map<RecordableTypeId, const char*>& getRecordableTypeIdRegistry() {
  static const map<RecordableTypeId, const char*> sRegistry = {
      {RecordableTypeId::Undefined, "Undefined"},
      {RecordableTypeId::VRSIndex, "VRS Index"}, // should probably not happen
      {RecordableTypeId::VRSDescription, "VRS Description"}, // should probably not happen

      {RecordableTypeId::ImageStream, "Generic Image Stream"},
      {RecordableTypeId::AudioStream, "Generic Audio Stream"},
      {RecordableTypeId::AnnotationStream, "Generic Annotation Stream"},
      {RecordableTypeId::ArchiveStream, "Archive Stream"},

      /// Recordable Class Ids -- start
      /// << Cameras >>
      {RecordableTypeId::ForwardCameraRecordableClass, "Forward Camera Class"},
      {RecordableTypeId::UpwardCameraRecordableClass, "Upward Camera Class"},
      {RecordableTypeId::DownwardCameraRecordableClass, "Downward Camera Class"},
      {RecordableTypeId::BackwardCameraRecordableClass, "Backward Camera Class"},
      {RecordableTypeId::SidewardCameraRecordableClass, "Sideward Camera Class"},
      {RecordableTypeId::OutwardCameraRecordableClass, "Outward Camera Class"},
      {RecordableTypeId::InwardCameraRecordableClass, "Inward Camera Class"},
      {RecordableTypeId::InsideOutCameraRecordableClass, "Inside Out Camera Class"},
      {RecordableTypeId::OutsideInCameraRecordableClass, "Outside In Camera Class"},
      {RecordableTypeId::DepthCameraRecordableClass, "Depth Camera Class"},
      {RecordableTypeId::IRCameraRecordableClass, "IR Camera Class"},
      {RecordableTypeId::EyeCameraRecordableClass, "Eye Camera Class"},
      {RecordableTypeId::FaceCameraRecordableClass, "Face Camera Class"},
      {RecordableTypeId::MouthCameraRecordableClass, "Mouth Camera Class"},
      {RecordableTypeId::RgbCameraRecordableClass, "RGB Camera Class"},

      /// << Microphones >>
      {RecordableTypeId::MonoAudioRecordableClass, "Mono Audio Class"},
      {RecordableTypeId::StereoAudioRecordableClass, "Stereo Audio Class"},
      {RecordableTypeId::AmbientAudioRecordableClass, "Ambient Audio Class"},

      /// << Discrete Sensors >>
      {RecordableTypeId::SensorRecordableClass, "Sensor Data Class"},
      {RecordableTypeId::ImuRecordableClass, "IMU Data Class"},
      {RecordableTypeId::AccelerometerRecordableClass, "Accelerometer Data Class"},
      {RecordableTypeId::MagnetometerRecordableClass, "Magnetometer Data Class"},
      {RecordableTypeId::GyroscopeRecordableClass, "Gyroscope Data Class"},
      {RecordableTypeId::LidarRecordableClass, "Lidar Data Class"},
      {RecordableTypeId::TemperatureRecordableClass, "Temperature Data Class"},
      {RecordableTypeId::BarometerRecordableClass, "Barometer Data Class"},
      {RecordableTypeId::PhotoplethysmogramRecordableClass, "Photoplethysmogram (PPG) Data Class"},

      /// << Calibration, Setup, Diagnostic, etc >>
      {RecordableTypeId::CalibrationRecordableClass, "Calibration Data Class"},
      {RecordableTypeId::AlignmentRecordableClass, "Alignment Data Class"},
      {RecordableTypeId::SetupRecordableClass, "Setup Data Class"},
      {RecordableTypeId::DiagnosticRecordableClass, "Diagnostic Data Class"},
      {RecordableTypeId::PerformanceRecordableClass, "Performance Data Class"},
      {RecordableTypeId::IlluminationRecordableClass, "Illumination Data Class"},

      /// << External Signals >>
      {RecordableTypeId::SyncRecordableClass, "Sync Data Class"},
      {RecordableTypeId::GpsRecordableClass, "GPS Data Class"},
      {RecordableTypeId::WifiBeaconRecordableClass, "Wifi Beacon Data Class"},
      {RecordableTypeId::BluetoothBeaconRecordableClass, "Bluetooth Beacon Data Class"},
      {RecordableTypeId::UsbRecordableClass, "USB Data Class"},
      {RecordableTypeId::TimeRecordableClass, "Time Domain Mapping Class"},
      {RecordableTypeId::AttentionRecordableClass, "Attention Data Class"},

      /// << User Input >>
      {RecordableTypeId::InputRecordableClass, "Input Data Class"},
      {RecordableTypeId::TextInputRecordableClass, "Text Input Data Class"},
      {RecordableTypeId::MouseRecordableClass, "Mouse Data Class"},
      {RecordableTypeId::TouchInputRecordableClass, "Touch Input Data Class"},
      {RecordableTypeId::GestureInputRecordableClass, "Gesture Input Data Class"},
      {RecordableTypeId::ControllerRecordableClass, "Controller Data Class"},

      /// << Events, commands, instructions, etc >>
      {RecordableTypeId::EventRecordableClass, "Event Data Class"},
      {RecordableTypeId::CommandRecordableClass, "Command Data Class"},
      {RecordableTypeId::InstructionRecordableClass, "Instruction Data Class"},
      {RecordableTypeId::ScriptRecordableClass, "Script Data Class"},
      {RecordableTypeId::ControlRecordableClass, "Control Data Class"},

      /// << Ground Truth >>
      {RecordableTypeId::GroundTruthRecordableClass, "Ground Truth Data Class"},
      {RecordableTypeId::GroundTruthImuRecordableClass, "Ground Truth IMU Data Class"},
      {RecordableTypeId::GroundTruthAlignmentRecordableClass, "Ground Truth Alignment Data Class"},
      {RecordableTypeId::GroundTruthPositionRecordableClass, "Ground Truth Position Data Class"},
      {RecordableTypeId::GroundTruthOrientationRecordableClass,
       "Ground Truth Orientation Data Class"},
      {RecordableTypeId::GroundTruthDepthRecordableClass, "Ground Truth Depth Data Class"},

      /// << Results of all kinds >>
      {RecordableTypeId::ResultRecordableClass, "Result Data Class"},
      {RecordableTypeId::PoseRecordableClass, "Pose Data Class"},
      {RecordableTypeId::MotionRecordableClass, "Motion Data Class"},
      {RecordableTypeId::GazeRecordableClass, "Gaze Data Class"},
      {RecordableTypeId::MeshRecordableClass, "Mesh Data Class"},
      {RecordableTypeId::MocapRecordableClass, "Mocap Data Class"},
      {RecordableTypeId::PointCloudRecordableClass, "Point Cloud Data Class"},
      {RecordableTypeId::MapRecordableClass, "Map Data Class"},

      /// << Annotations >>
      {RecordableTypeId::AnnotationRecordableClass, "Annotation Data Class"},

      /// << Test, Samples and other fake devices >>
      {RecordableTypeId::SampleDeviceRecordableClass, "Sample Class"},
      {RecordableTypeId::UnitTestRecordableClass, "Unit Test Class"},

      /// Recordable Class Ids -- end

      {RecordableTypeId::Cv1Camera, "Cv1 Camera"},
      {RecordableTypeId::Cv1IMU, "Cv1 IMU"},
      {RecordableTypeId::Cv1SyncPulse, "Cv1 Sync Pulse"},
      {RecordableTypeId::Cv1OutsideIn, "Cv1 Outside-In"},
      {RecordableTypeId::GenericPoseData, "Generic Pose Data"},
      {RecordableTypeId::Proto0CameraHAL, "Proto0 Camera (HAL)"},
      {RecordableTypeId::Proto0IMUHAL, "Proto0 IMU (HAL)"},
      {RecordableTypeId::Proto0SyncPulseHAL, "Proto0 Sync Pulse (HAL)"},
      {RecordableTypeId::Proto0CameraDML, "Proto0 Camera (DML)"},
      {RecordableTypeId::Proto0IMUDML, "Proto0 IMU (DML)"},
      {RecordableTypeId::Proto0SyncPulseDML, "Proto0 Sync Pulse (DML)"},
      {RecordableTypeId::Proto0ControllerHAL, "Proto0 Controller (HAL)"},
      {RecordableTypeId::Proto0ControllerSyncPulseHAL, "Proto0 Controller Sync Pulse (HAL)"},
      {RecordableTypeId::Proto0CameraHALConstellation, "Proto0 Camera Constellation (HAL)"},
      {RecordableTypeId::Proto0CameraHALSlam, "Proto0 Camera Slam (HAL)"},
      {RecordableTypeId::MontereyIMUDML, "Monterey IMU (DML)"},
      {RecordableTypeId::MontereySyncPulseDML, "Monterey Sync Pulse (DML)"},
      {RecordableTypeId::MontereyCameraSlamDML, "Monterey Camera Slam (DML)"},
      {RecordableTypeId::MontereyCameraConstellationDML, "Monterey Camera Constellation (DML)"},
      {RecordableTypeId::MontereyControllerDML, "Monterey Controller (DML)"},
      {RecordableTypeId::PolarisCamera, "Polaris Camera"},
      {RecordableTypeId::PolarisAudio, "Polaris Audio"},

      {RecordableTypeId::VisionInterfaceInput, "Vision Interface Input"},
      {RecordableTypeId::SlamCameraData, "Camera Data (SLAM)"},
      {RecordableTypeId::SlamImuData, "IMU Data (SLAM)"},
      {RecordableTypeId::SlamMagnetometerData, "Magnetometer Data (SLAM)"},
      {RecordableTypeId::ProjectorIlluminatedCameraData,
       "Projector Illuminated Camera Data (SLAM)"},
      {RecordableTypeId::ConstellationCameraData, "Camera Data (Constellation)"},
      {RecordableTypeId::ControllerImuData, "IMU Data (Controller)"},
      {RecordableTypeId::ControllerMagnetometerData, "Magnetometer Data (Controller)"},
      {RecordableTypeId::ControllerRadioPerformanceReport, "Controller Radio Performance Report"},
      {RecordableTypeId::ControllerCoiData, "Integrated IMU Data (Controller)"},
      {RecordableTypeId::ControllerButtonState, "Controller Touch Input Button State"},
      {RecordableTypeId::ControllerHapticsData, "Controller Haptics Status"},
      {RecordableTypeId::TimeSyncData, "Time Synchronization Samples"},
      {RecordableTypeId::FaceEyeTrackingCameraData, "Camera Data (Face/Eye Tracking)"},
      {RecordableTypeId::HandTrackingCameraData, "Camera Data (Nimble Hand Tracking)"},
      {RecordableTypeId::WpsLlaData, "WPS LLA Data"},

      {RecordableTypeId::ArgentCamera, "Argent Camera"},
      {RecordableTypeId::ArgentAudio, "Argent Audio"},
      {RecordableTypeId::ArgentPoseData, "Argent Pose Data"},
      {RecordableTypeId::ArgentMetadata, "Argent Metadata"},
      {RecordableTypeId::ArgentManifestDuration, "Argent Manifest Duration"},
      {RecordableTypeId::ArgentBiopotential, "Argent Biopotentials"},
      {RecordableTypeId::CthulhuImage, "Cthulhu Image Stream"},
      {RecordableTypeId::CthulhuAudio, "Cthulhu Audio Stream"},
      {RecordableTypeId::CthulhuAlignerMeta, "Cthulhu Aligner Metadata"},
      {RecordableTypeId::CthulhuGeneric, "Cthulhu Generic Stream"},
      {RecordableTypeId::CthulhuUserControl, "Cthulhu User Control Events"},

      {RecordableTypeId::InsideOutAlgorithms, "Inside-Out Algorithms"},

      // Groundtruth data recordings
      {RecordableTypeId::SyntheticGroundtruthIMU, "Synthetic Groundtruth IMU"},
      {RecordableTypeId::SyntheticGroundtruthDepthmap, "Synthetic Groundtruth Depthmap"},

      {RecordableTypeId::Generic3AxisSensor, "Generic 3 Axis Sensor"},

      // Depth sensing
      {RecordableTypeId::DepthSensing, "Depth Sensing"},
      {RecordableTypeId::DepthSensingAlgorithms, "Depth Sensing Algorithms"},
      {RecordableTypeId::AsterixOutputData, "AsterixOutputData"},
      {RecordableTypeId::PhoenixSensor, "PhoenixSensor"},
      {RecordableTypeId::HendrixStructuredLightProjector, "Hendrix Structured Light Projector"},
      {RecordableTypeId::StructuredLightIntermediateOutput,
       "Structured Light Intermediate Outputs"},
      {RecordableTypeId::SensorDepthData, "SensorDepthData"},
      {RecordableTypeId::WorldDepthData, "WorldDepthData"},
      {RecordableTypeId::PhoenixDiagnostics, "PhoenixDiagnostics"},
      {RecordableTypeId::PlanckLengthDepthMap, "PlanckLength DepthMap"},
      {RecordableTypeId::PlanckLengthRectDepthMap, "PlanckLength Rectified DepthMap"},
      {RecordableTypeId::PlanckLengthRGB, "PlanckLength RGB"},
      {RecordableTypeId::PlanckLengthDepthMapIR, "PlanckLength DepthMap for IR Frames"},
      {RecordableTypeId::PlanckLengthDepthMapRGB, "PlanckLength DepthMap for RGB Camera"},
      {RecordableTypeId::PlanckLengthPixelLabel, "PlanckLength Pixel Label"},
      {RecordableTypeId::PlanckLengthNormal, "PlanckLength Surface Normal"},

      // Face tracking
      {RecordableTypeId::FaceTrackingAlgorithms, "Face Tracking Algorithms"},
      {RecordableTypeId::FaceCameraOV9762, "Face Camera OV9762"},
      {RecordableTypeId::FaceFlexDepthCamera, "Face Flex Depth Camera"},
      {RecordableTypeId::FaceEyeTrackingLeds, "Face/Eye Tracking LEDs"},
      {RecordableTypeId::FaceAudio, "Face Audio"},
      {RecordableTypeId::FaceTrackingAnnotation, "Face Tracking Annotation"},

      // Eye tracking
      {RecordableTypeId::EyeTrackingCamera, "Eye Tracking Camera"},
      {RecordableTypeId::EyeTrackingCalibration, "Eye Tracking Calibration"},
      {RecordableTypeId::EyeTrackingSynthCamera_DEPRECATED,
       "Eye Tracking Synthesis Camera (Deprecated)"},
      {RecordableTypeId::DeviceMetricsForEyeTracking, "Device Metrics for EyeTracking"},
      {RecordableTypeId::EyeTrackingGroundTruth, "Eye Tracking Track Data Ground Truth"},

      // Hand tracking (nimble) and SAU's body tracking
      {RecordableTypeId::HandTrackingAlgorithms, "Hand Tracking Algorithms"},
      {RecordableTypeId::NimbleHandState, "Hand Tracking State"},
      {RecordableTypeId::NimbleMeshData, "Hand Tracking Mesh Data"},
      {RecordableTypeId::NimbleMocapData, "Hand Tracking Mocap Data"},
      {RecordableTypeId::NimbleBodyData, "Nimble Body Data"},
      {RecordableTypeId::NimbleKeyboardData_DEPRECATED, "Nimble Keyboard Data"},
      {RecordableTypeId::NimbleObjectTrackingData, "Nimble Object Tracking Data"},
      {RecordableTypeId::NimbleDepthTrackingMetrics, "Nimble Depth-Based Hand Tracking Metrics"},
      {RecordableTypeId::NimbleHandTrackingPluginData_DEPRECATED,
       "Nimble Hand Tracking Plugin Data"},
      {RecordableTypeId::NimbleSyntheticIntermediateData,
       "Nimble Hand Synthetic Intermediate Data"},
      {RecordableTypeId::NimbleHandContactAnnotationData, "Nimble Hand Contact Annotation Data"},
      {RecordableTypeId::NimbleDensePoseData, "Nimble DensePose data"},
      {RecordableTypeId::NimbleSenselForceArray, "Nimble Sensel force array data"},
      {RecordableTypeId::NimbleSenselLabelsArray, "Nimble Sensel labels array data"},
      {RecordableTypeId::NimbleSenselContactData, "Nimble Sensel contacts data"},
      {RecordableTypeId::NimbleTextData, "Nimble text stream data"},
      {RecordableTypeId::NimbleSurfaceContactData, "Nimble Surface contact data"},
      {RecordableTypeId::NimbleSelfTouchContactData, "Nimble Self touch contact data"},
      {RecordableTypeId::NimbleHandVisibilityData, "Nimble hand visibility data"},
      {RecordableTypeId::NimbleGestureData, "Nimble gesture data"},
      {RecordableTypeId::DeviceIndependentImages, "Device-independent Images (hand & body)"},
      {RecordableTypeId::DeviceIndependentDepthImage,
       "Device-independent Depth Images (hand & body)"},
      {RecordableTypeId::DeviceIndependentMonochromeImage,
       "Device-independent Monochrome Images (hand & body)"},
      {RecordableTypeId::DeviceIndependentRGBImage, "Device-independent RGB Images (hand & body)"},
      {RecordableTypeId::DeviceIndependentIRImage, "Device-independent IR Images (hand & body)"},
      {RecordableTypeId::DeviceIndependentSegmentationLabels,
       "Device-independent Segmentation Labels (hand & body)"},
      {RecordableTypeId::DeviceIndependentImageAnnotations, "Device-independent Image Annotations"},
      {RecordableTypeId::DeviceIndependentGestureAnnotations,
       "Device-independent Gesture Annotations"},
      {RecordableTypeId::DeviceIndependentCompressedRGBImage,
       "Device-independent RGB Images (compressed)"},
      {RecordableTypeId::DeviceIndependentFreeFormAnnotations,
       "Device-independent Free - form Annotations"},
      {RecordableTypeId::DeviceIndependentMonochrome10BitImage,
       "Device-independent 10-bit Monochrome Images (hand & body)"},
      {RecordableTypeId::DeviceIndependentPointCloud, "Device-independent point cloud"},
      {RecordableTypeId::DeviceIndependentMesh, "Device-independent mesh"},
      {RecordableTypeId::DeviceIndependentMsgPack,
       "Device-independent stream for generic msgpacks"},
      // Body tracking
      {RecordableTypeId::WillowDevices, "WillowDevices"},

      // Facebook AR Camera
      {RecordableTypeId::FacebookARCamera, "Facebook AR Camera"},
      {RecordableTypeId::FacebookARGyroscope, "Facebook AR Gyroscope"},
      {RecordableTypeId::FacebookARMagnetometer, "Facebook AR Magnetometer"},
      {RecordableTypeId::FacebookARAccelerometer, "Facebook AR Accelerometer"},
      {RecordableTypeId::FacebookARCalibratedMotionData,
       "Facebook AR Calibrated Device Motion Data"},
      {RecordableTypeId::FacebookARSnapshot, "Facebook AR Snapshot"},

      // Sequoia camera
      {RecordableTypeId::SequoiaCamera, "Sequoia Camera"},

      // Device Indepdent IMU
      {RecordableTypeId::DeviceIndependentIMUData, "Device-independent IMU Data"},

      // Surreal / Livemaps data collections
      {RecordableTypeId::SurrealGPSData, "Surreal GPS data"},
      {RecordableTypeId::SurrealWifiData, "Surreal Wifi data"},
      {RecordableTypeId::SurrealBluetoothData, "Surreal Bluetooth data"},
      {RecordableTypeId::SurrealLidarData, "Surreal lidar data"},
      {RecordableTypeId::SurrealSensorsData, "Surreal sensors data"},
      {RecordableTypeId::SurrealGameRotationVector, "Surreal game rotation vector data"},
      {RecordableTypeId::SurrealThirdPartyVIOData, "Surreal third party VIO data"},

      // IRISLib Eye Tracking
      {RecordableTypeId::IrisEyeTrackingConfiguration, "IRISLib Eye Tracking Configuration"},
      {RecordableTypeId::IrisEyeTrackingCamera_DEPRECATED,
       "IRISLib EyeTracking Camera (DEPRECATED)"},
      {RecordableTypeId::IrisEyeTrackingCalibration, "IRISLib Eye Tracking Calibration"},
      {RecordableTypeId::IrisEyeTrackingCometPosition, "IRISLib Eye Tracking Comet Position"},

      // Agios
      {RecordableTypeId::AgiosMasterType, "AGIOS Master Type"},
      {RecordableTypeId::AgiosImageStream, "AGIOS Image Stream"},
      {RecordableTypeId::AgiosBendGloveStream, "AGIOS Bend Glove Stream"},

      // Mixed Reality
      {RecordableTypeId::MixedRealityVisionImageData, "Mixed Reality Vision Interface Image Data"},

      // BCI
      {RecordableTypeId::BCIRawDatum, "BCI Raw Datum"},
      {RecordableTypeId::BCIDemodDatum, "BCI Demod Datum"},
      {RecordableTypeId::BCIGazeDatum, "BCI Gaze Datum"},
      {RecordableTypeId::BCIImuDatum, "BCI Imu Datum"},
      {RecordableTypeId::BCIAnnotationDatum, "BCI Annotation Datum"},
      {RecordableTypeId::BCILFAnalogDatum, "BCI LF Analog Datum"},

      // BlueBox
      {RecordableTypeId::BlueBoxAnyMessage, "BlueBox Generic Message (deprecated)"},
      {RecordableTypeId::BlueBoxV2AnyMessage, "BlueBox V2 Generic Message"},

      // Saturn
      {RecordableTypeId::SaturnCameraData, "Saturn Camera Data"},
      {RecordableTypeId::SaturnImuData, "Saturn IMU Data"},
      {RecordableTypeId::SaturnDepthData, "Saturn Depth Data"},

      // Calibration Stations
      {RecordableTypeId::AutoBotCameraData, "AutoBot Camera Data"},

      // Optitrack
      {RecordableTypeId::OptitrackCameraData, "Optitrack Camera Data"},

      // Free form data
      {RecordableTypeId::DescriptiveData, "Descriptive Data"},

      // Pretend devices for testing
      {RecordableTypeId::UnitTest1, "Unit Test 1"},
      {RecordableTypeId::UnitTest2, "Unit Test 2"},
      {RecordableTypeId::SampleDevice, "Sample Device"}};

  return sRegistry;
}

} // namespace

string toString(RecordableTypeId typeId) {
  const map<RecordableTypeId, const char*>& registry = getRecordableTypeIdRegistry();
  auto iter = registry.find(typeId);
  if (iter != registry.end()) {
    return iter->second;
  }
  return fmt::format("<Unknown device type '{}'>", static_cast<int>(typeId));
}

bool StreamId::isKnownTypeId(RecordableTypeId typeId) {
  const map<RecordableTypeId, const char*>& registry = getRecordableTypeIdRegistry();
  return registry.find(typeId) != registry.end();
}

string StreamId::getName() const {
  return fmt::format("{} #{}", getTypeName(), static_cast<int>(instanceId_));
}

string StreamId::getNumericName() const {
  return to_string(static_cast<int>(typeId_)) + '-' + to_string(instanceId_);
}

StreamId StreamId::fromNumericName(const string& numericName) {
  // Quick parsing of "NNN-DDD", two uint numbers separated by a '-'.
  const uint8_t* s = reinterpret_cast<const uint8_t*>(numericName.c_str());
  if (*s < '0' || *s > '9') {
    return {}; // must start with a digit
  }
  int recordableTypeId;
  for (recordableTypeId = 0; *s >= '0' && *s <= '9'; ++s) {
    recordableTypeId = 10 * recordableTypeId + (*s - '0');
  }
  if (*s++ == '-') {
    if (*s < '0' || *s > '9') {
      return {}; // instance id must start with a digit
    }
    uint16_t index = 0;
    while (*s >= '0' && *s <= '9') {
      index = 10 * index + (*s++ - '0');
    }
    if (*s == 0) {
      return StreamId(static_cast<RecordableTypeId>(recordableTypeId), index);
    }
  }
  return {};
}

} // namespace vrs
