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

#pragma once

#include <string>

#include <vrs/utils/FilterCopy.h>
#include <vrs/utils/RecordFileInfo.h>
#include <vrs/utils/Validation.h>
#include <vrs/utils/cli/PrintRecordFormatRecords.h>

namespace vrscli {
void printHelp(const std::string& appName);
void printSamples(const std::string& appName);

enum class Command {
  None,
  Help,
  Details,
  Copy,
  Merge,
  Check,
  Checksum,
  Checksums,
  ChecksumVerbatim,
  Hexdump,
  Decode,
  Compare,
  CompareVerbatim,
  Debug,
  PrintRecordFormats,
  ListRecords,
  PrintRecords,
  PrintRecordsDetailed,
  PrintRecordsJson,
  PrintRecordsJsonPretty,
  Rage,
  ExtractImages,
  ExtractAudio,
  ExtractAll,
  JsonDescription,
  FixIndex,
  CompressionBenchmark,

  COUNT
};

struct VrsCommand {
  VrsCommand();

  bool parseCommand(const std::string& appName, const char* cmdName);

  /// Parse a "logical" command line argument, which may have one or more parts.
  /// @param appName: Name of the application binary for error messages
  /// @param argn: First argument to look at. Might be updated to "process" additional parameters.
  /// @param argc: Max argument count.
  /// @param argv: All the command line arguments: the argument to parse is argv[argn]
  /// @param outStatusCode: Set on exit, if some error occurred, untouched otherwise.
  /// @return False if the parameter was not recognized (argn & outStatusCode were not changed).
  /// Returns true if the argument was recognized, which doesn't mean there was no error.
  /// If an argument error of any kind is found (invalid type, path to a file that's missing, etc),
  /// then outStatusCode is set to a non-zero value, and parsing & execution should be aborted, and
  /// the only optional action to be taken is if showHelp was also set, in which case the tool's
  /// help should be shown.
  /// On the other hand, if the argument and possible sub-arguments were valid,
  /// outStatusCode is unchanged (presumably, it's still set to EXIT_SUCCESS),
  /// and the next argument parameter to parse is at argn + 1.
  bool
  parseArgument(const std::string& appName, int& argn, int argc, char** argv, int& outStatusCode);

  /// Handle a parameter not recognized by parseArgument and potential additional parsing.
  /// Unrecognized arguments are expected to be additional file paths for merge operations.
  /// @param appName: Name of the application binary for error messages
  /// @param arg: the argument.
  bool processUnrecognizedArgument(const std::string& appName, const std::string& arg);

  /// Try to open the file given as part of the arguments.
  /// @return True if the file was opened successfully (meaning, it's a VRS file, valid so far).
  /// Returns false otherwise.
  bool openVrsFile();
  /// Try to open the additional file paths.
  /// @return True if the file was opened successfully (meaning, it's a VRS file, valid so far).
  /// Returns false otherwise.
  bool openOtherVrsFile(
      vrs::utils::FilteredFileReader& filteredReader,
      vrs::RecordFileInfo::Details details);
  /// Some operations can take multiple VRS files as input. This opens the other VRS files,
  /// while applying the filters on those as well.
  bool openOtherVrsFiles(vrs::RecordFileInfo::Details details);
  /// Apply the recorded filters
  void applyFilters(vrs::utils::FilteredFileReader& filteredReader);
  /// Run the commands requested using the member variables below
  /// @return 0, if no error should be signaled back to the caller of the tool,
  /// or some non-zero value if an error should be signaled.
  /// Note that the actual error value returned may or may not be meaningful,
  /// as it might be the system's default EXIT_FAILURE code.
  int runCommands();

  /// Perform Copy, Merge operation
  int doCopyMerge();

  bool isRemoteFileSystem(const std::string& path);

  /// Main operation
  Command cmd = Command::None;

  /// Source file and its filters
  vrs::utils::FilteredFileReader filteredReader;
  vrs::utils::RecordFilterParams filters;
  /// Helper to use when setting up decimation, to make sure it's created & initialized before use
  vrs::utils::DecimationParams& getDecimatorParams();

  /// Force showing the tool's help documentation
  bool showHelp = false;

  /// Misc flags and options for copy and merge operations, but also other operations.
  vrs::utils::CopyOptions copyOptions;
  vrs::utils::MakeStreamFilterFunction copyMakeStreamFilterFunction = vrs::utils::makeCopier;

  /// Target location for copy, merge, extract operations specified with the '--to' option
  std::string targetPath;

  /// Additional input files for merge operations
  std::list<vrs::utils::FilteredFileReader> otherFilteredReaders;

  /// Extract raw images as ".raw" files instead of converting them to png.
  bool extractImagesRaw = false;
};

} // namespace vrscli
