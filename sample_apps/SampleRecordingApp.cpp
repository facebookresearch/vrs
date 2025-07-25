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

#include <chrono>

#define DEFAULT_LOG_CHANNEL "SampleRecordingApp"
#include <logging/Log.h>
#include <logging/Verify.h>

#include <vrs/DataLayoutConventions.h>
#include <vrs/RecordFileWriter.h>
#include <vrs/RecordFormat.h>
#include <vrs/Recordable.h>
#include <vrs/TagConventions.h>
#include <vrs/os/Utils.h>

#include "SharedDefinitions.h"

using namespace vrs;
using namespace vrs::datalayout_conventions;
using namespace vrs_sample_apps;

namespace vrs_sample_apps {

static double now() {
  using namespace std::chrono;
  return duration_cast<duration<double>>(steady_clock::now().time_since_epoch()).count();
}

// Use your own clock source.
static double getTimestampSec() {
  static double kStartTime = now();
  return (now() - kStartTime) * 500; // arbitrarily spread timestamps over more time
}

/// \brief Sample fake device showing how to produce records containing metadata and images.
///
/// Stream of images, that resembles how we've stored camera data for many projects
/// A configuration record stores the camera image settings (resolution, pixel format...)
/// and maybe some calibration data, etc.
/// Data records contains a block of meta data along with some pixel data.
/// The meta datya captures some sensor data such as exposure and the camera's temperature, and
/// maybe some counters (frame counter,  camera time, etc).
class ImageStream : public Recordable {
  // Record format version numbers describe the overall record format.
  // Note DataLayout field changes do *not* require to change the record format version.
  static const uint32_t kConfigurationRecordFormatVersion = 1;
  static const uint32_t kDataRecordFormatVersion = 1;

 public:
  ImageStream() : Recordable(RecordableTypeId::ForwardCameraRecordableClass, kCameraStreamFlavor) {
    // Tell how the records of this stream should be compressed (or not)
    setCompression(CompressionPreset::ZstdMedium);
    // Extremly important: Define the format of this stream's records
    addRecordFormat(
        Record::Type::CONFIGURATION,
        kConfigurationRecordFormatVersion,
        // the following describe config records' format: a single datalayout content block
        config_.getContentBlock(),
        {&config_});
    addRecordFormat(
        Record::Type::DATA,
        kDataRecordFormatVersion,
        // the following describe data records' format: a datalayout content block + pixel data
        data_.getContentBlock() + ContentBlock(ImageFormat::RAW),
        {&data_});
  }
  const Record* createConfigurationRecord() override {
    config_.width.set(640);
    config_.height.set(480);
    config_.pixelFormat.set(PixelFormat::GREY8);
    config_.cameraCalibration.stage(CALIBRATION_VALUES);

    // create a record using that data
    return createRecord(
        getTimestampSec(),
        Record::Type::CONFIGURATION,
        kConfigurationRecordFormatVersion,
        DataSource(config_));
  }
  const Record* createStateRecord() override {
    // Best practice is to always create a record when asked, with a reasonable timestamp,
    // even if the record is empty.
    return createRecord(getTimestampSec(), Record::Type::STATE, 0);
  }
  // When an image is captured, create a record for it
  void createDataRecord(uint64_t frameCount) {
    // we create fake data: in a real app, the data will come for sensors/cameras
    data_.exposureTime.set(47390873);
    data_.exposure.set(2.5f);
    data_.frameCounter.set(frameCount);
    data_.cameraTemperature.set(38.5f);
    // fake pixel data
    vector<uint8_t> pixels(config_.width.get() * config_.height.get());
    for (size_t pixelIndex = 0; pixelIndex < pixels.size(); pixelIndex++) {
      pixels[pixelIndex] = static_cast<uint8_t>(frameCount + pixelIndex);
    }

    // create a record using that (fake) data
    createRecord(
        getTimestampSec(),
        Record::Type::DATA,
        kDataRecordFormatVersion,
        DataSource(data_, {pixels.data(), pixels.size()}));
  }

 private:
  // DataLayout objects aren't super cheap to create, so we reuse the same instances every time
  CameraStreamConfig config_;
  CameraStreamData data_;
};

/// \brief Sample fake device showing how to produce records containing audio data (no metadata).
///
/// Stream of audio blocks. Because audio samples can come at a high frequency, we collect in blocks
/// that we save in records. Note that the size of these blocks may vary from record to record,
/// even though in this sample, the records are always of the same size.
/// This sample stream doesn't use any configuration record, but we coud easily add one if needed.
class AudioStream : public Recordable {
  static const uint32_t kDataRecordFormatVersion = 1;

