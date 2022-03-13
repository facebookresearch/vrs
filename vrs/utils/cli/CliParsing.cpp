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

#include "CliParsing.h"

#include <sstream>

#include <vrs/helpers/Strings.h>

using namespace std;

namespace vrs::utils {

namespace {

inline bool isSigned(const char* str) {
  return *str == '+' || *str == '-';
}

StreamId parseStreamId(const string& id) {
  StreamId result;
  stringstream ss(id);
  int32_t typeIdNum;
  if (ss >> typeIdNum) {
    RecordableTypeId typeId = static_cast<RecordableTypeId>(typeIdNum);
    if (ss.peek() == '-') {
      ss.ignore();
      uint16_t instanceId;
      if (ss >> instanceId) {
        result = {typeId, instanceId};
      }
    }
  }
  return result;
}

} // namespace

bool parseCopyOptions(
    const string& appName,
    const string& arg,
    int& argn,
    int argc,
    char** argv,
    int& outStatusCode,
    CopyOptions& copyOptions) {
  if (arg == "--no-progress") {
    copyOptions.showProgress = false;
  } else if (arg == "--compression=default") {
    copyOptions.setCompressionPreset(CompressionPreset::Default);
  } else if (arg == "--compression=zdefault") {
    copyOptions.setCompressionPreset(CompressionPreset::ZstdMedium);
  } else if (helpers::startsWith(arg, "--compression=")) {
    const char* optionValue = arg.c_str() + strlen("--compression=");
    CompressionPreset preset = toEnum<CompressionPreset>(optionValue);
    if (preset != CompressionPreset::Undefined) {
      copyOptions.setCompressionPreset(preset);
    } else {
      cerr << appName << ": error. Invalid --compression argument value: '" << optionValue << "'."
           << endl;
      outStatusCode = EXIT_FAILURE;
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
        cerr << appName << ": error. Invalid '--chunk-size' numberic value." << endl;
        outStatusCode = EXIT_FAILURE;
      }
    } else {
      cerr << appName << ": '--chunk-size' <nb>[M|G]" << endl;
      outStatusCode = EXIT_FAILURE;
    }
  } else if (arg == "--mt") {
    if (++argn < argc) {
      try {
        copyOptions.compressionPoolSize = static_cast<unsigned>(stoul(argv[argn]));
      } catch (logic_error&) {
        cerr << appName << ": error. Invalid '--mt' numeric value." << endl;
        outStatusCode = EXIT_FAILURE;
      }
    } else {
      cerr << appName << ": '--mt' requires a number of threads." << endl;
      outStatusCode = EXIT_FAILURE;
    }
  } else {
    return false;
  }
  return true;
}

bool parseTagOverrideOptions(
    const string& appName,
    const string& arg,
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
        cerr << appName << ": error. '--file-tag' requires a real tag name." << endl;
        outStatusCode = EXIT_FAILURE;
      } else {
        copyOptions.tagOverrides.fileTags[tagName] = tagValue;
      }
    } else {
      cerr << appName << ": error. '--file-tag' requires a tag name & a tag value." << endl;
      outStatusCode = EXIT_FAILURE;
    }
  } else if (arg == "--stream-tag") {
    if (argn + 3 < argc) {
      const string streamId = string(argv[++argn]);
      StreamId id = parseStreamId(streamId);
      if (!id.isValid()) {
        cerr << appName << ": error. '--stream-tag' invalid stream id '" << streamId << "'."
             << endl;
        outStatusCode = EXIT_FAILURE;
      } else {
        const string tagName = string(argv[++argn]);
        const string tagValue = string(argv[++argn]);
        if (tagName.empty()) {
          cerr << appName << ": error. '--stream-tag' requires a real tag name." << endl;
          outStatusCode = EXIT_FAILURE;
        } else {
          copyOptions.tagOverrides.streamTags[id][tagName] = tagValue;
        }
      }
    } else {
      cerr << appName << ": error. '--stream-tag' requires a stream id, a tag name & a tag value."
           << endl;
      outStatusCode = EXIT_FAILURE;
    }
  } else {
    return false;
  }
  return true;
}

