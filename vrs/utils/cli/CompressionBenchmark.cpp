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

#include <vrs/utils/cli/CompressionBenchmark.h>

#include <fmt/core.h>

#include <vrs/ErrorCode.h>
#include <vrs/helpers/Strings.h>
#include <vrs/os/Time.h>
#include <vrs/os/Utils.h>

using namespace std;
using namespace vrs;
using namespace vrs::utils;

namespace vrs::utils {

namespace {

constexpr const char* kRowFormat = "{:<18}  {:>9} {:>6}    {:>10}  {:>10}  {:>9}  {:>8}\n";
constexpr int kRowWidth = 18 + 2 + 9 + 1 + 6 + 4 + 10 + 2 + 10 + 2 + 9 + 2 + 8;

void printSeparator() {
  fmt::print("{:-<{}}\n", "", kRowWidth);
}

} // namespace

int compressionBenchmark(FilteredFileReader& source, const CopyOptions& inOptions) {
  if (!source.spec.isDiskFile()) {
    fmt::print(stderr, "Benchmarks only available for local files.\n");
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
  options.showProgress = false;
  options.setCompressionPreset(CompressionPreset::None);
  filterCopy(source, masterPath, options);
  master.reader.setOpenProgressLogger(nullptr);
  int error = master.reader.openFile(masterPath);
  if (error == 0) {
    master.applyRecordableFilters({});
    master.applyTypeFilters({});
    int64_t sourceSize = master.reader.getTotalSourceSize();
    fmt::print(
        "Source: {} ({})\n\n",
        os::getFilename(masterPath),
        helpers::humanReadableFileSize(sourceSize));
    string copyPath = sourceBasename + "-comp.vrs";
    double firstCompressionDuration = 0;
    fmt::print(kRowFormat, "Preset", "Size", "Ratio", "Comp", "Decomp", "Time", "Relative");
    printSeparator();
    CompressionPreset lastPreset = CompressionPreset::Undefined;
    string decompPath = sourceBasename + "-decomp.vrs";
    for (int preset = static_cast<int>(CompressionPreset::CompressedFirst);
         preset <= static_cast<int>(CompressionPreset::CompressedLast);
         preset++) {
      auto currentPreset = static_cast<CompressionPreset>(preset);
      // Print a separator between codec families
      if (lastPreset != CompressionPreset::Undefined &&
          lastPreset <= CompressionPreset::LastLz4Preset &&
          currentPreset >= CompressionPreset::FirstZstdPreset) {
        printSeparator();
      }
      lastPreset = currentPreset;
      options.setCompressionPreset(currentPreset);
      double timeBefore = os::getTimestampSec();
      filterCopy(master, copyPath, options);
      double duration = os::getTimestampSec() - timeBefore;
      // Open compressed file to get its size and measure decompression speed
      FilteredFileReader compReader(copyPath);
      compReader.reader.setOpenProgressLogger(nullptr);
      int copyError = compReader.reader.openFile(copyPath);
      if (copyError == 0) {
        compReader.applyRecordableFilters({});
        compReader.applyTypeFilters({});
        int64_t copySize = compReader.reader.getTotalSourceSize();
        string presetName = vrs::toPrettyName(options.getCompression());
        string compSpeed =
            helpers::humanReadableFileSize(static_cast<int64_t>(sourceSize / duration)) + "/s";
        // Measure decompression speed
        CopyOptions decompOptions;
        decompOptions.showProgress = false;
        decompOptions.setCompressionPreset(CompressionPreset::None);
        double decompBefore = os::getTimestampSec();
        filterCopy(compReader, decompPath, decompOptions);
        double decompDuration = os::getTimestampSec() - decompBefore;
        string decompSpeed =
            helpers::humanReadableFileSize(static_cast<int64_t>(sourceSize / decompDuration)) +
            "/s";
        remove(decompPath.c_str());
        string sizeLabel = helpers::humanReadableFileSize(copySize);
        string ratio = fmt::format("{:.2f}x", static_cast<double>(sourceSize) / copySize);
        string time = fmt::format("{:.2f} s", duration);
        string relative;
        if (firstCompressionDuration <= 0) {
          firstCompressionDuration = duration;
          relative = "1.00";
        } else {
          relative = fmt::format("{:.2f}", duration / firstCompressionDuration);
        }
        fmt::print(
            kRowFormat, presetName, sizeLabel, ratio, compSpeed, decompSpeed, time, relative);
      } else {
        fmt::print(
            stderr,
            "Error compressing '{}'. Error #{}: {}\n",
            copyPath,
            copyError,
            errorCodeToMessage(copyError));
      }
    }
    printSeparator();
  } else {
    fmt::print(
        stderr,
        "Could not copy '{}' for compression experiment. Error #{}: {}\n",
        masterPath,
        error,
        errorCodeToMessage(error));
  }
  remove(masterPath.c_str());
  return error;
}

} // namespace vrs::utils
