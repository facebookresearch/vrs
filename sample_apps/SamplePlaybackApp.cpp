// Copyright (c) Meta Platforms, Inc. and affiliates.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <cassert>

#define DEFAULT_LOG_CHANNEL "SamplePlaybackApp"
#include <logging/Checks.h>
#include <logging/Log.h>
#include <logging/Verify.h>

#include <vrs/ErrorCode.h>
#include <vrs/RecordFileReader.h>
#include <vrs/RecordFormatStreamPlayer.h>
#include <vrs/os/Utils.h>

#include "SharedDefinitions.h"

using namespace vrs;
using namespace vrs::DataLayoutConventions;
using namespace vrs_sample_apps;

namespace vrs_sample_apps {

/// Image stream reader showing how to read records from a typical stream containing images.
class ImageStreamPlayer : public RecordFormatStreamPlayer {
 public:
  bool onDataLayoutRead(const CurrentRecord& record, size_t blockIndex, DataLayout& dl) override {
    switch (record.recordType) {
      case Record::Type::CONFIGURATION: {
        CameraStreamConfig& config = getExpectedLayout<CameraStreamConfig>(dl, blockIndex);
        // use the data...
        vector<float> calibration;
        config.cameraCalibration.get(calibration);
      } break;

      case Record::Type::DATA: {
        CameraStreamData& data = getExpectedLayout<CameraStreamData>(dl, blockIndex);
        // use the data...
        XR_CHECK(data.exposure.isAvailable());
      } break;

      default:
        assert(false); // should not happen, but you want to know if it does!
        break;
    }
    return true;
  }
  bool onImageRead(const CurrentRecord& record, size_t idx, const ContentBlock& cb) override {
    // the image data was not read yet: allocate your own buffer & read the pixel data!
    size_t frameByteCount = cb.getBlockSize();
    XR_CHECK(frameByteCount != 0); // Should not happen, but you want to know if it does!
    XR_CHECK(frameByteCount != ContentBlock::kSizeUnknown); // Should not happen either...

    /// find more about the image format:
    //    const ImageContentBlockSpec& spec = block.image();
    //    uint32_t width = spec.getWidth();
    //    uint32_t height = spec.getHeight();
    //    PixelFormat pixelFormat = spec.getPixelFormat();
    //    size_t bytesPerPixel = spec.getBytesPerPixel();
    //    uint32_t lineStrideBytes = spec.getStride();

    std::vector<uint8_t> frameBytes(frameByteCount);
    // Synchronously read the image data, all at once, or line-by-line, byte-by-byte, as you like...
    if (record.reader->read(frameBytes) == 0) {
      // In this sample code, we verify that the image data matches the expected pattern.
      for (size_t k = 0; k < frameByteCount; k++) {
        XR_CHECK(frameBytes[k] == static_cast<uint8_t>(k + imageIndex));
      }
    }
    imageIndex++;
    return true; // read next blocks, if any
  }
  size_t getImageReadCount() const {
    return imageIndex;
  }

 private:
  size_t imageIndex = 0;
};

/// Audio stream reader showing how to read records from a typical stream containing audio blocks.
class AudioStreamPlayer : public RecordFormatStreamPlayer {
 public:
  bool onAudioRead(const CurrentRecord& record, size_t blockIdx, const ContentBlock& cb) override {
    // the audio data was not read yet. Allocate/reuse your own buffers.
    if (XR_VERIFY(cb.getBlockSize() != ContentBlock::kSizeUnknown)) {
      XR_CHECK(cb.audio().getSampleCount() == kAudioBlockSize);
      vector<int16_t> audioData(cb.audio().getSampleCount());
      // actually read the audio data
      if (XR_VERIFY(record.reader->read(audioData) == 0)) {
        // use audio data. In this sample code, we verify that the data matches the expected pattern
        for (size_t k = 0; k < kAudioBlockSize; k++) {
          XR_CHECK(audioData[k] == static_cast<int16_t>(audioBlockIndex * kAudioBlockSize + k));
        }
      }
      audioBlockIndex++;
    }

    return true;
  }
  size_t getAudioBlockCount() const {
    return audioBlockIndex;
  }

 private:
  size_t audioBlockIndex = 0;
};

/// Stream reader showing how to read records containing only metadata information in a DataLayout.
class MotionStreamPlayer : public RecordFormatStreamPlayer {
 public:
  bool onDataLayoutRead(const CurrentRecord& record, size_t blockIndex, DataLayout& dl) override {
    switch (record.recordType) {
      case Record::Type::CONFIGURATION: {
        MotionStreamConfig& myConfig = getExpectedLayout<MotionStreamConfig>(dl, blockIndex);
        myConfig.motionStreamParam.get(); // read data
        // use the data...
      } break;

      case Record::Type::DATA: {
        MotionStreamData& myData = getExpectedLayout<MotionStreamData>(dl, blockIndex);
        vector<Matrix3Dd> motionData;
        myData.motionData.get(motionData);
        // use the data...
      } break;

      default:
        assert(false); // should not happen, but you want to know if it does!
        break;
    }
    motionRecordCount++;
    return true;
  }
  size_t getMotionRecordCount() const {
    return motionRecordCount;
  }

 private:
  size_t motionRecordCount = 0;
};

} // namespace vrs_sample_apps

int main() {
  RecordFileReader reader;
  int status = reader.openFile(os::getHomeFolder() + kSampleFileName);
  if (status != 0) {
    XR_LOGE("Can't open file {}, error: {}", kSampleFileName, errorCodeToMessage(status));
    return status;
  }
  ImageStreamPlayer imageStreamPlayer;
  AudioStreamPlayer audioStreamPlayer;
  MotionStreamPlayer motionStreamPlayer;
  StreamId id;
  id = reader.getStreamForFlavor(
      RecordableTypeId::ForwardCameraRecordableClass, kCameraStreamFlavor);
  if (XR_VERIFY(id.isValid())) {
    reader.setStreamPlayer(id, &imageStreamPlayer);
  }
  id = reader.getStreamForFlavor(RecordableTypeId::AudioStream, kAudioStreamFlavor);
  if (XR_VERIFY(id.isValid())) {
    reader.setStreamPlayer(id, &audioStreamPlayer);
  }
  id = reader.getStreamForFlavor(RecordableTypeId::MotionRecordableClass, kMotionStreamFlavor);
  if (XR_VERIFY(id.isValid())) {
    reader.setStreamPlayer(id, &motionStreamPlayer);
  }
  // We're ready: read all the records in order, and send them to the stream players registered
  reader.readAllRecords();
  reader.closeFile();

  if (XR_VERIFY(imageStreamPlayer.getImageReadCount() == kDataRecordCount)) {
    XR_LOGI("Successfully read {} images.", kDataRecordCount);
  }
  if (XR_VERIFY(audioStreamPlayer.getAudioBlockCount() == kDataRecordCount)) {
    XR_LOGI("Successfully read {} audio blocks.", kDataRecordCount);
  }
  if (XR_VERIFY(motionStreamPlayer.getMotionRecordCount() == kDataRecordCount + 1)) {
    XR_LOGI("Successfully read {} motion blocks.", kDataRecordCount);
  }

  return 0;
}
