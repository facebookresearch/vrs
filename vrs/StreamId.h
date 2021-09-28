// Copyright (c) Facebook, Inc. and its affiliates. Confidential and proprietary.

#pragma once

#include <cstdint>
#include <string>

namespace vrs {

using std::string;

/// VRS streams need a unique ID, so that VRS can differentiate the records of different streams.
///
/// RecordableTypeId represent a specific hardware or logical device, or when teams have made
/// their data interoperable, a class of devices using a common data format, or maybe a virtual
/// device, or a stream of annotations of all sorts...
/// For simplicity, we will say that each RecordableTypeId enum value identifies some type of
/// << recordable >>, which is a producer of data to be recorded in their own stream.
///
/// Using an enum value for each recordable type is admittedly a very lazy and inelegant way
/// to identify unique recordable types, because it requires every use case to have their own
/// recordable type id in a shared enum definition. This is the legacy method.
///
/// Today, rather than adding new enum values as we've done for too long, please use an existing
/// << recordable class >> id (values between 200 and 999) along with a << flavor >> that describes
/// more specifically the data being recorded.
///
/// Whereas << recordable class >> ids are meant to be used and reused by different teams, who can
/// describe their use case using the << flavor >> parameter, the legacy ids are not supposed to be
/// used outside of the context they were created for.
/// Should none of the existing << recordable class >> match your use case, feel free to propose new
/// ones. That's much better than creating new legacy enum values.
///
/// For now, if you can not use a << recordable class >> value with a << flavor >> without having to
/// refactor your code, you can still create new enum values, even if that's bad practice.
///
/// For each enum value, a text description must be provided in StreamId.cpp.
///
enum class RecordableTypeId : uint16_t {
  Undefined = 0xffff,

  /// for VRS internal use only
  VRSIndex = 1, // See IndexRecord
  VRSDescription = 2, // See DescriptionRecord

  /// Generic devices based on RecordFormat & DataLayout, and following DataLayout conventions
  /// Prefer using << Recordable class >> ids now.
  ImageStream = 100,
  AudioStream = 101,
  AnnotationStream = 102,
  ArchiveStream = 103,

  /// << Recordable class >> ids designed to make adding new enum values unnecessary going forward.
  /// Please consider using a combination of << recordable class >> ids and << flavor >> to
  /// describe your devices, rather than adding new enum values.

  /// << Cameras >>
  /// Cameras are arguably the most the important and common class of devices of all.
  /// New devices include many different types at the same time, hence the wide variety offered.
  ForwardCameraRecordableClass = 200,
  UpwardCameraRecordableClass = 201,
  DownwardCameraRecordableClass = 202,
  BackwardCameraRecordableClass = 203,
  SidewardCameraRecordableClass = 204,
  OutwardCameraRecordableClass = 205,
  InwardCameraRecordableClass = 206,
  InsideOutCameraRecordableClass = 207,
  OutsideInCameraRecordableClass = 208,
  DepthCameraRecordableClass = 209,
  IRCameraRecordableClass = 210,
  EyeCameraRecordableClass = 211,
  FaceCameraRecordableClass = 212,
  MouthCameraRecordableClass = 213,
  RgbCameraRecordableClass = 214,

  /// << Microphones >>
  MonoAudioRecordableClass = 230,
  StereoAudioRecordableClass = 231,
  AmbientAudioRecordableClass = 232,

  /// << Discrete Sensors >>
  SensorRecordableClass = 240,
  ImuRecordableClass = 241,
  AccelerometerRecordableClass = 242,
  MagnetometerRecordableClass = 243,
  GyroscopeRecordableClass = 244,
  LidarRecordableClass = 245,
  TemperatureRecordableClass = 246,
  BarometerRecordableClass = 247,
  PhotoplethysmogramRecordableClass = 248,

  /// << Calibration, Setup, Diagnostic, etc >>
  CalibrationRecordableClass = 260,
  AlignmentRecordableClass = 261,
  SetupRecordableClass = 262,
  DiagnosticRecordableClass = 263,
  PerformanceRecordableClass = 264,
  IlluminationRecordableClass = 265,

