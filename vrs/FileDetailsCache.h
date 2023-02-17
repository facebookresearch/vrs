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

#include <map>
#include <set>
#include <string>
#include <vector>

#include "ForwardDefinitions.h"

namespace vrs {

using std::map;
using std::set;
using std::string;
using std::vector;

/// Helper methods to read & write file details in a cache file.
namespace FileDetailsCache {

/// Create a VRS details file cache at a disk location, in one synchronous operation.
/// @param cacheFile: path to the cache file to write.
/// @param streamIds: stream IDs to save.
/// @param fileTags: file tags to save.
/// @param streamTags: stream tags to save.
/// @param recordIndex: VRS file index to save.
/// @param fileHasIndex: does the original VRS file have a proper index.
/// @return A status code, 0 for success.
int write(
    const string& cacheFile,
    const set<StreamId>& streamIds,
    const map<string, string>& fileTags,
    const map<StreamId, StreamTags>& streamTags,
    const vector<IndexRecord::RecordInfo>& recordIndex,
    bool fileHasIndex);

/// Read a VRS details file cache from a disk location.
/// @param cacheFile: path to the cache file to read.
/// @param outStreamIds: stream Ids read.
/// @param outFileTags: file tags read.
/// @param outStreamTags: stream tags read.
/// @param outRecordIndex: VRS file index read.
/// @param outFileHasIndex: does the original VRS file have a proper index.
/// @return A status code, 0 for success.
int read(
    const string& cacheFile,
    set<StreamId>& outStreamIds,
    map<string, string>& outFileTags,
    map<StreamId, StreamTags>& outStreamTags,
    vector<IndexRecord::RecordInfo>& outRecordIndex,
    bool& outFileHasIndex);

} // namespace FileDetailsCache

} // namespace vrs
