// (c) Facebook, Inc. and its affiliates. Confidential and proprietary.

#pragma once

// clang-format off

/*
  These tag names are pure conventions VRS users may want to follow when creating their VRS files.
*/

#include <vrs/RecordFileWriter.h>

namespace vrs {
namespace tag_conventions {

// Overall identification, HW & SW independent

  // Project name: overarching project name.
  constexpr const char* kProjectName = "project_name";
  // EPOCH time in seconds since Jan 1, 1970, when the capture started. Uses C's 'time(NULL)'
  constexpr const char* kCaptureTimeEpoch = "capture_time_epoch";
  // Session ID: unique identifier which can be used to recognize the session
  constexpr const char* kSessionId = "session_id";
  // Capture type: description of the recording session context
  // Ex: "calibration", "data_collection", "test"
  constexpr const char* kCaptureType = "capture_type";
  // Tags, for instance, for Facebook tags.
  constexpr const char* kTagSet = "tag_set";

  /// Add a unique session id tag, generated then.
  /// @param writer: the file writer to add the tag to.
  /// @return The generated session ID.
  std::string addUniqueSessionId(RecordFileWriter& writer);
  /// Add a capture time tag, using the current time.
  /// @param writer: the file writer to add the tag to.
  void addCaptureTime(RecordFileWriter& writer);
  /// Add a set of tags to a file.
  /// @param writer: the file writer to add the tagset to.
  /// @param tags: a vector of text tags.
  void addTagSet(RecordFileWriter& writer, const vector<string>& tags);

// Hardware components
// For dependent devices with their own id/serial (controllers?), tag at the stream level

  // Device Type: device type. Ex: "Monterey", "Caplipso", "Laguna"
  constexpr const char* kDeviceType = "device_type";
  // Device version: device version name. Ex: "proto0", "EVT2"
  constexpr const char* kDeviceVersion = "device_version";
  // Device Serial: serial number of the main HW (in case of multi part HW)
  constexpr const char* kDeviceSerial = "device_serial";
  // Device ID: user code identification of the device
  constexpr const char* kDeviceId = "device_id";
  // Hardware configuration: description of the hardware setup
  constexpr const char* kHardwareConfiguration = "hardware_configuration";

  /// Add tags describing a device.
  /// @param writer: the file writer, or a recordable to describe.
  /// @param type: a device type descrption.
  /// @param serialNumber: the device's serial number.
  /// @param version: the device's version number.
  template <class T> inline // vrs::RecordFileWriter or vrs::Recordable
  void addDevice(T& writer, const string& type, const string& serialNumber, const string& version) {
    writer.setTag(kDeviceType, type);
    writer.setTag(kDeviceSerial, serialNumber);
    writer.setTag(kDeviceVersion, version);
  }
  /// Add a tag for the device ID.
  /// @param writer: the file writer, or a recordable to describe.
  /// @param id: the device's ID.
  template <class T> inline // vrs::RecordFileWriter or vrs::Recordable
  void addDeviceId(T& writer, const string& id) {
    writer.setTag(kDeviceId, id);
  }

// Software components
// For dependent devices with their own SW/FW (controllers?), tag at the stream level

  // OS fingerprint: Operating system build signature
  constexpr const char* kOsFingerprint = "os_fingerprint";
  // SW compile time: when the recording software was compiled
  constexpr const char* kSoftwareCompileDate = "software_compile_date";
  // SW revision: source control revision of the software
  constexpr const char* kSoftwareRevision = "software_revision";
  // FW compile time: when the recording firmware was compiled
  constexpr const char* kFirmwareCompileDate = "firmware_compile_date";
  // FW revision: source control revision of the firmware
  constexpr const char* kFirmwareRevision = "firmware_revision";

