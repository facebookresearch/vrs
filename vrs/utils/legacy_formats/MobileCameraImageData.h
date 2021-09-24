// (c) Facebook, Inc. and its affiliates. Confidential and proprietary.

#pragma once

#include <cstdint>

#include <memory>

namespace ctlegacy {

// Structure of the frame data returned from the camera endpoint
// NOTE: This is copy-paste from Mobile DML, since this struct will be used after DML is gone.
struct MobileCameraImageData {
  // Unique id of the camera that captured this frame
  std::uint32_t CameraUniqueId = 0;
  // Stream id of the camera if operating in interleaved mode
  // Default to raw meaning non-interleaved mode
  std::uint32_t CameraStreamId = 0;
  // Each frame should be +1 of the previous value
  std::uint64_t FrameCounter = 0;
  // The exposure time (in seconds) used to capture this frame
  double ExposureTime = 0.0;
  // The gain used to capture this frame: converted from raw 1~255 to 1~16f
  float Gain = 0.0;
  // The raw gain value read from camera HAL
  std::uint32_t GainRaw = 0;
  // The time (in seconds) that the frame arrived at the computer
  double ArrivalTime = 0.0;
  // The time (in microseconds) that the frame was read from camera
  std::uint64_t CameraTimeMicroSec = 0;
  // The raw frame data.  This is a shared_ptr so we can know when clients are
  // finished with the buffer and we can re-use it.
  std::shared_ptr<std::uint8_t> Data;
  // Number of bytes in "Data"
  std::uint32_t NumBytes = 0;
  // Width of the image
  std::uint16_t Width = 0;
  // Height of the image
  std::uint16_t Height = 0;
  // Camera capture time: mid exposure in the device clock domain
  double CaptureTime = 0.0; // in seconds
  // Temperature of the camera sensor that this image is read from
  float Temperature = 0.0;
};

} // namespace ctlegacy
