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

#include "CliParsing.h"

#include <iostream>

#include <vrs/helpers/Strings.h>

using namespace std;

namespace vrs::utils {

namespace {

inline bool isSigned(const char* str) {
  return *str == '+' || *str == '-';
}

} // namespace

bool parseCopyOptions(
    string_view appName,
    string_view arg,
    int& argn,
    int argc,
    char** argv,
    int& outStatusCode,
    CopyOptions& copyOptions) {
  string_view compressionOption = "--compression=";
  if (arg == "--no-progress") {
    copyOptions.showProgress = false;
  } else if (helpers::startsWith(arg, compressionOption)) {
    string_view optionValue = arg.substr(compressionOption.size());
    if (optionValue == "default") {
      copyOptions.setCompressionPreset(CompressionPreset::Default);
    } else if (optionValue == "zsdefault") {
      copyOptions.setCompressionPreset(CompressionPreset::ZstdMedium);
    } else {
      CompressionPreset preset = toEnum<CompressionPreset>(string(optionValue));
      if (preset != CompressionPreset::Undefined) {
        copyOptions.setCompressionPreset(preset);
      } else {
        cerr << appName << ": error. Invalid --compression argument value: '" << optionValue
             << "'.\n";
        outStatusCode = EXIT_FAILURE;
      }
    }
  } else if (arg == "--chunk-size") {
    if (++argn < argc) {
      size_t factor = 1;
      string param = argv[argn];
      if (!param.empty()) {
        switch (::tolower(param.back())) {
          case 'm':
            factor = 1;
            param.pop_back();
            break;
          case 'g':
            factor = 1024;
            param.pop_back();
            break;
        }
      }
      try {
        copyOptions.maxChunkSizeMB = stoull(param) * factor;
      } catch (logic_error&) {
        cerr << appName << ": error. Invalid '--chunk-size' numeric value.\n";
        outStatusCode = EXIT_FAILURE;
      }
    } else {
      cerr << appName << ": '--chunk-size' <nb>[M|G]\n";
      outStatusCode = EXIT_FAILURE;
    }
  } else if (arg == "--mt") {
    if (++argn < argc) {
      try {
        copyOptions.compressionPoolSize = static_cast<unsigned>(stoul(argv[argn]));
      } catch (logic_error&) {
        cerr << appName << ": error. Invalid '--mt' numeric value.\n";
        outStatusCode = EXIT_FAILURE;
      }
    } else {
      cerr << appName << ": '--mt' requires a number of threads.\n";
      outStatusCode = EXIT_FAILURE;
    }
  } else {
    return false;
  }
  return true;
}

void printCopyOptionsHelp() {
  cout << "  [ --no-progress ]:"
          " don't show any progress information (useful for offline usage with basic terminals).\n"
          "  [ --mt <thread-count> ]: use <thread-count> threads for compression while copying.\n"
          "  [ --chunk-size <nb>[M|G] ]: chunk output file every <nb> number of MiB or GiB.\n"
          "    Use 'M' for MiB (default), or 'G' for GiB.\n"
          "  [ --compression={none|default|fast|tight|zfast|zlight|zmedium|ztight|zmax} ]:"
          " set compression setting.\n";
}

bool parseTagOverrideOptions(
    string_view appName,
    string_view arg,
    int& argn,
    int argc,
    char** argv,
    int& outStatusCode,
    CopyOptions& copyOptions) {
  if (arg == "--file-tag") {
    if (argn + 2 < argc) {
      const string tagName = string(argv[++argn]);
      const string tagValue = string(argv[++argn]);
      if (tagName.empty()) {
        cerr << appName << ": error. '--file-tag' requires a real tag name.\n";
        outStatusCode = EXIT_FAILURE;
      } else {
        copyOptions.getTagOverrider().fileTags[tagName] = tagValue;
      }
    } else {
      cerr << appName << ": error. '--file-tag' requires a tag name & a tag value.\n";
      outStatusCode = EXIT_FAILURE;
    }
  } else if (arg == "--stream-tag") {
    if (argn + 3 < argc) {
      const string streamId = string(argv[++argn]);
      StreamId id = StreamId::fromNumericName(streamId);
      if (!id.isValid()) {
        cerr << appName << ": error. '--stream-tag' invalid stream id '" << streamId << "'.\n";
        outStatusCode = EXIT_FAILURE;
      } else {
        const string tagName = string(argv[++argn]);
        const string tagValue = string(argv[++argn]);
        if (tagName.empty()) {
          cerr << appName << ": error. '--stream-tag' requires a real tag name.\n";
          outStatusCode = EXIT_FAILURE;
        } else {
          copyOptions.getTagOverrider().streamTags[id][tagName] = tagValue;
        }
      }
    } else {
      cerr << appName
           << ": error. '--stream-tag' requires a stream id, a tag name & a tag value.\n";
      outStatusCode = EXIT_FAILURE;
    }
  } else {
    return false;
  }
  return true;
}

void printTagOverrideOptionsHelp() {
  cout << "  [ --file-tag <tag_name> <tag_value> ]:"
          " set a file tag in the copied/merged file.\n"
          "  [ --stream-tag <recordable_type_id> <tag_name> <tag_value> ]:"
          " set a tag of a particular stream in the copied/merged file.\n";
}

bool parseTimeAndStreamFilters(
    string_view appName,
    string_view arg,
    int& argn,
    int argc,
    char** argv,
    int& outStatusCode,
    FilteredFileReader& filteredReader,
    RecordFilterParams& outFilters) {
  if (arg == "--after") {
    if (++argn < argc) {
      if (!filteredReader.afterConstraint(argv[argn])) {
        cerr << appName << ": error. Invalid '--after' numeric value.\n";
        outStatusCode = EXIT_FAILURE;
      }
    } else {
      cerr << appName << ": error. '--after' requires a numeric parameter.\n";
      outStatusCode = EXIT_FAILURE;
    }
  } else if (arg == "--before") {
    if (++argn < argc) {
      if (!filteredReader.beforeConstraint(argv[argn])) {
        cerr << appName << ": error. Invalid '--before' numeric value.\n";
        outStatusCode = EXIT_FAILURE;
      }
    } else {
      cerr << appName << ": error. '--before' requires a numeric parameter.\n";
      outStatusCode = EXIT_FAILURE;
    }
  } else if (arg == "--range") {
    if (argn + 2 < argc) {
      string after = argv[++argn];
      string before = argv[++argn];
      if (!filteredReader.afterConstraint(after) || !filteredReader.beforeConstraint(before)) {
        cerr << appName << ": error. Invalid '--range' numeric value(s).\n";
        outStatusCode = EXIT_FAILURE;
      }
    } else {
      cerr << appName << ": error. '--range' requires two numeric parameters.\n";
      outStatusCode = EXIT_FAILURE;
    }
  } else if (arg == "--around") {
    if (argn + 2 < argc) {
      try {
        filteredReader.filter.minTime = stod(argv[++argn]);
        filteredReader.filter.relativeMinTime = isSigned(argv[argn]);
        filteredReader.filter.maxTime = stod(argv[++argn]);
        filteredReader.filter.aroundTime = true;
      } catch (logic_error&) {
        cerr << appName << ": error. Invalid '--around' numeric value(s).\n";
        outStatusCode = EXIT_FAILURE;
      }
    } else {
      cerr << appName << ": error. '--range' requires two numeric parameters.\n";
      outStatusCode = EXIT_FAILURE;
    }
  } else if (arg == "-" || arg == "+") {
    if (++argn < argc) {
      bool valid = false;
      bool exc = (arg == "-");
      if (isdigit(static_cast<uint8_t>(argv[argn][0])) != 0) {
        valid = exc ? outFilters.excludeStream(argv[argn]) : outFilters.includeStream(argv[argn]);
      } else {
        valid = exc ? outFilters.excludeType(argv[argn]) : outFilters.includeType(argv[argn]);
      }
      if (!valid) {
        cerr << appName << ": error. Invalid '" << arg << "' argument.\n";
        outStatusCode = EXIT_FAILURE;
      }
    } else {
      cerr << appName << ": error. '" << arg << "' option requires a second argument.\n";
      outStatusCode = EXIT_FAILURE;
    }
  } else if (arg == "-1" || arg == "--first-records") {
    filteredReader.firstRecordsOnly = true;
  } else {
    return false;
  }
  return true;
}

void printTimeAndStreamFiltersHelp() {
  cout
      << "\n Timestamp Filtering Options:\n"
         "  - Time values starting with a digit are considered absolute timestamp values.\n"
         "  - Time values starting with a '+' sign represent offsets relative to the first data record timestamp.\n"
         "  - Time values starting with a '-' sign represent offsets relative to the last data record timestamp.\n"
         "  - All time values, timestamps, and offsets are expressed in seconds as floating point numbers.\n"
         "  - All min-time limits are exclusive, while max-time limits are inclusive, e.g., (min-time, max-time].\n"
         "  - When filtering by timestamp with a min-time limit, last configuration and state records are preserved.\n"
         "  [ --after [+|-]<min-time> ]: include records with timestamps greater than [+|-]<min-time>.\n"
         "  [ --before [+|-]<max-time> ]: include records with timestamps less than or equal to [+|-]<max-time>.\n"
         "  [ --range [+|-]<min-time> [+|-]<max-time> ]:"
         " include records with timestamps within the range ([+|-]min-time, [+|-]max-time].\n"
         "  [ --around [+|-]<time> <range> ]:"
         " include records with timestamps within the range ([+|-]time-range, [+|-]time+range].\n\n"
         " Stream Filtering Options:\n"
         "  [ + <recordable_type_id> ]: include streams of the specified recordable type ID.\n"
         "  [ + <recordable_type_id>-<instance_id> ]: include a specific stream ID.\n"
         "  [ - <recordable_type_id> ]: exclude all streams of the specified recordable type ID.\n"
         "  [ - <recordable_type_id>-<instance_id> ]: exclude a specific stream.\n\n"
         " Record Type Filtering Options:\n"
         "  [ + [configuration|state|data] ]: include records of the specified type.\n"
         "  [ - [configuration|state|data] ]: exclude records of the specified type.\n\n"
         " Other Record Filtering Options:\n"
         "  [ -1 | --first-records ]: only include the first record of each stream and type.\n";
}

namespace {
DecimationParams& getDecimatorParams(RecordFilterParams& filters) {
  if (!filters.decimationParams) {
    filters.decimationParams = make_unique<DecimationParams>();
  }
  return *filters.decimationParams;
}
} // namespace

bool parseDecimationOptions(
    string_view appName,
    string_view arg,
    int& argn,
    int argc,
    char** argv,
    int& outStatusCode,
    RecordFilterParams& outFilters) {
  if (arg == "--decimate") {
    if (argn + 2 < argc) {
      try {
        const string streamId = string(argv[++argn]);
        const double interval = stod(argv[++argn]);
        getDecimatorParams(outFilters).decimationIntervals.emplace_back(streamId, interval);
      } catch (logic_error&) {
        cerr << appName << ": error. Invalid --decimate numeric value.\n";
        outStatusCode = EXIT_FAILURE;
      }
    } else {
      cerr << appName << ": error. '--decimate' requires a stream id and a numeric parameter.\n";
      outStatusCode = EXIT_FAILURE;
    }
  } else if (arg == "--bucket-interval") {
    if (++argn < argc) {
      try {
        getDecimatorParams(outFilters).bucketInterval = stod(argv[argn]);
      } catch (logic_error&) {
        cerr << appName << ": error. Invalid '--bucket-interval' numeric value.\n";
        outStatusCode = EXIT_FAILURE;
      }
    } else {
      cerr << appName << ": error. '--bucket-interval' requires a numeric parameter.\n";
      outStatusCode = EXIT_FAILURE;
    }
  } else if (arg == "--bucket-max-delta") {
    if (++argn < argc) {
      try {
        getDecimatorParams(outFilters).bucketMaxTimestampDelta = stod(argv[argn]);
      } catch (logic_error&) {
        cerr << appName << ": error. Invalid '--bucket-max-delta' numeric value.\n";
        outStatusCode = EXIT_FAILURE;
      }
    } else {
      cerr << appName << ": error. '--bucket-max-delta' requires a numeric parameter.\n";
      outStatusCode = EXIT_FAILURE;
    }
  } else {
    return false;
  }
  return true;
}

void printDecimationOptionsHelp() {
  cout << "  [ --decimate <recordable_type_id>[-<instance_id>] <timestamp_interval> ]:"
          " output at most one data record\n"
          "    every <timestamp_interval> for the stream(s) specified.\n"
          "  [ --bucket-interval <timestamp_interval> ]\n"
          "  [ --bucket-max-delta <timestamp_delta> ]:"
          " bucket frames with close timestamps into buckets.\n"
          "    Only output one frame per stream per bucket."
          " If frame timestamps are more than max-delta away, skip them.\n";
}

} // namespace vrs::utils