  /// << External Signals >>
  SyncRecordableClass = 280,
  GpsRecordableClass = 281,
  WifiBeaconRecordableClass = 282,
  BluetoothBeaconRecordableClass = 283,
  UsbRecordableClass = 284,
  TimeRecordableClass = 285,
  AttentionRecordableClass = 286,

  /// << User Input >>
  InputRecordableClass = 300,
  TextInputRecordableClass = 301,
  MouseRecordableClass = 302,
  TouchInputRecordableClass = 303,
  GestureInputRecordableClass = 304,
  ControllerRecordableClass = 305,

  /// << Events, commands, instructions, etc >>
  EventRecordableClass = 320,
  CommandRecordableClass = 321,
  InstructionRecordableClass = 322,
  ScriptRecordableClass = 323,
  ControlRecordableClass = 324,

  /// << Ground Truth >>
  GroundTruthRecordableClass = 340,
  GroundTruthImuRecordableClass = 341,
  GroundTruthAlignmentRecordableClass = 342,
  GroundTruthPositionRecordableClass = 343,
  GroundTruthOrientationRecordableClass = 344,
  GroundTruthDepthRecordableClass = 345,

  /// << Results of all kinds >>
  ResultRecordableClass = 370,
  PoseRecordableClass = 371,
  MotionRecordableClass = 372,
  GazeRecordableClass = 373,
  MeshRecordableClass = 374,
  MocapRecordableClass = 375,
  PointCloudRecordableClass = 376,
  MapRecordableClass = 377,

  /// << Annotations >>
  AnnotationRecordableClass = 400,

  /// << Test, Samples and other fake devices >>
  SampleDeviceRecordableClass = 998,
  UnitTestRecordableClass = 999,

  /// The whole range 200-999 is reserved for << recordable class >> ids.
  FirstRecordableClassId = 200,
  LastRecordableClassId = 999,

  /// << Legacy devices >> start at 1000 (prefer using << recordable class >> ids)
  Cv1Camera = 1000,
  Proto0IMUHAL = 1001,
  Proto0SyncPulseHAL = 1002,
  DepthSensing = 1003,
  Proto0CameraHAL = 1004,
  Cv1IMU = 1005,
  Cv1SyncPulse = 1006,
  Proto0CameraDML = 1007,
  Proto0IMUDML = 1008,
  Proto0SyncPulseDML = 1009,
  Proto0ControllerHAL = 1010,
  Proto0ControllerSyncPulseHAL = 1011,
  Proto0CameraHALConstellation = 1012,
  Proto0CameraHALSlam = 1013,
  MontereyIMUDML = 1014,
  MontereySyncPulseDML = 1015,
  MontereyCameraSlamDML = 1016,
  MontereyCameraConstellationDML = 1017,
  MontereyControllerDML = 1018,
  PolarisCamera = 1100,
  PolarisAudio = 1101,

  /// visiontypes starts at 1200
  VisionInterfaceInput = 1200,
  SlamCameraData = 1201, // visiontypes image data, usually synchronized framesets. long exposure.
  SlamImuData = 1202, // visiontypes motion data for headset IMU.
  SlamMagnetometerData = 1203, // visiontypes motion data for headset magnetometer.
  ConstellationCameraData = 1204, // visiontypes image data, synchronized framesets. Short exposure.
  ControllerImuData = 1205, // visiontypes motion data, controller IMU.
  ControllerMagnetometerData = 1206, // Unused.
  ControllerRadioPerformanceReport = 1207, // Controller wireless link data quality report.
  ControllerCoiData = 1208, // Controller preintegrated data.
  FaceEyeTrackingCameraData = 1209, // visiontypes image data, synchronized framesets.
  HandTrackingCameraData = 1210, // visiontypes image data, synced framesets. Dynamic exposure.
  ProjectorIlluminatedCameraData =
      1211, // visiontypes image data, projector illuminated SLAM camera data.
  ControllerButtonState = 1212, // Controller button presses.

