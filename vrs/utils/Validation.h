// (c) Facebook, Inc. and its affiliates. Confidential and proprietary.

#pragma once

#include <list>
#include <string>

#include <vrs/Compressor.h>

#include "CopyHelpers.h"
#include "FilteredVRSFileReader.h"

namespace vrs::utils {

enum class CheckType { None, Check, Checksum, ChecksumVerbatim, Checksums, HexDump, Count };

/// Check a VRS file by reading all its records & counting errors.
/// The file should be open & filters applied already.
/// @param filteredReader: source file with record filtering
/// @param copyOptions: to disable progress logging. Other parameters are not applicable.
/// @param checkType: which type of check is expected.
std::string checkRecords(
    FilteredVRSFileReader& filteredReader,
    const CopyOptions& copyOptions,
    CheckType checkType);

/// Helpers to simplify unit tests validation
std::string recordsChecksum(const std::string& path, bool showProgress);
std::string verbatimChecksum(const std::string& path, bool showProgress);

enum class CompareType { None, Compare, CompareVerbatim, Count };

/// Compare VRS files from a data standpoint, comparing stream & file tags, the count of streams,
/// and records one by one, while respecting filters, so that you can compare parts of files.
/// This is much faster than comparing checksums, because md5 checksum calculation is fairly
/// expensive when dealing with GB of data...
/// Both files are expected to be opened already, all filters applied.
/// @param recordFilter: original file to compare, opened, with filters applied.
/// @param other: file to compare with, opened, with filters applied.
/// @param copyOptions: to disable progress logging. Other parameters are not applicable.
/// @param compareType: which type of compare is expected.
bool compareVRSfiles(
    FilteredVRSFileReader& first,
    FilteredVRSFileReader& second,
    const CopyOptions& copyOptions);

bool compareVerbatim(
    FilteredVRSFileReader& first,
    FilteredVRSFileReader& second,
    bool showProgress);

} // namespace vrs::utils
