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

#pragma once

#include <string>

#include <vrs/utils/FilteredFileReader.h>

namespace vrs::utils {

using std::string;

/// Parse copy options for copy/merge/filter operations.
/// --no-progress, --compression=<value>, --chunk-size <size>, --mt <n>
/// @param appName: Name of the application binary for error messages
/// @param arg: string to check.
/// @param argn: current argument index number, updated if more arguments were consumed.
/// @param argc: Max argument count.
/// @param argv: All the command line arguments available.
/// @param outStatusCode: Set if arg was recognied but an error occurred, untouched otherwise.
/// @param copyOptions: where to store the arguments parsed.
/// @return False if arg was not recognized (argn & outStatusCode were not changed).
/// Returns true if the argument was recognized, which doesn't mean there was no error.
/// If an argument error of any kind is found (invalid type, path to a file that's missing, etc),
/// then outStatusCode is set to a non-zero value, and parsing & execution should be aborted.
/// On the other hand, if the argument and possible complement arguments were valid,
/// outStatusCode is unchanged (presumably, it's still set to EXIT_SUCCESS),
/// and the next argument parameter to parse is at argn + 1.
bool parseCopyOptions(
    const string& appName,
    const string& arg,
    int& argn,
    int argc,
    char** argv,
    int& outStatusCode,
    CopyOptions& copyOptions);

/// Parse tag override options for copy/merge/filter operations.
/// --file-tag <tag-name> <tag-value>, --stream-tag <tag-name> <tag-value>
/// @param appName: Name of the application binary for error messages
/// @param arg: string to check.
/// @param argn: current argument index number, updated if more arguments were consumed.
/// @param argc: Max argument count.
/// @param argv: All the command line arguments available.
/// @param outStatusCode: Set if arg was recognied but an error occurred, untouched otherwise.
/// @param copyOptions: where to store the arguments parsed.
/// @return False if arg was not recognized (argn & outStatusCode were not changed).
/// Returns true if the argument was recognized, which doesn't mean there was no error.
/// If an argument error of any kind is found (invalid type, path to a file that's missing, etc),
/// then outStatusCode is set to a non-zero value, and parsing & execution should be aborted.
/// On the other hand, if the argument and possible complement arguments were valid,
/// outStatusCode is unchanged (presumably, it's still set to EXIT_SUCCESS),
/// and the next argument parameter to parse is at argn + 1.
bool parseTagOverrideOptions(
    const string& appName,
    const string& arg,
    int& argn,
    int argc,
    char** argv,
    int& outStatusCode,
    CopyOptions& copyOptions);

/// Parse time constraints for record iteration operations, including copy/merge/filter.
/// --after <t>, --before <t>, --range <start> <end>, --around <center> <radius>, --first-records,
/// + ( <RecordableTypeId> | <StreamId> | <Record-Type> ),
/// - ( <RecordableTypeId> | <StreamId> | <Record-Type> )
/// @param appName: Name of the application binary for error messages
/// @param arg: string to check.
/// @param argn: current argument index number, updated if more arguments were consumed.
/// @param argc: Max argument count.
/// @param argv: All the command line arguments available.
/// @param outStatusCode: Set if arg was recognied but an error occurred, untouched otherwise.
/// @param copyOptions: where to store the arguments parsed.
/// @return False if arg was not recognized (argn & outStatusCode were not changed).
/// Returns true if the argument was recognized, which doesn't mean there was no error.
/// If an argument error of any kind is found (invalid type, path to a file that's missing, etc),
/// then outStatusCode is set to a non-zero value, and parsing & execution should be aborted.
/// On the other hand, if the argument and possible complement arguments were valid,
/// outStatusCode is unchanged (presumably, it's still set to EXIT_SUCCESS),
/// and the next argument parameter to parse is at argn + 1.
bool parseTimeAndStreamFilters(
    const string& appName,
    const string& arg,
    int& argn,
    int argc,
    char** argv,
    int& outStatusCode,
    FilteredFileReader& filteredReader,
    RecordFilterParams& outFilters);

/// Parse decimation options for record iteration operations, including copy/merge/filter.
/// --decimate <streamId> <interval>, --bucket-interval <interval>, --bucket-max-delta <max-delta>
/// @param appName: Name of the application binary for error messages
/// @param arg: string to check.
/// @param argn: current argument index number, updated if more arguments were consumed.
/// @param argc: Max argument count.
/// @param argv: All the command line arguments available.
/// @param outStatusCode: Set if arg was recognied but an error occurred, untouched otherwise.
/// @param copyOptions: where to store the arguments parsed.
/// @return False if arg was not recognized (argn & outStatusCode were not changed).
/// Returns true if the argument was recognized, which doesn't mean there was no error.
/// If an argument error of any kind is found (invalid type, path to a file that's missing, etc),
/// then outStatusCode is set to a non-zero value, and parsing & execution should be aborted.
/// On the other hand, if the argument and possible complement arguments were valid,
/// outStatusCode is unchanged (presumably, it's still set to EXIT_SUCCESS),
/// and the next argument parameter to parse is at argn + 1.
bool parseDecimationOptions(
    const string& appName,
    const string& arg,
    int& argn,
    int argc,
    char** argv,
    int& outStatusCode,
    RecordFilterParams& outFilters);

} // namespace vrs::utils
