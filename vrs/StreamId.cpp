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

#include "StreamId.h"

#include <string>
#include <unordered_map>

#include <fmt/format.h>

using namespace std;

namespace vrs {

namespace {

const unordered_map<RecordableTypeId, const char*>& getRecordableTypeIdRegistry() {
  static const unordered_map<RecordableTypeId, const char*> sRegistry = {
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
      {RecordableTypeId::SlamCameraData, "Camera Data (SLAM)"},
      {RecordableTypeId::DisplayObserverCameraRecordableClass, "Display Observing Camera Class"},
      {RecordableTypeId::WorldObserverCameraRecordableClass, "World Observing Camera Class"},
      {RecordableTypeId::DisparityCameraRecordableClass, "Disparity Camera Class"},

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
      {RecordableTypeId::EMGRecordableClass, "Electromyography (EMG) Data Class"},
      {RecordableTypeId::EMGGestureRecordableClass, "Electromyography (EMG) Gesture Data Class"},
      {RecordableTypeId::CapacitiveTouchRecordableClass, "Capacitive Touch Data Class"},
      {RecordableTypeId::HeartRateRecordableClass, "Heart Rate Data Class"},
      {RecordableTypeId::CaloriesRecordableClass, "Calories Data Class"},
      {RecordableTypeId::CsaRecordableClass, "Current Sense Amplifier (CSA) Data Class"},
      {RecordableTypeId::RadarRecordableClass, "Radar Data Class"},
      {RecordableTypeId::StepCountRecordableClass, "Step Count Data Class"},
      {RecordableTypeId::ForceRecordableClass, "Force Data Class"},
      {RecordableTypeId::DistanceRecordableClass, "Distance Data Class"},
      {RecordableTypeId::ActiveMinutesRecordableClass, "Active Minutes Data Class"},
      {RecordableTypeId::HeadingRecordableClass, "Heading Data Class"},

      {RecordableTypeId::SlamImuData, "IMU Data (SLAM)"},
      {RecordableTypeId::SlamMagnetometerData, "Magnetometer Data (SLAM)"},

      /// << Calibration, Setup, Diagnostic, etc >>
      {RecordableTypeId::CalibrationRecordableClass, "Calibration Data Class"},
      {RecordableTypeId::AlignmentRecordableClass, "Alignment Data Class"},
      {RecordableTypeId::SetupRecordableClass, "Setup Data Class"},
      {RecordableTypeId::DiagnosticRecordableClass, "Diagnostic Data Class"},
      {RecordableTypeId::PerformanceRecordableClass, "Performance Data Class"},
      {RecordableTypeId::IlluminationRecordableClass, "Illumination Data Class"},
      {RecordableTypeId::DisplayRecordableClass, "Display Data Class"},

      /// << External Signals >>
      {RecordableTypeId::SyncRecordableClass, "Sync Data Class"},
      {RecordableTypeId::GpsRecordableClass, "GPS Data Class"},
      {RecordableTypeId::WifiBeaconRecordableClass, "Wifi Beacon Data Class"},
      {RecordableTypeId::BluetoothBeaconRecordableClass, "Bluetooth Beacon Data Class"},
      {RecordableTypeId::UsbRecordableClass, "USB Data Class"},
      {RecordableTypeId::TimeRecordableClass, "Time Domain Mapping Class"},
      {RecordableTypeId::AttentionRecordableClass, "Attention Data Class"},
      {RecordableTypeId::GMSRecordableClass, "GMS Data Class"},

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
      {RecordableTypeId::SensorVarianceBiasRecordableClass, "Sensor Variance/Bias Data Class"},
      {RecordableTypeId::AnchorRecordableClass, "Anchor Data Class"},
      {RecordableTypeId::SegmentationRecordableClass, "Segmentation Data Class"},
      {RecordableTypeId::TextRecordableClass, "Text Data Class"},
      {RecordableTypeId::SpeechRecordableClass, "Speech Data Class"},

      /// << Annotations >>
      {RecordableTypeId::AnnotationRecordableClass, "Annotation Data Class"},

      /// << Test, Samples and other fake devices >>
      {RecordableTypeId::SampleDeviceRecordableClass, "Sample Class"},
      {RecordableTypeId::UnitTestRecordableClass, "Unit Test Class"},

  /// Recordable Class Ids -- end

#if IS_VRS_FB_INTERNAL()
#include "StreamId_fb.h"
#endif

      // Pretend devices for testing
      {RecordableTypeId::UnitTest1, "Unit Test 1"},
      {RecordableTypeId::UnitTest2, "Unit Test 2"},
      {RecordableTypeId::SampleDevice, "Sample Device"}};

  return sRegistry;
}

StreamId fromNumericNameWithSeparator(const string& numericName, uint8_t separator) {
  // Quick parsing of "NNN-DDD", two uint numbers separated by a separator.
  const auto* s = reinterpret_cast<const uint8_t*>(numericName.c_str());
  if (*s < '0' || *s > '9') {
    return {}; // must start with a digit
  }
  int recordableTypeId = 0;
  for (; *s >= '0' && *s <= '9'; ++s) {
    recordableTypeId = 10 * recordableTypeId + (*s - '0');
  }
  if (*s++ == separator) {
    if (*s < '0' || *s > '9') {
      return {}; // instance id must start with a digit
    }
    uint16_t index = 0;
    while (*s >= '0' && *s <= '9') {
      index = 10 * index + (*s++ - '0');
    }
    if (*s == 0) {
      return {static_cast<RecordableTypeId>(recordableTypeId), index};
    }
  }
  return {};
}

} // namespace

string toString(RecordableTypeId typeId) {
  const unordered_map<RecordableTypeId, const char*>& registry = getRecordableTypeIdRegistry();
  auto iter = registry.find(typeId);
  if (iter != registry.end()) {
    return iter->second;
  }
  return fmt::format("<Unknown device type '{}'>", static_cast<int>(typeId));
}

bool StreamId::isKnownTypeId(RecordableTypeId typeId) {
  const unordered_map<RecordableTypeId, const char*>& registry = getRecordableTypeIdRegistry();
  return registry.find(typeId) != registry.end();
}

string StreamId::getName() const {
  return fmt::format("{} #{}", getTypeName(), static_cast<int>(instanceId_));
}

string StreamId::getNumericName() const {
  return to_string(static_cast<int>(typeId_)) + '-' + to_string(instanceId_);
}

StreamId StreamId::fromNumericName(const string& numericName) {
  return fromNumericNameWithSeparator(numericName, '-');
}

StreamId StreamId::fromNumericNamePlus(const string& numericName) {
  return fromNumericNameWithSeparator(numericName, '+');
}

} // namespace vrs
