/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include <cstdint>
#include <string>

#include <vrs/os/Platform.h>

namespace vrs {

using std::string;

/// \brief VRS stream type or class identifier enum.
///
/// Each stream in a VRS file has a type, represented by RecordableTypeId.
/// RecordableTypeId values represent a class of recordable, a particular logical or virtual
/// recordable, or a hardware specific recordable, using their specific record formats.
///
/// Initially, every recordable needed their own enum values. This was non-scalable way to identify
/// unique recordable types, and required every use case to have their own recordable type id in a
/// shared enum definition. This method is now deprecated.
///
/// Today, instead of creating new RecordableTypeId values for new devices, use an existing
/// "Recordable Class" ID (values between 200 and 999) along with a flavor to describe the data
/// being recorded in the stream.
///
/// Should none of the existing "recordable class" match your use case, please propose new ones.
///
/// Meta only: If you can not use a "recordable class" value with a "flavor" without having to
/// refactor a lot of code, you can still create new enum values, even if that's bad practice.
///
/// For each enum value, a proper description must be provided in StreamId.cpp
///
enum class RecordableTypeId : uint16_t {
  // for VRS internal use only
  VRSIndex = 1, ///< Internal, for index records. See IndexRecord.
  VRSDescription = 2, ///< Internal, for description records. See DescriptionRecord.

  // Generic devices using RecordFormat and DataLayout, and following DataLayout conventions.
  ImageStream = 100, ///< Generic image stream. Prefer using "Recordable Class" IDs.
  AudioStream = 101, ///< Generic audio stream. Prefer using "Recordable Class" IDs.
  AnnotationStream = 102, ///< Generic annotation stream. Prefer using "Recordable Class" IDs.
  ArchiveStream = 103, ///< Generic archive stream. Prefer using "Recordable Class" IDs.

  // << Start of Recordable Class IDs >>

  // "Recordable Class" IDs designed to make adding new enum values unnecessary going forward.
  // Use a combination of "Recordable Class" ID and "flavor" to describe your use case uniquely.
  // Many of these recordable class IDs are in anticipation of a potential future use case.

  // << Cameras >>
  // Cameras are arguably the most the important and common class of devices of all.
  // New devices include many different types at the same time, hence the wide variety offered.

  ForwardCameraRecordableClass = 200, ///< For cameras looking forward.
  UpwardCameraRecordableClass = 201, ///< For cameras looking up.
  DownwardCameraRecordableClass = 202, ///< For cameras looking down.
  BackwardCameraRecordableClass = 203, ///< For cameras looking back.
  SidewardCameraRecordableClass = 204, ///< For cameras looking to the side.
  OutwardCameraRecordableClass = 205, ///< For cameras looking outward.
  InwardCameraRecordableClass = 206, ///< For cameras looking inward.
  InsideOutCameraRecordableClass = 207, ///< For inside out cameras.
  OutsideInCameraRecordableClass = 208, ///< For outside in cameras.
  DepthCameraRecordableClass = 209, ///< For depth cameras.
  IRCameraRecordableClass = 210, ///< For infrared cameras.
  EyeCameraRecordableClass = 211, ///< For cameras recording eyes.
  FaceCameraRecordableClass = 212, ///< For cameras recording a face.
  MouthCameraRecordableClass = 213, ///< For cameras recording a mouth.
  RgbCameraRecordableClass = 214, ///< For color cameras.
  DisplayObserverCameraRecordableClass = 215, ///< For display observing cameras.

  // << Microphones >>
  MonoAudioRecordableClass = 230, ///< For mono microphones.
  StereoAudioRecordableClass = 231, ///< For stereo microphones.
  AmbientAudioRecordableClass = 232, ///< For multichannel microphones.

  // << Discrete Sensors >>
  SensorRecordableClass = 240, ///< For unspecified sensor data. Use flavors to be specific.
  ImuRecordableClass = 241, ///< For IMU data streams.
  AccelerometerRecordableClass = 242, ///< For accelerometer data streams.
  MagnetometerRecordableClass = 243, ///< For magnetometer data streams.
  GyroscopeRecordableClass = 244, ///< For gyroscope data streams.
  LidarRecordableClass = 245, ///< For Lidar data streams.
  TemperatureRecordableClass = 246, ///< For temperature data streams.
  BarometerRecordableClass = 247, ///< For barometer data streams.
  PhotoplethysmogramRecordableClass = 248, ///< For photoplethysmography data streams.
  EMGRecordableClass = 249, ///< For electromyography data streams
  CapacitiveTouchRecordableClass = 250, ///< For capacitive touch data streams.

  // << Calibration, Setup, Diagnostic, etc >>
  CalibrationRecordableClass = 260, ///< For calibration data streams.
  AlignmentRecordableClass = 261, ///< For alignment data streams.
  SetupRecordableClass = 262, ///< For setup data streams.
  DiagnosticRecordableClass = 263, ///< For diagnostic data streams.
  PerformanceRecordableClass = 264, ///< For performance data streams.
  IlluminationRecordableClass = 265, ///< For illumination data streams.