  ControllerHapticsData = 1213, // Controller haptics status.
  WpsLlaData = 1214, // visiontypes WPS LLA data.
  TimeSyncData = 1215, // Time synchronization data to sync local and reference time domains.

  /// FRL-R Pittsburgh claims 1300 - 1399
  ArgentCamera = 1300,
  ArgentAudio = 1301,
  ArgentPoseData = 1302,
  ArgentMetadata = 1303,
  ArgentManifestDuration = 1304,
  ArgentBiopotential = 1305,
  CthulhuImage = 1310,
  CthulhuAudio = 1311,
  CthulhuAlignerMeta = 1312,
  CthulhuGeneric = 1313,
  CthulhuUserControl = 1314,

  /// Outside-in start at 2000
  Cv1OutsideIn = 2000,
  GenericPoseData = 2001,

  /// Inside-out start at 3000
  InsideOutAlgorithms = 3000,
  SyntheticGroundtruthIMU = 3001,
  SyntheticGroundtruthDepthmap = 3002,
  Generic3AxisSensor = 3003,

  /// Depth Sensing start at 4000
  DepthSensingAlgorithms = 4000,
  AsterixOutputData = 4001,
  PhoenixSensor = 4002,
  HendrixStructuredLightProjector = 4003, // Hendrix projects infrared lines, not dots
  StructuredLightIntermediateOutput = 4004, // Ground truth about the structured light
  SensorDepthData = 4100,
  WorldDepthData = 4101,
  PhoenixDiagnostics = 4102,
  PlanckLengthDepthMap = 4200,
  PlanckLengthRectDepthMap = 4201,
  PlanckLengthRGB = 4202,
  PlanckLengthDepthMapIR = 4203,
  PlanckLengthDepthMapRGB = 4204,
  PlanckLengthPixelLabel = 4205,
  PlanckLengthNormal = 4206,

  /// Face Tracking start at 5000
  FaceTrackingAlgorithms = 5000,
  FaceCameraOV9762 = 5001,
  FaceFlexDepthCamera = 5002,
  FaceEyeTrackingLeds = 5003,
  FaceAudio = 5100,
  FaceTrackingAnnotation = 5200,

  /// Eye Tracking starts at 6000
  EyeTrackingCamera = 6000,
  EyeTrackingCalibration = 6001,
  EyeTrackingSynthCamera_DEPRECATED = 6002,
  DeviceMetricsForEyeTracking = 6003,
  EyeTrackingGroundTruth = 6004,

  /// Hand Tracking start at 7000
  /// Config data for all formats is a MessagePack blob.
  HandTrackingAlgorithms = 7000,
  NimbleHandState = 7001,
  NimbleMeshData = 7002,
  NimbleMocapData = 7003,
  NimbleBodyData = 7004,
  NimbleKeyboardData_DEPRECATED = 7005,
  NimbleObjectTrackingData = 7006,
  NimbleDepthTrackingMetrics = 7007,
  NimbleHandTrackingPluginData_DEPRECATED = 7008,
  NimbleSyntheticIntermediateData = 7009,
  NimbleHandContactAnnotationData = 7010,

  /// DensePose gives a dense UV field across the surface of a mesh;
  /// this is a 2d image that contains (instanceID, partID, U, V)
  NimbleDensePoseData = 7011,

  /// Sensels are a force sensitive touch panel (not captouch). They give
  /// an "image" of force per pixel, as well as a segmented image of
  /// disjoint contact regions and statistics about those contact regions.
  NimbleSenselForceArray = 7012,
  NimbleSenselLabelsArray = 7013,
  NimbleSenselContactData = 7014,

  // Streams of text for handtracking based text-input
  NimbleTextData = 7015,

  // Surface contact information for Nimble
  NimbleSurfaceContactData = 7016,

  // Self touch contact information for Nimble
  NimbleSelfTouchContactData = 7017,

  // Hand visibility information for Nimble
  NimbleHandVisibilityData = 7018,

