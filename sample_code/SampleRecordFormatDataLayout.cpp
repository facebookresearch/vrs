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
#include <chrono>
#include <iostream>

#include <vrs/os/Time.h>

#include <vrs/DataLayout.h>
#include <vrs/DataLayoutConventions.h>
#include <vrs/RecordFormatStreamPlayer.h>
#include <vrs/RecordReaders.h>
#include <vrs/Recordable.h>

using namespace vrs;

namespace vrs_sample_code {

using datalayout_conventions::ImageSpecType;

/*
 * This sample code demonstrates the use of the RecordFormat & DataLayout.
 * For sample code showing how to setup a RecordFileReader or a RecordFileWriter,
 * please refer to Sample_RecordFile.cpp
 *
 * Sample camera device:
 *  - spec of images given in configuration records:
 *    Configuration records = 1 DataLayout block
 *  - metadata associated with each camera frame, itself as raw pixels:
 *    Data records = 1 DataLayout block + 1 image/raw block
 *
 */

/// Definition of the configuration records' metadata.
class MyCameraDataLayoutConfiguration : public AutoDataLayout {
 public:
  // Spec of a raw image, stored in data records (controlled by most recent config record)
  DataPieceValue<ImageSpecType> width{datalayout_conventions::kImageWidth};
  DataPieceValue<ImageSpecType> height{datalayout_conventions::kImageHeight};
  // Prefer to specify a storage type when storing an enum, to make the storage format explicit.
  DataPieceEnum<PixelFormat, ImageSpecType> pixelFormat{datalayout_conventions::kImagePixelFormat};

  // Additional configuration information for the camera
  DataPieceValue<uint32_t> cameraId{"camera_id"};
  DataPieceString cameraRole{"camera_role"};
  DataPieceValue<Point3Dd> cameraPosition{"camera_position"};

  AutoDataLayoutEnd endLayout;
};

/// Definition of the data records' metadata.
class MyCameraDataLayoutData : public AutoDataLayout {
 public:
  // Additional data provided with each frame
  DataPieceValue<double> exposureTime{"exposure_time"};
  DataPieceValue<double> arrivalTime{"arrival_time"};
  DataPieceValue<uint64_t> frameCounter{"frame_counter"};
  DataPieceValue<float> cameraTemperature{"camera_temperature"};
  DataPieceValue<float> roomTemperature{"room_temperature"};

  // SAMPLE TYPES
  //
  // The supported POD types are:
  //  - [int|uint][8|16|32|64],
  //  - float,
  //  - double,
  //  - Point[2|3|4][f|d],
  //  - Matrix[2|3|4][f|d].
  //
  // These POD types can be used with DataPieceValue<T>, DataPieceArray<T>, DataPieceVector<T> and
  // DataPieceStringMap<T>.
  //
  // Also supported:
  //  - DataPieceArray<string>,
  //  - DataPieceVector<string>,
  //  - DataPieceStringMap<string>.
  //
  // Note that you can *not* use an arbitrary POD struct of your choice, because DataLayout could
  // not help you manage changes to that POD definition, which would defeat the purpose.
  // Instead, create a top level field, using names creating a namespace of kind:
  // Instead of doing:
  //   DataPieceValue<struct {int counter; float time;}> myStruct{"my_struct"};
  // do:
  //   DataPieceValue<int> myStructCounter{"my_struct_counter"};
  //   DataPieceValue<float> myStructTime{"my_struct_time"};
  //
  // Yes, it's more verbose, and yes, it prevents you from storing your internal data formats,
  // but the truth is that using your internal data format for storage is a colossal design blunder
  // that should never ever make, as the day someone changes that data structure in any way,
  // you will "lose" support for your old files, without warning!
  //
  DataPieceValue<uint32_t> oneValue{"one_value"}; // a single POD value
  DataPieceArray<Matrix3Dd> arrayOfMatrix3Dd{"matrices", 3}; // fixed size array of POD values
  DataPieceVector<Point3Df> vectorOfPoint3Df{"points"}; // vector of POD values or string values
  DataPieceVector<string> vectorOfString{"strings"}; // Yes, vector of strings is supported!
  DataPieceString aString{"some_string"}; // a string
  DataPieceStringMap<Matrix4Dd> aStringMatrixMap{"some_string_matrix_map"};
  DataPieceStringMap<string> aStringStringMap{"some_string_string_map"};

  AutoDataLayoutEnd endLayout;
};

/// Definition of some obsolete metadata.
class MyCameraDataLayoutLegacyData : public AutoDataLayout {
 public:
  // Additional (made-up) data that used to be present
  DataPieceValue<double> otherTime{"other_time"};
  DataPieceValue<uint32_t> frameCounter{"frame_counter"}; // same name, different type

  AutoDataLayoutEnd endLayout;
};

/// Class to generate a stream of sample records.
class MyCameraRecordable : public Recordable {
  // Record format version numbers describe the overall record format.
  // Note DataLayout field changes do *not* require to change the record format version.
  static const uint32_t kConfigurationRecordFormatVersion = 1;
  static const uint32_t kDataRecordFormatVersion = 1;