  // << External Signals >>
  SyncRecordableClass = 280, ///< For synchronisation data streams.
  GpsRecordableClass = 281, ///< For GPS data streams.
  WifiBeaconRecordableClass = 282, ///< For WIFI beacon data streams.
  BluetoothBeaconRecordableClass = 283, ///< For bluetooth beacon data streams.
  UsbRecordableClass = 284, ///< For USB data streams.
  TimeRecordableClass = 285, ///< For time data streams.
  AttentionRecordableClass = 286, ///< For attention data streams.
  GMSRecordableClass = 287, ///< For GMS location data stream.

  // << User Input >>
  InputRecordableClass = 300, ///< For user input data streams.
  TextInputRecordableClass = 301, ///< For text input data streams.
  MouseRecordableClass = 302, ///< For mouse data streams.
  TouchInputRecordableClass = 303, ///< For touch input data streams.
  GestureInputRecordableClass = 304, ///< For gesture input data streams.
  ControllerRecordableClass = 305, ///< For controller data streams.

  // << Events, commands, instructions, etc >>
  EventRecordableClass = 320, ///< For event data streams.
  CommandRecordableClass = 321, ///< For command data streams.
  InstructionRecordableClass = 322, ///< For instructions data streams.
  ScriptRecordableClass = 323, ///< For script data streams.
  ControlRecordableClass = 324, ///< For control data streams.

  // << Ground Truth >>
  GroundTruthRecordableClass = 340, ///< For ground truth data streams.
  GroundTruthImuRecordableClass = 341, ///< For ground truth IMU data streams.
  GroundTruthAlignmentRecordableClass = 342, ///< For ground truth alignment data streams.
  GroundTruthPositionRecordableClass = 343, ///< For ground truth position data streams.
  GroundTruthOrientationRecordableClass = 344, ///< For ground truth orientation data streams.
  GroundTruthDepthRecordableClass = 345, ///< For ground truth depth data streams.

  // << Results of all kinds >>
  ResultRecordableClass = 370, ///< For result streams.
  PoseRecordableClass = 371, ///< For pose streams.
  MotionRecordableClass = 372, ///< For motion data streams.
  GazeRecordableClass = 373, ///< For gaze data streams.
  MeshRecordableClass = 374, ///< For mesh data streams.
  MocapRecordableClass = 375, ///< For motion capture data streams.
  PointCloudRecordableClass = 376, ///< For point cloud data streams.
  MapRecordableClass = 377, ///< For map data streams.
  SensorVarianceBiasRecordableClass = 378, ///< For sensor variance/bias results.
  AnchorRecordableClass = 379, ///< For anchor data streams (i.e. spatial persistence anchors).

  // << Annotations >>
  AnnotationRecordableClass = 400, ///< For annotation streams.

  // << Test, Samples and other fake devices >>
  SampleDeviceRecordableClass = 998, ///< For sample device streams.
  UnitTestRecordableClass = 999, ///< For unit test streams.

  // The range 200-999 is reserved for "Recordable Class" IDs exclusively.
  FirstRecordableClassId = 200, ///< Helper values to test if a type is a recordable class.
  LastRecordableClassId = 999, ///< Helper values to test if a type is a recordable class.

  // << End of Recordable Class IDs >>

  // Legacy values needed for open source purposes.
  SlamCameraData = 1201, ///< Legacy slam data stream.
  SlamImuData = 1202, ///< Legacy IMU data stream.
  SlamMagnetometerData = 1203, ///< Legacy magnetometer data stream.

#if IS_VRS_FB_INTERNAL()
#include "StreamId_fb.h"
#endif

  // Test devices start at 65500.
  TestDevices = 65500,
  UnitTest1 = TestDevices, ///< For unit tests.
  UnitTest2, ///< For unit tests.
  SampleDevice, ///< For sample code.

  Undefined = 65535 ///< Value used for default initializations and marking undefined situations.
};

/// Get an English readable recordable type name for the enum value.
/// @param typeId: the recordable type id to describe
/// @return English readable recordable type name.
///
/// Note that VRS stores the actual string-name when recording a file, so that you can later tell
/// how the recordable type was called when the recording was made.
/// VRStool will then show both the original and current recordable type names.
string toString(RecordableTypeId typeId);

/// Tell if an id is that of a "Recordable Class".
inline bool isARecordableClass(RecordableTypeId typeId) {
  return typeId >= RecordableTypeId::FirstRecordableClassId &&
      typeId <= RecordableTypeId::LastRecordableClassId;
}

/// \brief VRS stream identifier class.
///
/// Identifier for a stream of records, containing a RecordableTypeId and an instance id, so
/// that multiple streams of the same kind can be recorded side-by-side in a VRS file unambiguously.
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
  StreamId& operator=(StreamId&& rhs) = default;
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

  /// StreamId value guaranteed to be smaller than any valid StreamId object.
  static StreamId lowest() {
    return {static_cast<RecordableTypeId>(0), 0};
  }

 private:
  RecordableTypeId typeId_; ///< Identifier for a record type.
  uint16_t instanceId_; ///< Unique instance id, *not* controlled or controllable by client code.
};

} // namespace vrs
