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

string_view sCommands[] = {
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
    "json-pretty-description",
    "fix-index",
    "compression-benchmark",
};
ENUM_STRING_CONVERTER(Command, sCommands, Command::None);

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
      {Command::JsonPrettyDescription, 1},
      {Command::FixIndex, 1000, Details::Basics},
      {Command::CompressionBenchmark, 1},
  };
  if (!XR_VERIFY(enumIsValid(cmd))) {
    return sCommandSpecs[static_cast<size_t>(Command::None)];
  }
  return sCommandSpecs[static_cast<size_t>(cmd)];
}

} // namespace

#define CMD(H, C) H ":\n  " << appName << " " C "\n"

namespace vrscli {

void printHelp(const string& appName) {
  cout
      << CMD("Get details about a VRS file", "[ file.vrs ] [filter-options]")

      << "\n"
      << CMD("Command format", "<command> [ arguments ]*")

      << "\n"
      << CMD("Show this documentation", "help")

      << "\n"

      << CMD("Export file description as single-line JSON", "json-description <file.vrs>")
      << CMD("Export file description as indented JSON", "json-pretty-description <file.vrs>")
      << CMD("List records with timestamp, stream name and type",
             "list <file.vrs> [filter-options]")
      << CMD("Show RecordFormat and DataLayout definitions", "record-formats <file.vrs>")
      << CMD("Print records using RecordFormat & DataLayout", "print <file.vrs> [filter-options]")
      << CMD("Print detailed records with RecordFormat & DataLayout",
             "print-details <file.vrs> [filter-options]")
      << CMD("Export records as JSON", "print-json <file.vrs> [filter-options]")
      << CMD("Export records as indented JSON", "print-json-pretty <file.vrs> [filter-options]")
      << CMD("Print file info and first records for diagnostics", "rage <file.vrs>")

      << "\n"
      << CMD("Copy streams from one or more files into one",
             "copy [ vrsfiles.vrs ]+ --to <target.vrs> [copy-options] [tag-options] [filter-options]")
      << CMD("Merge streams from multiple files into one",
             "merge [ vrsfiles.vrs ]+ --to <target.vrs> [copy-options] [tag-options] [filter-options]")
      << CMD("Create compressed copy with zeroed image and audio data",
             "copy --zero-vrs <file.vrs> --to <output.vrs>")

      << "\n"
      << CMD("Extract images to folder (jpg/png preserved, RAW converted to PNG or kept as raw with --raw-images)",
             "extract-images file.vrs [ --to <folder_path> ] [ --raw-images ] [filter-options]")
      << CMD("Extract audio as WAVE files",
             "extract-audio file.vrs [ --to <folder_path> ] [filter-options]")
      << CMD("Extract images, audio, and metadata",
             "extract-all file.vrs [ --to <folder_path> ] [filter-options]")

      << "\n"
      << CMD("Verify file integrity", "check <file.vrs> [filter-options]")
      << CMD("Verify record formats and image compression", "decode <file.vrs> [filter-options]")
      << CMD("Calculate file checksum at VRS data level", "checksum <file.vrs> [filter-options]")
      << CMD("Calculate checksums for each file section", "checksums <file.vrs> [filter-options]")
      << CMD("Calculate raw file checksum", "checksum-verbatim <file.vrs> [filter-options]")
      << CMD("Show hex dump with checksums", "hexdump <file.vrs> [filter-options]")
      << CMD("Compare files at VRS data level",
             "compare <original.vrs> [others.vrs]+ [filter-options]")
      << CMD("Compare files at raw binary level", "compare-verbatim <original.vrs> <other.vrs>")

      << "\n"
      << CMD("Run compression benchmarks", "compression-benchmark <file.vrs>")

      << "\n"
      << "Special Commands:\n"
      << CMD("Fix VRS index in place (WARNING: MODIFIES FILES IF NEEDED)",
             "fix-index <file.vrs> [<file2.vrs>]+")
      << CMD("Show VRS file format internals", "debug <file.vrs>")

      << "\n"
      << "Record Filtering Options:\n";
  printTimeAndStreamFiltersHelp();
  printDecimationOptionsHelp();

  cout << "\n"
          "Copy Options:\n";
  printCopyOptionsHelp();

  cout << "\n"
          "Tag Override Options:\n";
  printTagOverrideOptionsHelp();
}

#define SP(x) "  " << appName << " " x "\n"

void printSamples(const string& appName) {
  cout << "\n"
       << "Examples:\n"
       << "Get file summary:\n"
       << SP("src.vrs")

       << "Get file description as JSON:\n"
       << SP("json-description file.vrs")

       << "Get file description as indented JSON:\n"
       << SP("json-pretty-description file.vrs")

       << "List records with basic details:\n"
       << SP("list src.vrs")

       << "Get configuration records as JSON:\n"
       << SP("print-json src.vrs + configuration")

       << "Get device 1001's config & state records as JSON:\n"
       << SP("print-json src.vrs + 1001 - data")

       << "Clean up recording with default compression and rebuilt index:\n"
       << SP("copy src.vrs --to cleanedRecording.vrs")

       << "Recompress with tighter zstd compression:\n"
       << SP("copy src.vrs --to tight.vrs --compression=ztight")

       << "Remove all ImageStream streams:\n"
       << SP("src.vrs # identify that '100' is ImageStream")
       << SP("copy src.vrs --to noImageStream.vrs - 100")

       << "Extract specific streams:\n"
       << SP("src.vrs # identify stream IDs (e.g., 100-1 and 101-1)")
       << SP("copy src.vrs --to extract.vrs + 100-1 + 101-1")

       << "Trim data records in first 2s and last 1s:\n"
       << SP("copy src.vrs --to extract.vrs --after +2 --before -1")

       << "Combine multiple files (keeping streams separate):\n"
       << SP("copy first.vrs second.vrs third.vrs --to new.vrs")

       << "Merge multiple files (combining streams by type):\n"
       << SP("merge first.vrs second.vrs third.vrs --to new.vrs")

       << "Extract all images to folder:\n"
       << SP("extract-images file.vrs --to imageFolder")

       << "Extract ImageStream images from first 5 seconds:\n"
       << SP("src.vrs # identify that '100' is ImageStream")
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
        parseDecimationOptions(appName, arg, argn, argc, argv, outStatusCode, decimationParams);
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
    otherReader.filter.copyTimeConstraints(filteredReader.filter);
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
  if (decimationParams) {
    decimationParams->decimate(reader);
  }
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
          !(file = FileHandler::makeOpen(spec)) || !FileFormat::printVRSFileInternals(file)) {
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
    case Command::JsonPrettyDescription:
      cout << RecordFileInfo::jsonOverview(
                  filteredReader.reader,
                  filteredReader.filter.streams,
                  RecordFileInfo::Details::Everything + RecordFileInfo::Details::JsonPretty)
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
    statusCode = filterMerge(filteredReader, otherFilteredReaders, targetPath, copyOptions);
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
  unique_ptr<FileHandler> filehandler = FileHandler::makeOpen(path);
  return filehandler && filehandler->isRemoteFileSystem();
}

DefaultDecimator::Params& VrsCommand::getDecimatorParams() {
  if (!decimationParams) {
    decimationParams = make_unique<DefaultDecimator::Params>();
  }
  return *decimationParams;
}

} // namespace vrscli
