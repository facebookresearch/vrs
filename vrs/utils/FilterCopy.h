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

#include <vrs/utils/FilteredFileReader.h>

namespace vrs::utils {

void printProgress(const char* status, size_t currentSize, size_t totalSize, bool showProgress);

/// Type of function that returns a new stream player suitable to copy or filter a particular stream
/// during a copy operation.
/// This StreamPlayer is responsible for:
///  - copying the stream's tags,
///  - hooking up itself up to the reader,
///  - creating & hooking-up a Recordable for the writer, so it can create records to write out,
///  - seting up the output stream's compression
///  - when it receives a record from its StreamPlayer interface, creating a corresponding record in
///  the
///    output file.
/// See Copier for the model version that simply copies a stream unmodified.
/// If you need to edit/modify records, that's your chance to hook-up your dark magic.
/// @param fileReader: the open VRS file from which records are read.
/// @param fileWriter: the VRS file to which records should be written, probably by using a
/// Recordable that it will attach to that fileWriter.
/// @param streamId: id of the stream this stream player is responsible for copying or filtering.
/// @param copyOptions: the copy options used for this copy operation.
/// @return A pointer to a stream player that is now attached to the fileReader, and will create
/// records in the fileWriter, as appropriate.
using MakeStreamFilterFunction = function<unique_ptr<StreamPlayer>(
    RecordFileReader& fileReader,
    RecordFileWriter& fileWriter,
    StreamId streamId,
    const CopyOptions& copyOptions)>;

/// Default MakeStreamFilterFunction to be used by copyRecords() to simply copy a whole stream.
unique_ptr<StreamPlayer> makeCopier(
    RecordFileReader& fileReader,
    RecordFileWriter& fileWriter,
    StreamId streamId,
    const CopyOptions& copyOptions);

/// Copy records from one file to another, using a filtered reader
/// but otherwise making no changes to records?
/// @param filteredReader: source file with record selection filtering
/// @param pathToCopy: path of the destination location
/// @param copyOptions: copy parameters, such as compression preset, chunking behavior, etc.
/// @param makeStreamFilter: optional helper that can decide which stream to copy or filter.
/// @param throttledFileDelegate: delegate to create & close the target file.
/// This might be useful when doing more advanced operations, such as uploads.
/// @return a status code, with 0 meaning success, and any non-zero value is a failure, in which
/// case an error message will be printed in cerr.
int filterCopy(
    FilteredFileReader& filteredReader,
    const string& pathToCopy,
    const CopyOptions& copyOptions,
    MakeStreamFilterFunction makeStreamFilter = makeCopier,
    unique_ptr<ThrottledFileDelegate> throttledFileDelegate =
        std::make_unique<ThrottledFileDelegate>());

/// Merge records from multiple files into a new file, using multiple filtered readers.
/// @param filteredReader: first source file with record filtering.
/// @param moreRecordFilters: more source files with filterering
/// @param pathToCopy: path of the destination location
/// @param copyOptions: copy parameters, such as compression preset, chunking behavior, etc.
/// @param throttledFileDelegate: delegate to create & close the target file.
/// This might be useful when doing more advanced operations, such as uploads.
/// @return a status code, with 0 meaning success, and any non-zero value is a failure, in which
/// case an error message will be printed in cerr.
int filterMerge(
    FilteredFileReader& recordFilter,
    const vector<FilteredFileReader*>& moreRecordFilters,
    const string& pathToCopy,
    const CopyOptions& copyOptions,
    unique_ptr<ThrottledFileDelegate> throttledFileDelegate =
        std::make_unique<ThrottledFileDelegate>());

} // namespace vrs::utils
