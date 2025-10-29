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

#include <vrs/utils/cli/CliParsing.h>

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
  cout
      << "  [ --no-progress ]: Hide progress information (useful when scripting using basic terminals).\n"
         "  [ --mt <thread-count> ]: Use specified number of threads for compression.\n"
         "  [ --chunk-size <nb>[M|G] ]: Split output file into chunks of <nb> MiB (default) or GiB.\n"
         "  [ --compression=<level> ]: Set lz4 or zstd record compression level.\n"
         "    levels: none, default, fast, tight, zfast, zlight, zmedium, ztight, zmax.\n"
         "  [ --late-index ]: Write index at end of file (smaller file, but worsse for streaming).\n";
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
  cout << "  [ --file-tag <tag_name> <tag_value> ]: Set file-level tag.\n"
          "  [ --stream-tag <recordable_type_id> <tag_name> <tag_value> ]: Set stream-level tag.\n";
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
      << "\n Timestamp Filtering:\n"
         "  - Timestamps: Values starting with a digit are absolute timestamps.\n"
         "  - '+' prefix: Offset from first data record timestamp.\n"
         "  - '-' prefix: Offset from last data record timestamp.\n"
         "  - All times are in seconds as floating point numbers.\n"
         "  - Range notation: (min-time, max-time] (min exclusive, max inclusive).\n"
         "  - Min-time filters preserve last configuration and state records before.\n"
         "  [ --after [+|-]<min-time> ]: Include records after specified time.\n"
         "  [ --before [+|-]<max-time> ]: Include records before or at specified time.\n"
         "  [ --range [+|-]<min-time> [+|-]<max-time> ]: Include records within specified time range.\n"
         "  [ --around [+|-]<time> <range> ]: Include records within time±range window.\n\n"
         " Stream Filtering:\n"
         "  [ + <recordable_type_id> ]: Include all streams of specified type.\n"
         "  [ + <recordable_type_id>-<instance_id> ]: Include specific stream.\n"
         "  [ - <recordable_type_id> ]: Exclude all streams of specified type.\n"
         "  [ - <recordable_type_id>-<instance_id> ]: Exclude specific stream.\n\n"
         " Record Type Filtering:\n"
         "  [ + [configuration|state|data] ]: Include specified record type.\n"
         "  [ - [configuration|state|data] ]: Exclude specified record type.\n\n"
         " Other Record Filtering:\n"
         "  [ -1 | --first-records ]: Include only first record of each stream and type.\n";
}

namespace {
DefaultDecimator::Params& getDecimatorParams(
    unique_ptr<DefaultDecimator::Params>& decimationParams) {
  if (!decimationParams) {
    decimationParams = make_unique<DefaultDecimator::Params>();
  }
  return *decimationParams;
}
} // namespace

bool parseDecimationOptions(
    string_view appName,
    string_view arg,
    int& argn,
    int argc,
    char** argv,
    int& outStatusCode,
    unique_ptr<DefaultDecimator::Params>& decimationParams) {
  if (arg == "--decimate") {
    if (argn + 2 < argc) {
      try {
        const string streamId = string(argv[++argn]);
        const double interval = stod(argv[++argn]);
        getDecimatorParams(decimationParams).decimationIntervals.emplace_back(streamId, interval);
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
        getDecimatorParams(decimationParams).bucketInterval = stod(argv[argn]);
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
        getDecimatorParams(decimationParams).bucketMaxTimestampDelta = stod(argv[argn]);
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
  cout
      << "  [ --decimate <recordable_type_id>[-<instance_id>] <timestamp_interval> ]: Output at most\n"
         "    one data record per <timestamp_interval> seconds for specified stream(s).\n"
         "  [ --bucket-interval <timestamp_interval> ]\n"
         "  [ --bucket-max-delta <timestamp_delta> ]: Group records with similar timestamps into buckets.\n"
         "    Outputs one records per stream per bucket. Skips records with timestamps exceeding max-delta.\n";
}

} // namespace vrs::utils
