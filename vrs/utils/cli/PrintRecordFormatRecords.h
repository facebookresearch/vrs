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

/// Selection of a print format
enum class PrintoutType {
  None,
  Details, ///< Print all DataLayout specs of the fields (less readable, but more complete)
  Compact, ///< Print the most relevant information of DataLayout fields (most human readable)
  JsonCompact, ///< Print in json format. Includes all details, but very hard to read by a human.
  JsonPretty ///< Print in json format. Same details, but with space and newlines for humans.
};

/// Print records using the RecordFormat conventions, in particular, print DataLayout blocks.
/// Binary content blocks such as images, audio, and custom blocks will only be described at the
/// RecordFormat level (don't expect an hex data print in particular).
/// @param filteredReader: the file to print, with filters
/// @param type: the type of print output selected (see enum definition)
void printRecordFormatRecords(FilteredFileReader& filteredReader, PrintoutType type);

} // namespace vrs::utils
