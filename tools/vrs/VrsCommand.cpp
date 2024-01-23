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

#include "VrsCommand.h"

#include <cstdlib>
#include <cstring>

#include <iostream>
#include <string>

#include <fmt/format.h>

#define DEFAULT_LOG_CHANNEL "VrsCommand"
#include <logging/Log.h>
#include <logging/Verify.h>

#include <vrs/ErrorCode.h>
#include <vrs/FileCache.h>
#include <vrs/FileHandlerFactory.h>
#include <vrs/helpers/EnumStringConverter.h>
#include <vrs/helpers/Rapidjson.hpp>
#include <vrs/helpers/Strings.h>
#include <vrs/os/Time.h>
#include <vrs/os/Utils.h>

#include <vrs/utils/cli/CliParsing.h>
#include <vrs/utils/cli/CompressionBenchmark.h>
#include <vrs/utils/cli/DataExtraction.h>
#include <vrs/utils/cli/ListRecords.h>
#include <vrs/utils/cli/MakeZeroFilterCopier.h>
#include <vrs/utils/cli/PrintRecordFormatRecords.h>
#include <vrs/utils/cli/PrintRecordFormats.h>

using namespace std;
using namespace vrs;
using namespace vrs::utils;
using vrs::RecordFileInfo::Details;

namespace {

using namespace vrscli;

const char* sCommands[] = {
    "none",
    "help",
    "details",
    "copy",
    "merge",
    "check",
    "checksum",
    "checksums",
    "checksum-verbatim",
    "hexdump",
    "decode",
    "compare",
    "compare-verbatim",
    "debug",
    "record-formats",
    "list",
    "print",
    "print-details",
    "print-json",
    "print-json-pretty",
    "rage",
    "extract-images",
    "extract-audio",
    "extract-all",
    "json-description",
    "fix-index",
    "compression-benchmark",
};
struct CommandConverter : public EnumStringConverter<
                              Command,
                              sCommands,
                              COUNT_OF(sCommands),
                              Command::None,
                              Command::None> {
  static_assert(COUNT_OF(sCommands) == enumCount<Command>(), "Missing GaiaOp name definitions");
};

const RecordFileInfo::Details kCopyOpsDetails = Details::MainCounters;

struct CommandSpec {
  Command cmd;
  uint32_t maxFiles{1};
  Details fileDetails = Details::None;
  bool mainFileIsVRS{true};
};

const CommandSpec& getCommandSpec(Command cmd) {
  static const vector<CommandSpec> sCommandSpecs = {
      {Command::None, 0},
      {Command::Help, 0},
      {Command::Details, 1, Details::Everything},
      {Command::Copy, 1000, kCopyOpsDetails},
      {Command::Merge, 1000, kCopyOpsDetails},
      {Command::Check, 1, Details::MainCounters},
      {Command::Checksum, 1},
      {Command::Checksums, 1},
      {Command::ChecksumVerbatim, 1, Details::None, false},
      {Command::Hexdump, 1},
      {Command::Decode, 1, Details::MainCounters},
      {Command::Compare, 1000, Details::MainCounters},
      {Command::CompareVerbatim, 1000, Details::None, false},
      {Command::Debug, 1, Details::None, false},
      {Command::PrintRecordFormats, 1},
      {Command::ListRecords, 1},
      {Command::PrintRecords, 1},
      {Command::PrintRecordsDetailed, 1},
      {Command::PrintRecordsJson, 1},
      {Command::PrintRecordsJsonPretty, 1},
      {Command::Rage, 1, Details::Everything},
      {Command::ExtractImages, 1},
      {Command::ExtractAudio, 1},
      {Command::ExtractAll, 1},
      {Command::JsonDescription, 1},
      {Command::FixIndex, 1000, Details::Basics},
      {Command::CompressionBenchmark, 1},
  };
  if (!XR_VERIFY(cmd > Command::None && cmd < Command::COUNT)) {
    return sCommandSpecs[static_cast<size_t>(Command::None)];
  }
  return sCommandSpecs[static_cast<size_t>(cmd)];
}

} // namespace

