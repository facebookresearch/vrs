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

#include <vrs/RecordFileReader.h>
#include <vrs/RecordFormatStreamPlayer.h>

#include <vrs/oss/aria/AudioDataLayout.h>
#include <vrs/oss/aria/BaroDataLayout.h>
#include <vrs/oss/aria/BluetoothBeaconDataLayouts.h>
#include <vrs/oss/aria/GpsDataLayout.h>
#include <vrs/oss/aria/ImageDataLayout.h>
#include <vrs/oss/aria/MotionDataLayout.h>
#include <vrs/oss/aria/TimeSyncDataLayout.h>
#include <vrs/oss/aria/WifiBeaconDataLayouts.h>

using namespace vrs;

namespace aria_sample_reader {

void printDataLayout(const CurrentRecord& r, DataLayout& datalayout) {
  fmt::print(
      "{:.3f} {} record, {} [{}]\n",
      r.timestamp,
      toString(r.recordType),
      r.streamId.getName(),
      r.streamId.getNumericName());
  datalayout.printLayoutCompact(std::cout, "  ");
}

class AriaImagePlayer : public RecordFormatStreamPlayer {
  bool onDataLayoutRead(const CurrentRecord& r, size_t blockIndex, DataLayout& dl) override {
    if (r.recordType == Record::Type::CONFIGURATION) {
      auto& config = getExpectedLayout<aria::ImageSensorConfigurationLayout>(dl, blockIndex);
      // Read config record metadata...
      printDataLayout(r, config);
    } else if (r.recordType == Record::Type::DATA) {
      auto& data = getExpectedLayout<aria::ImageDataLayout>(dl, blockIndex);
      // Read data record metadata...
      printDataLayout(r, data);
    }
    return true;
  }
  bool onImageRead(const CurrentRecord& r, size_t /*idx*/, const ContentBlock& cb) override {
    // the image data was not read yet: allocate your own buffer & read!
    std::vector<uint8_t> frameBytes(cb.getBlockSize());
    const auto& imageSpec = cb.image();
    // Synchronously read the image data, which is jpg compressed with Aria
    if (cb.image().getImageFormat() == ImageFormat::JPG && r.reader->read(frameBytes) == 0) {
      /// do your thing with the jpg data...
      fmt::print(
          "{:.3f} {} [{}]: {}, {} bytes.\n",
          r.timestamp,
          r.streamId.getName(),
          r.streamId.getNumericName(),
          imageSpec.asString(),
          imageSpec.getBlockSize());
    }
    return true; // read next blocks, if any
  }
};

class AriaMotionSensorPlayer : public RecordFormatStreamPlayer {
  bool onDataLayoutRead(const CurrentRecord& r, size_t blockIndex, DataLayout& dl) override {
    if (r.recordType == Record::Type::CONFIGURATION) {
      auto& config = getExpectedLayout<aria::MotionSensorConfigurationLayout>(dl, blockIndex);
      // Read config record metadata...
      printDataLayout(r, config);
    } else if (r.recordType == Record::Type::DATA) {
      auto& data = getExpectedLayout<aria::MotionDataLayout>(dl, blockIndex);
      // Read data record metadata...
      printDataLayout(r, data);
    }
    return true;
  }
};

class AriaStereoAudioPlayer : public RecordFormatStreamPlayer {
  bool onDataLayoutRead(const CurrentRecord& r, size_t blockIndex, DataLayout& dl) override {
    if (r.recordType == Record::Type::CONFIGURATION) {
      auto& config = getExpectedLayout<aria::AudioConfigurationLayout>(dl, blockIndex);
      // Read config record metadata...
      printDataLayout(r, config);
    } else if (r.recordType == Record::Type::DATA) {
      auto& data = getExpectedLayout<aria::AudioDataLayout>(dl, blockIndex);
      // Read data record metadata...
      printDataLayout(r, data);
    }
    return true;
  }
  bool onAudioRead(const CurrentRecord& r, size_t blockIdx, const ContentBlock& cb) override {
    const AudioContentBlockSpec& audioSpec = cb.audio();
    assert(audioSpec.getSampleFormat() == AudioSampleFormat::S32_LE);
    vector<int32_t> audioData(audioSpec.getSampleCount() * audioSpec.getChannelCount());
    // actually read the audio data
    if (r.reader->read(audioData) == 0) {
      fmt::print(
          "{:.3f} {} [{}]: {} {}x{} samples.\n",
          r.timestamp,
          r.streamId.getName(),
          r.streamId.getNumericName(),
          audioSpec.asString(),
          audioSpec.getSampleCount(),
          audioSpec.getChannelCount());
    }
    return true;
  }
};

class AriaWifiBeaconPlayer : public RecordFormatStreamPlayer {
  bool onDataLayoutRead(const CurrentRecord& r, size_t blockIndex, DataLayout& dl) override {
    if (r.recordType == Record::Type::CONFIGURATION) {
      auto& config = getExpectedLayout<aria::WifiBeaconConfigurationLayout>(dl, blockIndex);
      // Read config record metadata...
      printDataLayout(r, config);
    } else if (r.recordType == Record::Type::DATA) {
      auto& data = getExpectedLayout<aria::WifiBeaconDataLayout>(dl, blockIndex);
      // Read data record metadata...
      printDataLayout(r, data);
    }
    return true;
  }
};

class AriaBlueToothBeaconPlayer : public RecordFormatStreamPlayer {
  bool onDataLayoutRead(const CurrentRecord& r, size_t blockIndex, DataLayout& dl) override {
    if (r.recordType == Record::Type::CONFIGURATION) {
      auto& config = getExpectedLayout<aria::BluetoothBeaconConfigurationLayout>(dl, blockIndex);
      // Read config record metadata...
      printDataLayout(r, config);
    } else if (r.recordType == Record::Type::DATA) {
      auto& data = getExpectedLayout<aria::BluetoothBeaconDataLayout>(dl, blockIndex);
      // Read data record metadata...
      printDataLayout(r, data);
    }
    return true;
  }
};

class AriaGpsPlayer : public RecordFormatStreamPlayer {
  bool onDataLayoutRead(const CurrentRecord& r, size_t blockIndex, DataLayout& dl) override {
    if (r.recordType == Record::Type::CONFIGURATION) {
      auto& config = getExpectedLayout<aria::GpsConfigurationLayout>(dl, blockIndex);
      // Read config record metadata...
      printDataLayout(r, config);
    } else if (r.recordType == Record::Type::DATA) {
      auto& data = getExpectedLayout<aria::GpsDataLayout>(dl, blockIndex);
      // Read data record metadata...
      printDataLayout(r, data);
    }
    return true;
  }
};

class AriaBarometerPlayer : public RecordFormatStreamPlayer {
  bool onDataLayoutRead(const CurrentRecord& r, size_t blockIndex, DataLayout& dl) override {
    if (r.recordType == Record::Type::CONFIGURATION) {
      auto& config = getExpectedLayout<aria::BarometerConfigurationLayout>(dl, blockIndex);
      // Read config record metadata...
      printDataLayout(r, config);
    } else if (r.recordType == Record::Type::DATA) {
      auto& data = getExpectedLayout<aria::BarometerDataLayout>(dl, blockIndex);
      // Read data record metadata...
      printDataLayout(r, data);
    }
    return true;
  }
};

class AriaTimeSyncPlayer : public RecordFormatStreamPlayer {
  bool onDataLayoutRead(const CurrentRecord& r, size_t blockIndex, DataLayout& dl) override {
    if (r.recordType == Record::Type::CONFIGURATION) {
      auto& config = getExpectedLayout<aria::TimeSyncConfigurationLayout>(dl, blockIndex);
      // Read config record metadata...
      printDataLayout(r, config);
    } else if (r.recordType == Record::Type::DATA) {
      auto& data = getExpectedLayout<aria::TimeSyncDataLayout>(dl, blockIndex);
      // Read data record metadata...
      printDataLayout(r, data);
    }
    return true;
  }
};

struct AriaFileReader {
  /// This function is the entry point for your reader
  static void readFile(const string& vrsFilePath) {
    RecordFileReader reader;
    if (reader.openFile(vrsFilePath) == 0) {
      std::vector<std::unique_ptr<StreamPlayer>> streamPlayers;
      // Map the devices referenced in the file to stream player objects
      // Just ignore the device(s) you do not care for
      for (auto id : reader.getStreams()) {
        unique_ptr<StreamPlayer> streamPlayer;
        switch (id.getTypeId()) {
          case RecordableTypeId::SlamCameraData:
          case RecordableTypeId::RgbCameraRecordableClass:
          case RecordableTypeId::EyeCameraRecordableClass:
            streamPlayer = std::make_unique<AriaImagePlayer>();
            break;
          case RecordableTypeId::SlamImuData:
          case RecordableTypeId::SlamMagnetometerData:
            streamPlayer = std::make_unique<AriaMotionSensorPlayer>();
            break;
          case RecordableTypeId::WifiBeaconRecordableClass:
            streamPlayer = std::make_unique<AriaWifiBeaconPlayer>();
            break;
          case RecordableTypeId::StereoAudioRecordableClass:
            streamPlayer = std::make_unique<AriaStereoAudioPlayer>();
            break;
          case RecordableTypeId::BluetoothBeaconRecordableClass:
            streamPlayer = std::make_unique<AriaBlueToothBeaconPlayer>();
            break;
          case RecordableTypeId::GpsRecordableClass:
            streamPlayer = std::make_unique<AriaGpsPlayer>();
            break;
          case RecordableTypeId::BarometerRecordableClass:
            streamPlayer = std::make_unique<AriaBarometerPlayer>();
            break;
          case RecordableTypeId::TimeRecordableClass:
            streamPlayer = std::make_unique<AriaTimeSyncPlayer>();
            break;
          default:
            fmt::print("Unexpected stream: {}, {}.\n", id.getNumericName(), id.getName());
            break;
        }
        if (streamPlayer) {
          reader.setStreamPlayer(id, streamPlayer.get());
          streamPlayers.emplace_back(move(streamPlayer));
        }
      }
      // We're ready: read all the records in order, and send them to the stream players registered
      reader.readAllRecords();
    }
  }
};

} // namespace aria_sample_reader

int main(int argc, char** argv) {
  aria_sample_reader::AriaFileReader::readFile("myAriaFile.vrs");
  return 0;
}
