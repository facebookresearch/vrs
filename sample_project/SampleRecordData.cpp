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

#include <vrs/os/Time.h>

// A DEFAULT_LOG_CHANNEL needs to be defined before importing the vrs/oss/logging library
#define DEFAULT_LOG_CHANNEL "SampleVrsProject"
#include <vrs/oss/logging/Verify.h>

#include <vrs/DataLayout.h>
#include <vrs/DataPieces.h>
#include <vrs/RecordFileWriter.h>
#include <vrs/RecordFormat.h>
#include <vrs/Recordable.h>

using namespace vrs;
using namespace std;

/*
 * This sample code demonstrates a basic VRS app that records a set of arbitrary data to a VRS file.
 * The point of this code is be used in the sample_project to show how one can compile and link to
 * the VRS library using CMake in their own project.
 *
 * For in-depth information and examples on how to properly use the VRS library please see files in
 * the sample_app and sample_code directories.
 */

namespace vrs_sample_project {

/// Definition of some trivial metadata
class MyMetadata : public AutoDataLayout {
 public:
  DataPieceValue<uint32_t> sensorValue{"my_sensor"};

  AutoDataLayoutEnd endLayout;
};

constexpr const char* kSampleFlavor = "team/vrs/sample";

/// Sample device recording some trivial metadata.
class RecordableDemo : public Recordable {
  static const uint32_t kDataRecordFormatVersion = 1;

 public:
  // declare your device's unique id in RecordableTypeId.h
  RecordableDemo() : Recordable(RecordableTypeId::SampleDeviceRecordableClass, kSampleFlavor) {
    // define your RecordFormat & DataLayout definitions for this stream
    addRecordFormat(
        Record::Type::DATA, // the type of records this definition applies to
        kDataRecordFormatVersion, // a record format version
        metadata_.getContentBlock(), // the RecordFormat definition
        {&metadata_}); // the DataLayout definition for the datalayout content block declared above.
  }

  // When appropriate, you will be requested to give the configuration of your device/module
  // Create a state record to restore the internal state of your recordable on playback
  // Note: always provide a record, even if you don't need to save anything (as shown here).
  const Record* createConfigurationRecord() override {
    // Use the same time source for ALL your records in the entire file!
    // In this sample, the record has no payload.
    double someTimeInSec = os::getTimestampSec();
    return createRecord(someTimeInSec, Record::Type::CONFIGURATION, 0);
  }

  // When appropriate, you will be requested to give the state of your device/module
  // Create a state record to restore the internal state of your recordable on playback
  // Note: always provide a record, even if you don't need to save anything (as shown here).
  const Record* createStateRecord() override {
    double someTimeInSec = os::getTimestampSec();
    return createRecord(someTimeInSec, Record::Type::STATE, 0);
  }

  // This method demonstrates how the recordable creates a metadata record
  void createDataRecord(uint32_t sensorValue) {
    metadata_.sensorValue.set(sensorValue); // Record the value we want to save in the record
    // Use the same time source for ALL your records in the entire file!
    double someTimeInSec = os::getTimestampSec();
    createRecord(
        someTimeInSec, Record::Type::DATA, kDataRecordFormatVersion, DataSource(metadata_));
  }

 private:
  MyMetadata metadata_;
};

} // namespace vrs_sample_project

using namespace vrs_sample_project;

int main() {
  // Make the file & attach the recordable
  RecordFileWriter fileWriter;
  RecordableDemo recordable;
  fileWriter.addRecordable(&recordable);

  // Use a simple VRS file synchronous creation method where we do the following:
  //   Step 1: create all the records in memory
  //   Step 2: write them all at once in a single big blocking call!
  //
  // For other VRS data writing methods please see sample_code/SampleRecordAndPlay.cpp

  // Create a bunch of arbitrary records.
  for (size_t k = 0; k < 100; k++) {
    recordable.createDataRecord(k);
  }

  // Close the file & wait for the data to be written out
  XR_VERIFY(fileWriter.writeToFile("my_record_file.vrs") == 0);

  return 0;
}
