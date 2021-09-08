// Facebook Technologies, LLC Proprietary and Confidential.

#pragma once

#include <array>
#include <list>

#include <vrs/Compressor.h>

#include "CopyHelpers.h"
#include "FilteredVRSFileReader.h"

namespace vrs::utils {

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
/// See vrs::Copier for the model version that simply copies a stream unmodified.
/// If you need to edit/modify records, that's your chance to hook-up your dark magic.
/// @param fileReader: the open VRS file from which records are read.
/// @param fileWriter: the VRS file to which records should be written, probably by using a
/// Recordable that it will attach to that fileWriter.
/// @param streamId: id of the stream this stream player is responsible for copying or filtering.
/// @param copyOptions: the copy options used for this copy operation.
/// @return A pointer to a stream player that is now attached to the fileReader, and will create
/// records in the fileWriter, as appropriate.
using MakeStreamFilterFunction = std::function<std::unique_ptr<StreamPlayer>(
    RecordFileReader& fileReader,
    RecordFileWriter& fileWriter,
    StreamId streamId,
    const CopyOptions& copyOptions)>;

/// Default MakeStreamFilterFunction to be used by copyRecords() to simply copy a whole stream.
std::unique_ptr<StreamPlayer> makeCopier(
    RecordFileReader& fileReader,
    RecordFileWriter& fileWriter,
    StreamId streamId,
    const CopyOptions& copyOptions);

void copyMergeDoc();

/// Copy records from one file to another, using a filtered reader
/// but otherwise making no changes to records?
/// @param filteredReader: source file with record filtering
/// Because filteredReader using a FileHandler interface, the VRS file
/// might come from a disk, or some other implementation of FileHandler,
/// such as VRSEverstoreFile...
/// @param pathToCopy: path of the destination file
/// @param copyOptions: copy parameters, such as compression preset, chunking behavior, etc.
/// @param uploadMetadata: optional. When specified, the data is streamed-up to Gaia.
/// The file path is used as a temporary location during the upload, and auto deleted on the way.
/// @return a status code, with 0 meaning success, and any non-zero value is a failure, in which
/// case an error message will be printed in cerr.
int copyRecords(
    FilteredVRSFileReader& filteredReader,
    const std::string& pathToCopy,
    const CopyOptions& copyOptions,
    std::unique_ptr<UploadMetadata>&& uploadMetadata = nullptr,
    MakeStreamFilterFunction makeStreamFilter = makeCopier);

/// Merge records from multiple files to a new file, using multipler filtered readers
/// but otherwise making no changes to records.
/// @param filteredReader: source file with record filtering.
/// Because filteredReader using a FileHandler interface, the VRS file
/// might come from a disk, or some other implementation of FileHandler,
/// such as VRSEverstoreFile...
/// @param moreRecordFilters: more source files with filterering, which might be any FileHandler
/// implementation.
/// @param pathToCopy: path of the destination file
/// @param copyOptions: copy parameters, such as compression preset, chunking behavior, etc.
/// @param uploadMetadata: optional. When specified, the data is streamed-up to Gaia.
/// The file path is used as a temporary location during the upload, and auto deleted on the way.
/// @return a status code, with 0 meaning success, and any non-zero value is a failure, in which
/// case an error message will be printed in cerr.
int mergeRecords(
    FilteredVRSFileReader& recordFilter,
    std::list<FilteredVRSFileReader>& moreRecordFilters,
    const std::string& pathToCopy,
    const CopyOptions& copyOptions,
    std::unique_ptr<UploadMetadata>&& uploadMetadata = nullptr);

/// Download records using streaming functionality.
/// @param downloadFilteredReader: a source file with record filtering.
/// @param downloadFolder: folder in which to download the file.
/// @param copyOptions: copy parameters, such as compression preset, chunking behavior, etc.
/// @return a status code, with 0 meaning success, and any non-zero value is a failure, in which
/// case an error message will be printed in cerr.
int downloadRecords(
    FilteredVRSFileReader& downloadFilteredReader,
    const std::string& downloadFolder,
    const CopyOptions& copyOptions);

/// Update a VRS file and creates a new version of the file in Gaia, record-by-record, reprocessing
/// it on the fly, with optional filters.
/// @param updateFilteredReader: a source file in Gaia with record filtering.
/// @param copyOptions: copy parameters, such as compression preset, etc.
/// @return a status code, with 0 meaning success, and any non-zero value is a failure, in which
/// case an error message will be printed in cerr.
int updateRecords(
    GaiaId updateId,
    FilteredVRSFileReader& updateFilteredReader,
    const CopyOptions& copyOptions);

} // namespace vrs::utils