  /// Add a tag describing the OS version.
  /// @param writer: the file writer to attach the tag to.
  void addOsFingerprint(RecordFileWriter& writer);
  /// Add a tag describing the running software version.
  /// @param writer: the file writer to attach the tag to.
  /// @param compileDate: the software's compile date.
  /// @param rev: the software's revision number.
  inline
  void addSoftwareDetails(RecordFileWriter& writer, const string& compileDate, const string& rev) {
    writer.setTag(kSoftwareCompileDate, compileDate);
    writer.setTag(kSoftwareRevision, rev);
  }
  /// Add tags describing the FW version of the main device or the recordable.
  /// @param writer: the file writer or a recordable to describe.
  /// @param fwCompileDate: the FW's compile date.
  /// @param fwRevision: the FW's revision number.
  template <class T> // vrs::RecordFileWriter or vrs::Recordable
  inline void addFirmwareDetails(T& writer, const string& fwCompileDate, const string& fwRevision) {
    writer.setTag(kFirmwareCompileDate, fwCompileDate);
    writer.setTag(kFirmwareRevision, fwRevision);
  }

// For streams which may have multiple instances in the same recording

  // Device role: which "role" has this device in the system.
  // Ex: "top-right camera", "left controller"
  constexpr const char* kDeviceRole = "device_role"; // When relevant

// Key configuration/settings (when relevant, never required)

  // Image decimation factor: when only 1 out of N images are being recorded
  constexpr const char* kImageDecimationFactor = "image_decimation_factor";
  // Overall camera frame rate
  constexpr const char* kCameraFrameRate = "camera_frame_rate";
  // Intensity target used by dynamic exposure control. How to fill it:
  // * Do not fill it or fill it with -1 when unknown.
  // * Fill it with 0 when using fixed exposure settings.
  // * Fill it with the right intensity target when using dynamic exposure
  constexpr const char* kDynamicExposureTarget = "iot_dynamic_exposure_target";

// Helper functions
  /// Convert a set of string tags to json.
  /// @param tags: A set of string tags.
  /// @return A json string containing all the tags.
  string makeTagSet(const vector<string>& tags);
  /// Convert a json tag set back to a vector of string tags.
  /// @param jsonTagSet: json string containing the tags.
  /// @param outVectorTagSet: vector of strings where to place the tags parsed.
  /// @return True if parsing worked (might still not be real tagset).
  bool parseTagSet(const string& jsonTagSet, vector<string>& outVectorTagSet);

/*
 * Sample: Monterey + controllers
 *
 * File tags:
 *  kProjectName = "Monterey"
 *  kCaptureTimeEpoch = "1520364293"
 *  kSessionId = "5584bdc43"
 *  kCaptureType = "calibration"   <- other use case: "INSIDE_OUT_TRACKER_RECORDING"...
 *
 *  kDeviceType = "Monterey"
 *  kDeviceVersion = "EVT3"             <- main device (HMD for Monterey)
 *  kDeviceSerial = "0ba3602ffbee80b9"  <- of HMD
 *  kHardwareConfiguration = "hmd + 2 hand controllers"
 *
 *  kOsFingerprint = "oculus/vr_monterey_proto1/proto1:7.1.1/N9F27L/863:userdev/dev-keys"
 *  kSoftwareCompileDate = "Sep 26 2017 19:06:45"
 *  kSoftwareRevision = "185ea53a5584bdc43640ba3602ccbee80b91d33e"
 *  kFirmwareCompileDate = "Sep 20 2017 15:26:25"
 *  kFirmwareRevision = "265ea53b5584bdc43640ba3602ffbee80b91d43a"
 *
 * Controller stream tags (for each stream)
 *  kDeviceType = "Monterey Controller"
 *  kDeviceVersion = "EVT0"
 *  kDeviceSerial = "bee80b91d3"
 *  kDeviceId = "left controller"
 *  kFirmwareCompileDate = "Sep 20 2017 15:26:25"
 *  kFirmwareRevision = "265ea53b5584bdc43640ba3602ffbee80b91d43a"
 */

} // namespace tag_conventions
} // namespace vrs