#define CMD(H, C) H ":\n  " << appName << " " C "\n"

namespace vrscli {

void printHelp(const string& appName) {
  cout
      << CMD("Get details about a VRS files", "[ file.vrs ] [filter-options]")

      << "\n"
      << CMD("All the other commands have the following format", "<command> [ arguments ]*")

      << "\n"
      << CMD("Show this documentation", "help")

      << "\n"
      << CMD("Copy all the streams from one or more files into one",
             "copy [ vrsfiles.vrs ]+ --to <target.vrs> [copy-options] [tag-options] [filter-options]")
      << CMD("Merge all the streams from one or more files into one",
             "merge [ vrsfiles.vrs ]+ --to <target.vrs> [copy-options] [tag-options] [filter-options]")
      << CMD("Copy all the data from a file into a new one, but with blanked/zeroed image and audio data,\n"
             "so the copy is much smaller because of lossless compression",
             "copy --zero-vrs <file.vrs> --to <output.vrs>")

      << "\n"
      << CMD("List records, with their timestamp, stream name and identifier, and record type.",
             "list <file.vrs> [filter-options]")
      << CMD("Show RecordFormat and DataLayout definitions", "record-formats <file.vrs>")
      << CMD("Print records using RecordFormat & DataLayout", "print <file.vrs> [filter-options]")
      << CMD("Print records with details using RecordFormat & DataLayout",
             "print-details <file.vrs> [filter-options]")
      << CMD("Print records as json using RecordFormat & DataLayout",
             "print-json <file.vrs> [filter-options]")
      << CMD("Print records as json-pretty using RecordFormat & DataLayout",
             "print-json-pretty <file.vrs> [filter-options]")
      << CMD("Print detailed file info and first records for one-stop diagnostic purposes",
             "rage <file.vrs>")

      << "\n"
      << CMD("Extract images in a folder. jpg and png are extracted as is.\n"
             "RAW images are saved as GREY8, GREY16, RGB8 or RGBA8 png files,\n"
             "or as .raw image files without any conversion with the --raw-images option.",
             "extract-images file.vrs [ --to <folder_path> ] [ --raw-images ] [filter-options]                        ")
      << CMD("Extract audio data as WAVE file(s) in a folder",
             "extract-audio file.vrs [ --to <folder_path> ] [filter-options]")
      << CMD("Extract images, audio, and meta data in a folder",
             "extract-all file.vrs [ --to <folder_path> ] [filter-options]")

      << "\n"
      << CMD("Check that a file can be read (checks integrity)",
             "check <file.vrs> [filter-options]")
      << CMD("Check that a file can be decoded (record-format integrity and image decompression)",
             "decode <file.vrs> [filter-options]")
      << CMD("Calculate a checksum for the whole file, at the VRS data level",
             "checksum <file.vrs> [filter-options]")
      << CMD("Calculate checksums for each part of the VRS file",
             "checksums <file.vrs> [filter-options]")
      << CMD("Calculate a checksum for the whole file, at the raw level (VRS or not)",
             "checksum-verbatim <file.vrs> [filter-options]")
      << CMD("Calculate checksums for each part of the VRS file, print records in hex",
             "hexdump <file.vrs> [filter-options]")
      << CMD("Compare a VRS file to one or more files, at the VRS data logical level",
             "compare <original.vrs> [others.vrs]+ [filter-options]")
      << CMD("Compare two files at the raw level (VRS or not)",
             "compare-verbatim <original.vrs> <other.vrs>")

      << "\n"
      << CMD("Compute some lossless compression benchmarks", "compression-benchmark <file.vrs>")

      << "\n"
      << "Special Commands:\n"
      << CMD("Fix VRS index in place, if necessary. MIGHT MODIFY THE ORIGINAL FILES IF NEEDED.",
             "fix-index <file.vrs> [<file2.vrs>+")
      << CMD("Print VRS file format debug information", "debug <file.vrs>")

      << "\n"
      << "Filter options:\n";
  printTimeAndStreamFiltersHelp();
  printDecimationOptionsHelp();

  cout << "\n"
          "Copy options:\n";
  printCopyOptionsHelp();

  cout << "\n"
          "Tag override options:\n";
  printTagOverrideOptionsHelp();
}

#define SP(x) "  " << appName << " " x "\n"

void printSamples(const string& appName) {
  cout << "\n"
       << "Examples:\n"
       << "To peek at what's inside a recording:\n"
       << SP("src.vrs")

       << "To list records (basic details):\n"
       << SP("list src.vrs")

       << "To peek at what's inside a recording and print as json:\n"
       << SP("json-description src.vrs")

       << "To print configuration records as json:\n"
       << SP("print-json src.vrs + configuration")

       << "To print device id 1001's configuration & state records as json:\n"
       << SP("print-json src.vrs + 1001 - data")

       << "Copy & clean-up a recording with default compression and rebuilding the index:\n"
       << SP("copy src.vrs --to cleanedRecording.vrs")

       << "Recompress a recording with a tighter compression than default:\n"
       << SP("copy src.vrs --to tight.vrs --compression=ztight")

       << "Remove all ImageStream streams:\n"
       << SP("src.vrs # to see that '100' is ImageStream...")
       << SP("copy src.vrs --to noImageStream.vrs - 100")

       << "Extract only two specific streams out of many streams:\n"
       << SP("src.vrs # to find the ids of the streams we want, for instance 100-1 and 101-1")
       << SP("copy src.vrs --to extract.vrs + 100-1 + 101-1")

       << "Trim data records in the first 2 seconds and the last second of a recording:\n"
       << SP("copy src.vrs --to extract.vrs --range +2 -1")

       << "Copy multiple VRS files into a single one, keeping all streams separate:\n"
       << SP("copy first.vrs second.vrs third.vrs --to new.vrs")

       << "Merge multiple VRS files into a single one, merging streams by type:\n"
       << SP("merge first.vrs second.vrs third.vrs --to new.vrs")

       << "Extract all images as images files:\n"
       << SP("extract-images file.vrs --to imageFolder")

       << "Save all ImageStream images, recorded in the first 5 seconds:\n"
       << SP("src.vrs # to see that '100' is ImageStream...")
       << SP("extract-images file.vrs --to imageFolder + 100 --before +5")

       << "\n";
}

VrsCommand::VrsCommand() {
  // Detect if we're running from Qt Creator or Nuclide, in which case we don't want file copy
  // operations to show progress, since those terminal outputs doesn't support overwrites...
  // This might not work on all platforms, but that's ok, as it's only a nice to have...
  const char* xpcServiceName = getenv("XPC_SERVICE_NAME");
  if (xpcServiceName != nullptr &&
      (strstr(xpcServiceName, "qtcreator") != nullptr ||
       strstr(xpcServiceName, "Qt Creator") != nullptr)) {
    copyOptions.showProgress = false;
  }
  // Detect running inside Nuclide
  const char* term = getenv("TERM");
  if (term != nullptr && strstr(term, "nuclide") != nullptr) {
    copyOptions.showProgress = false;
  }
}

bool VrsCommand::parseCommand(const std::string& appName, const char* cmdName) {
  cmd = CommandConverter::toEnum(cmdName);
  if (cmd != Command::None) {
    return XR_VERIFY(getCommandSpec(cmd).cmd == cmd);
  }
  if (processUnrecognizedArgument(appName, cmdName)) {
    cmd = Command::Details;
    return XR_VERIFY(getCommandSpec(cmd).cmd == cmd);
  }
  cerr << appName << ": '" << cmdName << "' is neither a known command name nor a valid path.\n";
  return false;
}

bool VrsCommand::parseArgument(
    const string& appName,
    int& argn,
    int argc,
    char** argv,
    int& outStatusCode) {
  string arg = argv[argn];
  if (arg == "-to" || arg == "--to") {
    if (++argn < argc) {
      targetPath = argv[argn];
    } else {
      cerr << appName << ": error. '--copy|-c' requires a destination path.\n";
      outStatusCode = EXIT_FAILURE;
    }
  } else if (arg == "--raw-images") {
    extractImagesRaw = true;
  } else if (arg == "--zero-vrs") {
    copyMakeStreamFilterFunction = makeZeroFilterCopier;
  } else {
    return parseCopyOptions(appName, arg, argn, argc, argv, outStatusCode, copyOptions) ||
        parseTagOverrideOptions(appName, arg, argn, argc, argv, outStatusCode, copyOptions) ||
        parseTimeAndStreamFilters(
               appName, arg, argn, argc, argv, outStatusCode, filteredReader, filters) ||
        parseDecimationOptions(appName, arg, argn, argc, argv, outStatusCode, filters);
  }
  return true;
}

bool VrsCommand::processUnrecognizedArgument(const string& appName, const string& arg) {
  if (!arg.empty() && arg.front() == '-') {
    cerr << appName << ": Invalid argument: '" << arg << "'\n";
    return false;
  }
  FileSpec spec;
  bool isAcceptable = false;
  if (spec.fromPathJsonUri(arg) == 0) {
    if (spec.isDiskFile()) {
      isAcceptable = os::isFile(arg);
    } else {
      unique_ptr<FileHandler> fileHandler =
          FileHandlerFactory::getInstance().getFileHandler(spec.fileHandlerName);
      if (fileHandler) {
        if (fileHandler->isRemoteFileSystem()) {
          isAcceptable = true; // trust that the object exists
        } else {
          isAcceptable = os::isFile(arg);
        }
      }
    }
  }
  if (!isAcceptable) {
    cerr << appName << ": Invalid file path: '" << arg << "'\n";
    return false;
  }
  size_t maxFileCount = (cmd == Command::None) ? 1 : getCommandSpec(cmd).maxFiles;
  size_t fileCount = filteredReader.spec.empty() ? 0 : (1 + otherFilteredReaders.size());
  if (fileCount < maxFileCount) {
    if (filteredReader.spec.empty()) {
      filteredReader.setSource(arg);
    } else {
      otherFilteredReaders.emplace_back(arg);
    }
  } else {
    cerr << appName << ": Too many file parameters.\n";
    return false;
  }
  return true;
}

bool VrsCommand::openVrsFile() {
  const auto& cmdSpec = getCommandSpec(cmd);
  if (cmdSpec.maxFiles < 1) {
    return true;
  }
  if (filteredReader.spec.empty()) {
    cerr << "Missing VRS file arguments.\n";
    return false;
  }
  if (getCommandSpec(cmd).mainFileIsVRS) {
    return filteredReader.reader.openFile(filteredReader.spec, cmd == Command::FixIndex) == 0;
  }
  return true;
}

bool VrsCommand::openOtherVrsFile(
    FilteredFileReader& otherReader,
    RecordFileInfo::Details details) {
  if (!otherReader.reader.isOpened()) {
    // Open the reader, apply the filters and print their overview
    if (otherReader.reader.openFile(otherReader.spec, cmd == Command::FixIndex) != 0) {
      cerr << "Error: could not open " << otherReader.spec.getEasyPath() << "\n";
      return false;
    }
    otherReader.filter = filteredReader.filter;
    if (filters.decimationParams) {
      otherReader.decimator_ = make_unique<Decimator>(otherReader, *filters.decimationParams);
    }
    applyFilters(otherReader);
    if (details != Details::None) {
      RecordFileInfo::printOverview(cout, otherReader.reader, otherReader.filter.streams, details);
    }
  }
  return true;
}

bool VrsCommand::openOtherVrsFiles(RecordFileInfo::Details details) {
  for (FilteredFileReader& otherReader : otherFilteredReaders) {
    if (!openOtherVrsFile(otherReader, details)) {
      return false;
    }
  }
  return true;
}

void VrsCommand::applyFilters(FilteredFileReader& reader) {
  reader.applyFilters(filters);
}

int VrsCommand::runCommands() {
  int statusCode = EXIT_SUCCESS;
  applyFilters(filteredReader);

  const auto& cmdSpec = getCommandSpec(cmd);
  if (cmdSpec.mainFileIsVRS && cmdSpec.fileDetails != Details::None) {
    RecordFileInfo::printOverview(
        cout, filteredReader.reader, filteredReader.filter.streams, cmdSpec.fileDetails);
  }

  switch (cmd) {
    case Command::Help:
      showHelp = true;
      break;
    case Command::Details:
      // Opening the VRS file already printed the file details...
      break;
    case Command::FixIndex:
      if (!openOtherVrsFiles(cmdSpec.fileDetails)) {
        statusCode = EXIT_FAILURE;
      }
      break;
    case Command::Copy:
      copyOptions.mergeStreams = false;
      statusCode = doCopyMerge();
      break;
    case Command::Merge:
      copyOptions.mergeStreams = true;
      statusCode = doCopyMerge();
      break;
    case Command::Check:
      cout << checkRecords(filteredReader, copyOptions, CheckType::Check) << "\n";
      break;
    case Command::Decode:
      cout << checkRecords(filteredReader, copyOptions, CheckType::Decode) << "\n";
      break;
    case Command::Checksum:
      cout << checkRecords(filteredReader, copyOptions, CheckType::Checksum) << "\n";
      break;
    case Command::Checksums:
      cout << checkRecords(filteredReader, copyOptions, CheckType::Checksums) << "\n";
      break;
    case Command::Hexdump:
      copyOptions.showProgress = false;
      cout << checkRecords(filteredReader, copyOptions, CheckType::HexDump) << "\n";
      break;
    case Command::ChecksumVerbatim:
      cout << verbatimChecksum(filteredReader.getPathOrUri(), copyOptions.showProgress) << "\n";
      break;
    case Command::Compare:
      for (auto& otherFile : otherFilteredReaders) {
        cout << "Comparing with ";
        if (openOtherVrsFile(otherFile, cmdSpec.fileDetails)) {
          bool areSame = compareVRSfiles(filteredReader, otherFile, copyOptions);
          cout << (areSame ? "Files are equivalent." : "Files differ.") << "\n";
        }
      }
      break;
    case Command::CompareVerbatim:
      for (auto& otherFile : otherFilteredReaders) {
        bool areSame =
            compareVerbatim(filteredReader.spec, otherFile.spec, copyOptions.showProgress);
        cout << (areSame ? "Files are identical." : "Files differ.") << "\n";
      }
      break;
    case Command::Debug: {
      cout << "VRS file internals of '" << filteredReader.getPathOrUri() << "'\n";
      FileSpec spec;
      unique_ptr<FileHandler> file;
      if (RecordFileReader::vrsFilePathToFileSpec(filteredReader.getPathOrUri(), spec) != 0 ||
          FileHandlerFactory::getInstance().delegateOpen(spec, file) != 0 ||
          !FileFormat::printVRSFileInternals(*file)) {
        statusCode = EXIT_FAILURE;
      }
    } break;
    case Command::PrintRecordFormats:
      cout << printRecordFormats(filteredReader) << "\n";
      break;
    case Command::ListRecords:
      listRecords(filteredReader);
      break;
    case Command::PrintRecords:
      printRecordFormatRecords(filteredReader, PrintoutType::Compact);
      break;
    case Command::PrintRecordsDetailed:
      printRecordFormatRecords(filteredReader, PrintoutType::Details);
      break;
    case Command::PrintRecordsJson:
      printRecordFormatRecords(filteredReader, PrintoutType::JsonCompact);
      break;
    case Command::PrintRecordsJsonPretty:
      printRecordFormatRecords(filteredReader, PrintoutType::JsonPretty);
      break;
    case Command::Rage:
      cout << "\nFirst records:\n";
      filteredReader.firstRecordsOnly = true;
      printRecordFormatRecords(filteredReader, PrintoutType::Details);
      break;
    case Command::ExtractImages:
      extractImages(targetPath.empty() ? "." : targetPath, filteredReader, extractImagesRaw);
      break;
    case Command::ExtractAudio:
      extractAudio(targetPath.empty() ? "." : targetPath, filteredReader);
      break;
    case Command::ExtractAll:
      extractAll(targetPath.empty() ? "." : targetPath, filteredReader);
      break;
    case Command::JsonDescription:
      cout << RecordFileInfo::jsonOverview(
                  filteredReader.reader,
                  filteredReader.filter.streams,
                  RecordFileInfo::Details::Everything)
           << "\n";
      break;
    case Command::CompressionBenchmark:
      statusCode = compressionBenchmark(filteredReader, copyOptions);
      break;
    case Command::None:
    case Command::COUNT:
      break;
  }

  return statusCode;
}

int VrsCommand::doCopyMerge() {
  int statusCode = SUCCESS;
  if (targetPath.empty()) {
    fmt::print(
        stderr, "Error: Need a local path to do {} operation.\n", CommandConverter::toString(cmd));
    return EXIT_FAILURE;
  }
  double timeBefore = os::getTimestampSec();
  string commandName = otherFilteredReaders.empty() ? "Copy" : "Merge";
  if (otherFilteredReaders.empty()) {
    statusCode = filterCopy(filteredReader, targetPath, copyOptions, copyMakeStreamFilterFunction);
  } else {
    // Apply the filters to the other sources after opening them
    if (!openOtherVrsFiles(kCopyOpsDetails)) {
      return EXIT_FAILURE;
    }
    vector<FilteredFileReader*> recordFilters;
    for (auto& recordFilter : otherFilteredReaders) {
      recordFilters.push_back(&recordFilter);
    }
    statusCode = filterMerge(filteredReader, recordFilters, targetPath, copyOptions);
  }
  if (statusCode != 0) {
    cerr << commandName << " failed: " << errorCodeToMessage(statusCode) << "\n";
  } else {
    double duration = os::getTimestampSec() - timeBefore;
    if (!copyOptions.outUri.empty() && copyOptions.outUri != "gaia:0") {
      cout << commandName << " successful to " << copyOptions.outUri << "\n";
    }
    cout << "Wrote " << copyOptions.outRecordCopiedCount << " records in "
         << helpers::humanReadableDuration(duration) << ".\n";
    // If this is an upload operation, the output file is removed after it is uploaded.
    // If you directly upload to remote storage, you also don't have a local file.
    if (!isRemoteFileSystem(targetPath)) {
      RecordFileReader outputFile;
      int error = outputFile.openFile(targetPath);
      if (error == 0) {
        RecordFileInfo::printOverview(cout, outputFile, kCopyOpsDetails);
        int64_t sourceSize = filteredReader.reader.getTotalSourceSize();
        for (const FilteredFileReader& otherSource : otherFilteredReaders) {
          sourceSize += otherSource.reader.getTotalSourceSize();
        }
        int64_t copySize = outputFile.getTotalSourceSize();
        int64_t change = sourceSize - copySize;
        cout << "Preset " << vrs::toPrettyName(copyOptions.getCompression()) << ": ";
        if (change == 0) {
          cout << "No file size change.\n";
        } else {
          if (change > 0) {
            cout << "Saved ";
          } else {
            cout << "Added ";
            change = -change;
          }
          cout << fmt::format(
              "{}, {:.2f}% in {}.\n",
              helpers::humanReadableFileSize(change),
              100. * change / sourceSize,
              helpers::humanReadableDuration(duration));
        }
      } else {
        cerr << "Error: could not open copied file '" << filteredReader.getPathOrUri()
             << "', error #" << error << ": " << errorCodeToMessage(error) << "\n";
        statusCode = EXIT_FAILURE;
      }
    }
  }
  return statusCode;
}

bool VrsCommand::isRemoteFileSystem(const string& path) {
  unique_ptr<FileHandler> filehandler;
  if (FileHandlerFactory::getInstance().delegateOpen(path, filehandler) != SUCCESS) {
    return false;
  }
  return filehandler->isRemoteFileSystem();
}

DecimationParams& VrsCommand::getDecimatorParams() {
  if (!filters.decimationParams) {
    filters.decimationParams = make_unique<DecimationParams>();
  }
  return *filters.decimationParams;
}

} // namespace vrscli
