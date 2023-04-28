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

#include "CompressionBenchmark.h"

#include <iomanip>
#include <iostream>

#include <vrs/ErrorCode.h>
#include <vrs/helpers/Strings.h>
#include <vrs/os/Time.h>
#include <vrs/os/Utils.h>

using namespace std;
using namespace vrs;
using namespace vrs::utils;

namespace vrs::utils {

int compressionBenchmark(FilteredFileReader& source, const CopyOptions& inOptions) {
  if (!source.spec.isDiskFile()) {
    cerr << "Benchmarks only available for local files." << endl;
    return FAILURE;
  }
  const string sourcePath = source.getPathOrUri();
  string sourceBasename = sourcePath;
  // remove ".vrs" suffix
  if (helpers::endsWith(sourceBasename, ".vrs")) {
    sourceBasename.resize(sourceBasename.size() - 4);
  }
  string masterPath = sourceBasename + "-uncompressed.vrs";
  // first, do a copy, with no compression at all, to get a baseline size
  FilteredFileReader master(masterPath);
  CopyOptions options{inOptions};
  options.setCompressionPreset(CompressionPreset::None);
  filterCopy(source, masterPath, options);
  int error = master.reader.openFile(masterPath);
  if (error == 0) {
    master.applyRecordableFilters({});
    master.applyTypeFilters({});
    int64_t sourceSize = master.reader.getTotalSourceSize();
    cout << os::getFilename(masterPath) << "\t" << helpers::humanReadableFileSize(sourceSize)
         << endl;
    string copyPath = sourceBasename + "-comp.vrs";
    double firstCompressionDuration = 0;
    for (int preset = static_cast<int>(CompressionPreset::CompressedFirst);
         preset <= static_cast<int>(CompressionPreset::CompressedLast);
         preset++) {
      options.setCompressionPreset(static_cast<CompressionPreset>(preset));
      double timeBefore = os::getTimestampSec();
      filterCopy(master, copyPath, options);
      double duration = os::getTimestampSec() - timeBefore;
      RecordFileReader outputFile;
      int copyError = outputFile.openFile(copyPath);
      if (copyError == 0) {
        int64_t copySize = outputFile.getTotalSourceSize();
        int64_t change = sourceSize - copySize;
        cout << vrs::toPrettyName(options.getCompression()) << "\t";
        int64_t processingSpeed = static_cast<int64_t>(sourceSize / duration);
        cout << helpers::humanReadableFileSize(processingSpeed) << "/s\t";
        if (change == 0) {
          cout << "No file size change." << endl;
        } else {
          if (change < 0) {
            cout << "File size increase\t";
            change = -change;
          }
          processingSpeed = static_cast<int64_t>(change / duration);
          cout << helpers::humanReadableFileSize(change) << "\t" << fixed << setprecision(2)
               << 100. * change / sourceSize << "%\t" << duration << " s\t"
               << helpers::humanReadableFileSize(processingSpeed) << "/s";
        }
        // Printout speed ratio, first compression to the current compression
        if (firstCompressionDuration <= 0) {
          firstCompressionDuration = duration;
          cout << "\t" << 1. << " (ref)"; // reference time: "1"...
        } else {
          cout << "\t" << duration / firstCompressionDuration;
        }
        cout << endl;
      } else {
        cerr << "Error compressing '" << copyPath << "'. Error #" << copyError << ": "
             << errorCodeToMessage(copyError) << endl;
      }
    }
  } else {
    cerr << "Could not copy '" << masterPath << "' for compression experiment. Error #" << error
         << ": " << errorCodeToMessage(error) << endl;
  }
  remove(masterPath.c_str());
  return error;
}

} // namespace vrs::utils