 public:
  MyCameraRecordable() : Recordable(RecordableTypeId::SampleDevice) {
    // Ideal place to define the record format & data layout descriptions
    addRecordFormat(
        Record::Type::CONFIGURATION,
        kConfigurationRecordFormatVersion,
        config_.getContentBlock(),
        {&config_});
    addRecordFormat(
        Record::Type::DATA,
        kDataRecordFormatVersion,
        data_.getContentBlock() + ContentBlock(ImageFormat::RAW),
        {&data_});
  }
  const Record* createConfigurationRecord() override {
    // write the description of the device in the config_ DataLayout
    config_.width.set(1080);
    config_.height.set(768);
    config_.pixelFormat.set(PixelFormat::GREY8);
    config_.cameraId.set(1);
    config_.cameraRole.stage("top_pointing_down");
    config_.cameraPosition.set({100, 123.45678, -256.1256987});

    // create a record using that data
    return createRecord(
        os::getTimestampSec(),
        Record::Type::CONFIGURATION,
        kConfigurationRecordFormatVersion,
        DataSource(config_));
  }
  const Record* createStateRecord() override {
    // Best practice is to always create a record when asked, with a reasonable timestamp,
    // even if the record is empty.
    return createRecord(os::getTimestampSec(), Record::Type::STATE, 0);
  }
  void createDataRecords(const uint8_t* pixelData) {
    data_.arrivalTime.set(os::getTimestampSec());
    data_.exposureTime.set(2.5);
    data_.frameCounter.set(25);
    data_.cameraTemperature.set(38.5f);
    data_.roomTemperature.set(25.9f);

    // create a record using that data
    createRecord(
        os::getTimestampSec(),
        Record::Type::DATA,
        kDataRecordFormatVersion,
        DataSource(data_, {pixelData, config_.width.get() * config_.height.get()}));
  }

 private:
  MyCameraDataLayoutConfiguration config_;
  MyCameraDataLayoutData data_;
};

/// Class to consume records read from a file
class MyCameraStreamPlayer : public RecordFormatStreamPlayer {
  bool onDataLayoutRead(const CurrentRecord& record, size_t blockIndex, DataLayout& readData)
      override {
    // readData is the datalayout read from the disk, as described in the VRS file.
    // It might be the same, older or newer compared to what your current definition.
    // The getExpectedLayout<MyDataLayout> API gives you a datalayout matching your current
    // definition, mapped to the disk's datalayout. You can efficiently access all the fields, but
    // beware that some fields may be missing, if older definitions did not include them...
    // Use the isAvailable() method to tell if a DataPiece introduced later is present or not.
    switch (record.recordType) {
      case Record::Type::CONFIGURATION: {
        MyCameraDataLayoutConfiguration& myConfig =
            getExpectedLayout<MyCameraDataLayoutConfiguration>(readData, blockIndex);
        // use the data...
        myConfig.cameraRole.get(); // access the data...
      } break;

      case Record::Type::DATA: {
        // Here are the fields written & expected in the latest version
        MyCameraDataLayoutData& myData =
            getExpectedLayout<MyCameraDataLayoutData>(readData, blockIndex);
        // For fields that are no longer present, but we might need for some compatibility reason
        MyCameraDataLayoutLegacyData& legacyData =
            getLegacyLayout<MyCameraDataLayoutLegacyData>(readData, blockIndex);
        // use the data...
        myData.cameraTemperature.get();
        // The type of frame_counter was changed: use the old version if necessary
        uint64_t frameCounter = myData.frameCounter.isAvailable() ? myData.frameCounter.get()
                                                                  : legacyData.frameCounter.get();
        std::cout << frameCounter; // use variable to make linter happy...
      } break;

      default:
        assert(false); // should not happen, but you want to know if it does!
        break;
    }
    return true; // read next blocks, if any
  }

  // When a RecordFormat image block definition isn't specific enough to describe a raw image
  // format, VRS will search for image spec definitions automatically, following the procedure
  // described in DataLayoutConventions.h.
  bool onImageRead(const CurrentRecord& record, size_t blockIndex, const ContentBlock& block)
      override {
    // Unlike with VRS 1.0 APIs, the image data was not read yet: allocate your own buffer & read!
    size_t frameByteCount = block.getBlockSize();
    assert(frameByteCount != 0); // Should not happen, but you want to know if it does!
    assert(frameByteCount != ContentBlock::kSizeUnknown); // Should not happen either...
    std::vector<uint8_t> frameBytes(frameByteCount);
    // Synchronously read the image data, all at once, line-by-line, byte-by-byte, as you like...
    if (record.reader->read(frameBytes) == 0) {
      // do something with the image...
    }
    return true; // read next blocks, if any
  }
};

} // namespace vrs_sample_code