bool parseTimeAndStreamFilters(
    const string& appName,
    const string& arg,
    int& argn,
    int argc,
    char** argv,
    int& outStatusCode,
    FilteredFileReader& filteredReader,
    RecordFilterParams& outFilters) {
  if (arg == "--after") {
    if (++argn < argc) {
      if (!filteredReader.afterConstraint(argv[argn])) {
        cerr << appName << ": error. Invalid '--after' numeric value." << endl;
        outStatusCode = EXIT_FAILURE;
      }
    } else {
      cerr << appName << ": error. '--after' requires a numeric parameter." << endl;
      outStatusCode = EXIT_FAILURE;
    }
  } else if (arg == "--before") {
    if (++argn < argc) {
      if (!filteredReader.beforeConstraint(argv[argn])) {
        cerr << appName << ": error. Invalid '--before' numeric value." << endl;
        outStatusCode = EXIT_FAILURE;
      }
    } else {
      cerr << appName << ": error. '--before' requires a numeric parameter." << endl;
      outStatusCode = EXIT_FAILURE;
    }
  } else if (arg == "--range") {
    if (argn + 2 < argc) {
      string after = argv[++argn];
      string before = argv[++argn];
      if (!filteredReader.afterConstraint(after) || !filteredReader.beforeConstraint(before)) {
        cerr << appName << ": error. Invalid '--range' numeric value(s)." << endl;
        outStatusCode = EXIT_FAILURE;
      }
    } else {
      cerr << appName << ": error. '--range' requires two numeric parameters." << endl;
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
        cerr << appName << ": error. Invalid '--around' numeric value(s)." << endl;
        outStatusCode = EXIT_FAILURE;
      }
    } else {
      cerr << appName << ": error. '--range' requires two numeric parameters." << endl;
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
        cerr << appName << ": error. Invalid '" << arg << "' argument." << endl;
        outStatusCode = EXIT_FAILURE;
      }
    } else {
      cerr << appName << ": error. '" << arg << "' option requires a second argument." << endl;
      outStatusCode = EXIT_FAILURE;
    }
  } else if (arg == "--first-records") {
    filteredReader.firstRecordsOnly = true;
  } else {
    return false;
  }
  return true;
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
    const string& appName,
    const string& arg,
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
        getDecimatorParams(outFilters).decimationIntervals.push_back({streamId, interval});
      } catch (logic_error&) {
        cerr << appName << ": error. Invalid --decimate numeric value." << endl;
        outStatusCode = EXIT_FAILURE;
      }
    } else {
      cerr << appName << ": error. '--decimate' requires a stream id and a numeric parameter."
           << endl;
      outStatusCode = EXIT_FAILURE;
    }
  } else if (arg == "--bucket-interval") {
    if (++argn < argc) {
      try {
        getDecimatorParams(outFilters).bucketInterval = stod(argv[argn]);
      } catch (logic_error&) {
        cerr << appName << ": error. Invalid '--bucket-interval' numeric value." << endl;
        outStatusCode = EXIT_FAILURE;
      }
    } else {
      cerr << appName << ": error. '--bucket-interval' requires a numeric parameter." << endl;
      outStatusCode = EXIT_FAILURE;
    }
  } else if (arg == "--bucket-max-delta") {
    if (++argn < argc) {
      try {
        getDecimatorParams(outFilters).bucketMaxTimestampDelta = stod(argv[argn]);
      } catch (logic_error&) {
        cerr << appName << ": error. Invalid '--bucket-max-delta' numeric value." << endl;
        outStatusCode = EXIT_FAILURE;
      }
    } else {
      cerr << appName << ": error. '--bucket-max-delta' requires a numeric parameter." << endl;
      outStatusCode = EXIT_FAILURE;
    }
  } else {
    return false;
  }
  return true;
}

} // namespace vrs::utils