  // General gesture information for Nimble
  NimbleGestureData = 7019,

  /// Device-independent image formats used by the hand-tracking
  /// and body-tracking teams.
  /// Config data for all formats is a MessagePack blob.
  /// Details of the format are in this Quip doc: https://fb.quip.com/y7YbAhM1bb3T
  DeviceIndependentImages = 8000,
  DeviceIndependentDepthImage = 8001,
  DeviceIndependentMonochromeImage = 8002,
  DeviceIndependentRGBImage = 8003,
  DeviceIndependentIRImage = 8004,
  DeviceIndependentSegmentationLabels = 8005,
  DeviceIndependentImageAnnotations = 8006,
  DeviceIndependentGestureAnnotations = 8007,
  DeviceIndependentCompressedRGBImage = 8008,
  DeviceIndependentFreeFormAnnotations = 8009,
  DeviceIndependentMonochrome10BitImage = 8010,
  DeviceIndependentPointCloud = 8011,
  DeviceIndependentMsgPack = 8012,
  DeviceIndependentMesh = 8013,

  /// Body Tracking starts at 9000
  WillowDevices = 9000,

  /// Facebook AR camera on mobile devices
  FacebookARCamera = 10000, // Image data from camera
  FacebookARGyroscope = 10001, // Raw sensor data
  FacebookARMagnetometer = 10002, // Raw sensor data
  FacebookARAccelerometer = 10003, // Raw sensor data
  FacebookARCalibratedMotionData = 10004, // Contains attitude of the device
  FacebookARSnapshot = 10005, // Computed snapshot tracking result

  // Sequoia devices
  SequoiaCamera = 11000,

  // Device Independent IMU
  DeviceIndependentIMUData = 12000,

  // Surreal / Livemaps starts at 13000
  SurrealGPSData = 13000,
  SurrealWifiData = 13001,
  SurrealBluetoothData = 13002,
  SurrealLidarData = 13003,
  SurrealSensorsData = 13004,
  SurrealGameRotationVector = 13005,
  SurrealThirdPartyVIOData = 13006,

  // IRISLib Eye Tracking starts at 14000
  IrisEyeTrackingConfiguration = 14000,
  IrisEyeTrackingCamera_DEPRECATED = 14001, // Not used. Use PolarisCamera instead.
  IrisEyeTrackingCalibration = 14002,
  IrisEyeTrackingCometPosition = 14003,

  // AGIOS starts at 15000
  AgiosMasterType = 15000,
  AgiosImageStream = 15001,
  AgiosBendGloveStream = 15002,

  // Mixed Reality starts at 16000
  MixedRealityVisionImageData = 16000,

  // BCI starts at 17000
  BCIRawDatum = 17000,
  BCIDemodDatum = 17001,
  BCIGazeDatum = 17002,
  BCIImuDatum = 17003,
  BCIAnnotationDatum = 17004,
  BCILFAnalogDatum = 17005,

  // BlueBox starts at 18000
  BlueBoxAnyMessage = 18000,
  BlueBoxV2AnyMessage = 18001,

  // Saturn starts at 19000
  SaturnCameraData = 19000,
  SaturnImuData = 19001,
  SaturnDepthData = 19002,

  // Calibration station data types starts at 20000
  AutoBotCameraData = 20000,

  // Optitrack starts at 30000
  OptitrackCameraData = 30000,

  // Free form data starts at 40000
  DescriptiveData = 40000,

  // !! READ ME PLEASE !!
  // Let's reserve blocks of at most 100 ids at a time, before someone else reserves 10,000 ids for
  // their project... So please, find a spot under 40,000 before going beyond!
  //
  // Also: don't forget to add a text description for your Ids in StreamId.cpp. Thanks!

  /// Test devices start at 65500.
  TestDevices = 65500,
  UnitTest1 = TestDevices,
  UnitTest2,
  SampleDevice,