 public:
  AudioStream() : Recordable(RecordableTypeId::AudioStream, kAudioStreamFlavor) {
    // Tell how the records of this stream should be compressed (or not)
    setCompression(CompressionPreset::ZstdMedium);
    // Extremly important: Define the format of this stream's records
    addRecordFormat(
        Record::Type::DATA,
        kDataRecordFormatVersion,
        // the following describe data records' format: a block of audio samples
        ContentBlock(AudioFormat::PCM, AudioSampleFormat::S16_LE, kNumChannels, 0, kSampleRate),
        {});
  }
  const Record* createConfigurationRecord() override {
    // Best practice is to always create a record when asked, with a reasonable timestamp,
    // even if the record is empty.
    return createRecord(getTimestampSec(), Record::Type::CONFIGURATION, 0);
  }
  const Record* createStateRecord() override {
    // Best practice is to always create a record when asked, with a reasonable timestamp,
    // even if the record is empty.
    return createRecord(getTimestampSec(), Record::Type::STATE, 0);
  }
  void createDataRecords(size_t blockIndex) {
    // Create a fake block of audio samples...
    vector<int16_t> samples(kAudioBlockSize);
    for (size_t k = 0; k < kAudioBlockSize; k++) {
      samples[k] = static_cast<int16_t>(blockIndex * kAudioBlockSize + k);
    }
    // Save the audio data in a record. Attention: we need a number of bytes!
    createRecord(
        getTimestampSec(),
        Record::Type::DATA,
        kDataRecordFormatVersion,
        DataSource(DataSourceChunk(samples.data(), samples.size() * sizeof(samples[0]))));
  }
};

/// \brief Sample fake device showing how to produce records containing metadata.
///
/// Stream of metadata of some sort.
/// Both configuration & data records contain a single datalayout content block.
class MotionStream : public Recordable {
  static const uint32_t kConfigurationRecordFormatVersion = 1;
  static const uint32_t kDataRecordFormatVersion = 1;

 public:
  MotionStream() : Recordable(RecordableTypeId::MotionRecordableClass, kMotionStreamFlavor) {
    // Tell how the records of this stream should be compressed (or not)
    setCompression(CompressionPreset::ZstdMedium);
    // Extremly important: Define the format of this stream's records
    addRecordFormat(
        Record::Type::CONFIGURATION,
        kConfigurationRecordFormatVersion,
        // the following describe config records' format: a single datalayout content block
        config_.getContentBlock(),
        {&config_});
    addRecordFormat(
        Record::Type::DATA,
        kDataRecordFormatVersion,
        // the following describe data records' format: a single datalayout content block
        data_.getContentBlock(),
        {&data_});
  }
  const Record* createConfigurationRecord() override {
    // Set the fields of config_, as necessary...
    config_.motionStreamParam.set(kMotionValue);
    return createRecord(
        getTimestampSec(),
        Record::Type::CONFIGURATION,
        kConfigurationRecordFormatVersion,
        DataSource(config_));
  }
  const Record* createStateRecord() override {
    // Best practice is to always create a record when asked, with a reasonable timestamp,
    // even if the record is empty.
    return createRecord(getTimestampSec(), Record::Type::STATE, 0);
  }
  void createDataRecord(const vector<Matrix3Dd>& motionData) {
    data_.motionData.stage(motionData);
    createRecord(
        getTimestampSec(), Record::Type::DATA, kDataRecordFormatVersion, DataSource(data_));
  }

 private:
  MotionStreamConfig config_;
  MotionStreamData data_;
};

} // namespace vrs_sample_apps

int main() {
  // Make the file & attach the streams
  RecordFileWriter fileWriter;
  ImageStream imageStream;
  AudioStream audioStream;
  MotionStream motionStream;
  fileWriter.addRecordable(&imageStream);
  fileWriter.addRecordable(&audioStream);
  fileWriter.addRecordable(&motionStream);

  // Add some context (add your own)
  tag_conventions::addCaptureTime(fileWriter);
  tag_conventions::addOsFingerprint(fileWriter);
  fileWriter.setTag("purpose", "sample_code"); // sample tag for illustration purposes

  // Create the file, start recording...
  XR_VERIFY(fileWriter.createFileAsync(os::getHomeFolder() + kSampleFileName) == 0);

  // Every second, write-out records older than 1 seconds
  fileWriter.autoWriteRecordsAsync([]() { return getTimestampSec() - 1; }, 1);

  // Create a bunch of fake records.
  // With a "real" app, those records would be created from different threads receiving data
  // from different sources (camera, audio driver, sensor, etc).
  for (size_t k = 0; k < kDataRecordCount; k++) {
    // create records, as long as you need to...
    imageStream.createDataRecord(k);
    vector<Matrix3Dd> motionData(k);
    motionStream.createDataRecord(motionData);
    audioStream.createDataRecords(k);
  }

  // Close the file & wait for the data to be written out...
  XR_VERIFY(fileWriter.waitForFileClosed() == 0);

  return 0;
}
