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

#include <vrs/Compressor.h>
#include <vrs/utils/FilteredFileReader.h>

namespace vrs::utils {

enum class CheckType { None, Check, Checksum, ChecksumVerbatim, Checksums, HexDump, Decode, COUNT };

/// Check a VRS file by reading all its records & counting errors.
/// The file should be open & filters applied already.
/// @param filteredReader: source file with record filtering
/// @param copyOptions: to disable progress logging. Other parameters are not applicable.
/// @param checkType: which type of check is expected, except ChecksumVerbatim (see API below).
string checkRecords(
    FilteredFileReader& filteredReader,
    const CopyOptions& copyOptions,
    CheckType checkType);

/// Helpers to simplify unit tests validation
string recordsChecksum(const string& path, bool showProgress);
string verbatimChecksum(const string& path, bool showProgress);

enum class CompareType { None, Compare, CompareVerbatim, COUNT };

/// Compare VRS files from a data standpoint, comparing stream & file tags, the count of streams,
/// and records one by one, while respecting filters, so that you can compare parts of files.
/// Two files losslessly compressed differently, with different indexes, may well be equal.
/// Both files are expected to be opened already, all filters applied.
/// @param first: VRS file to compare, opened, with filters applied.
/// @param second: other VRS file to compare against, opened, with filters applied.
/// @param copyOptions: to disable progress logging. Other parameters are not applicable.
bool compareVRSfiles(
    FilteredFileReader& first,
    FilteredFileReader& second,
    const CopyOptions& copyOptions);

/// Compare files at the byte level. Useful to validate upload copy/upload operations.
/// @param first: First file (not necessarily a VRS file)
/// @param second: Second file (not necessarily a VRS file)
bool compareVerbatim(const FileSpec& first, const FileSpec& second, bool showProgress);

} // namespace vrs::utils
