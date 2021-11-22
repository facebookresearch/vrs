// Copyright (c) Facebook, Inc. and its affiliates. Confidential and proprietary.

#pragma once

#include <cstdint>
#include <string>

#include <vrs/os/Platform.h>

namespace vrs {

using std::string;

/// \brief VRS stream type or class identifier enum.
///
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

  SlamCameraData = 1201,

#if IS_VRS_FB_INTERNAL()
#include "StreamId_fb.h"
#endif

  // Space for private devices

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

/// \brief VRS stream identifier class.
///
/// Identifier for a stream of records, containing a RecordableTypeId and an instance id, so that
/// multiple streams of the same kind can be recorded side-by-side in a VRS file unambiguously.
///
/// Note that instance ids are not meant to be controlled, set, or defined by the recording code.
///
/// During recording, VRS generates a unique instance id when a recordable is created,
/// to ensure that each recordable has a unique stream id in the whole system. In particular, by
/// design, if you stop recording, destroy the recordables and create new ones, the instance ids
/// generated will keep increasing.
/// Therefore, when discovering the streams in a VRS file, specific instance ids can't be used to
/// recognize different instances of particular RecordableTypeId. Instead, use recordable tags or
/// flavors. The APIs RecordFileReader::getStreamForTag() and RecordFileReader::getStreamForFlavor()
/// can then quickly determine the StreamId for each of the streams in the file.
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
