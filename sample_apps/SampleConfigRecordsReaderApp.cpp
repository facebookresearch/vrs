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
#include <map>

#define DEFAULT_LOG_CHANNEL "SampleConfigRecordsReaderApp"
#include <logging/Log.h>
#include <logging/Verify.h>

#include <vrs/ErrorCode.h>
#include <vrs/RecordFileReader.h>
#include <vrs/RecordFormatStreamPlayer.h>
#include <vrs/os/Utils.h>

#include "SharedDefinitions.h"

using namespace std;
using namespace vrs;
using namespace vrs::DataLayoutConventions;
using namespace vrs_sample_apps;

/// Sample app, to show how to get data from configuration records, maybe to setup a replay.
/// Ideally, such data would have been stored in stream tags, which are all available when the file
/// is open without needing any complicated code, but in practice, it's easy to get stuck, and to
/// need data found in different configuration records.
/// So here, you open the file,

int main() {
  RecordFileReader reader;
  int status = reader.openFile(os::getHomeFolder() + kSampleFileName);
  if (status != 0) {
    XR_LOGE("Can't open file {}, error: {}", kSampleFileName, errorCodeToMessage(status));
    return status;
  }

  // use an anonymous struct object to collect the data we care about
  struct : public RecordFormatStreamPlayer {
    // fields to collect. Using map or vectors, to be sure data was actually read from records.
    map<uint16_t, vector<float>> calibrations;

    bool onDataLayoutRead(const CurrentRecord& record, size_t index, DataLayout& dl) override {
      CameraStreamConfig& config = getExpectedLayout<CameraStreamConfig>(dl, index);
      if (config.cameraCalibration.isAvailable()) {
        // we chose to gather all the calibrations in a map
        config.cameraCalibration.get(calibrations[record.streamId.getInstanceId()]);
      }
      return false; // we can skip the end of this record
    }
  } collector;

  // read all the configuration records at once...
  reader.readFirstConfigurationRecordsForType(
      RecordableTypeId::ForwardCameraRecordableClass, &collector);

  // this is simply to prove that the fields were found, as we expected them...
  map<uint16_t, vector<float>> expectedCalibrations{{1, CALIBRATION_VALUES}};
  assert(collector.calibrations == expectedCalibrations);

  return 0;
}