  /// We're using an uint16_t, so do not use any value above 65535!
};

/// Get an English readable recordable type name for the enum value.
/// @param typeId: the recordable type id to describe
/// @return English readable recordable type name.
/// This name may change over time, so don't use it for identification purposes in your code.
/// This can allow the description to change during the life cycle of a product (proto0, proto1,
/// EVT1, DVT1, PVT1, etc.)
///
/// Note that VRS stores the actual string-name when recording a file, so that you can later tell
/// how the recordable type was called when the recording was made.
/// VRStool will then show both the original and current recordable type names.
string toString(RecordableTypeId typeId);

/// Tell if an id is that of a << Recordable Class >>.
inline bool isARecordableClass(RecordableTypeId typeId) {
  return typeId >= RecordableTypeId::FirstRecordableClassId &&
      typeId <= RecordableTypeId::LastRecordableClassId;
}

/// Identifier for a stream of records, containing a RecordableTypeId and an instance id, so that
/// multiple streams of the same kind can be recorded side-by-side in a VRS file unambiguously.
///
/// Note that instance ids are not meant to be controlled, set, or defined by client code.
///
/// During recording, VRS generates a unique instance id when a recordable is created,
/// to ensure that each recordable has a unique stream id in the whole system. By design, if you
/// stop recording, and destroy the recordables and create new ones, the instance ids will keep
/// increasing.
///
/// To identify streams, don't rely on instance id values, use recordable tags or flavors instead.
/// When reading a file, use RecordFileReader::getStreamForTag() and
/// RecordFileReader::getStreamForFlavor(), to directly find the streams you're looking for.
class StreamId {
 public:
  StreamId() : typeId_{RecordableTypeId::Undefined}, instanceId_{0} {}
  StreamId(const StreamId& rhs) = default;
  StreamId(RecordableTypeId typeId, uint16_t instanceId)
      : typeId_{typeId}, instanceId_{instanceId} {}

  /// Get the recordable type id.
  /// @return Recordable type id.
  RecordableTypeId getTypeId() const {
    return typeId_;
  }

  /// Get the instance id.
  /// @return Instance id.
  uint16_t getInstanceId() const {
    return instanceId_;
  }

  StreamId& operator=(const StreamId& rhs) = default;
  bool operator==(const StreamId& rhs) const {
    return typeId_ == rhs.typeId_ && instanceId_ == rhs.instanceId_;
  }

  bool operator!=(const StreamId& rhs) const {
    return !operator==(rhs);
  }

  /// Compare operator, so that we can use StreamId in containers, with a guarantied behavior.
  bool operator<(const StreamId& rhs) const {
    return typeId_ < rhs.typeId_ || (typeId_ == rhs.typeId_ && instanceId_ < rhs.instanceId_);
  }

  /// Test if the instance represents device.
  /// Useful when an API returns a StreamId, and needs to tell that no device was found.
  /// @return True if the instance is valid/found, false otherwise.
  bool isValid() const {
    return typeId_ != RecordableTypeId::Undefined;
  }

  /// Get the name of the type of device.
  /// @return English readable recordable name.
  string getTypeName() const {
    return toString(typeId_);
  }
  /// Get a readable name for the device, combining the recordable type name and the instance id.
  /// @return English readable recordable name, including the instance id.
  string getName() const;
  /// Get a name combining the recordable type and the instance id, as numbers.
  /// @return Recordable name, using numeric values.
  string getNumericName() const;
  /// Convert from a stream ID numeric string representation
  /// @param numericName: a stream ID name in numeric representation, e.g., "1100-1"
  /// @return A stream ID. Use isValid() to know if the conversion succeeded.
  static StreamId fromNumericName(const string& numericName);

  /// A recording might be using a type id not known by the current version of the code.
  /// This should not be problem, but in some situation, in particular display purposes in
  /// particular, it can be useful to be able to tell.
  static bool isKnownTypeId(RecordableTypeId typeId);

 private:
  RecordableTypeId typeId_; ///< Identifier for a record type.
  uint16_t instanceId_; ///< Unique instance id, *not* controlled or controllable by client code.
};

} // namespace vrs
