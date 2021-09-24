// (c) Facebook, Inc. and its affiliates. Confidential and proprietary.

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
/// @param outRecodIndex: VRS file index read.
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
