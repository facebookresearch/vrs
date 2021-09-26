// (c) Facebook, Inc. and its affiliates. Confidential and proprietary.

#pragma once

#include <ostream>
#include <set>
#include <string>

#include <vrs/ForwardDefinitions.h>

namespace vrs {

namespace RecordFileInfo {

using std::ostream;
using std::set;
using std::string;

enum class Details : uint32_t {

  None = 0,

  Basics = 1,

  ChunkList = 1 << 1,

  ListFileTags = 1 << 2,
  MainCounters = 1 << 3,

  StreamNames = 1 << 4,
  StreamTags = 1 << 5,
  StreamRecordCounts = 1 << 6,

  Overview = MainCounters,
  Counters = MainCounters | StreamRecordCounts,

  Everything = (1 << 24) - 1, // 24 bits for data selection

  // presentation flags
  UsePublicNames = 1 << 24,
};

/// Combine flags
inline Details operator|(const Details& lhs, const Details& rhs) {
  return static_cast<Details>(static_cast<uint32_t>(lhs) | static_cast<uint32_t>(rhs));
}

/// Test than two sets of flags intersect (at least one flag is set in both sets)
inline bool operator&&(const Details& lhs, const Details& rhs) {
  return (static_cast<uint32_t>(lhs) & static_cast<uint32_t>(rhs)) != 0;
}

/// Print a human readable overview information for a given VRS file,
/// showing data for all its streams.
/// @param out: Output stream.
/// @param path: Path to the VRS file.
/// @param details: Mask to tell which details to print out.
/// @return 0 if the file was found and could be opened, something else otherwise.
int printOverview(ostream& out, const string& path, Details details = Details::Overview);

/// Print a human readable overview information for a given VRS file, for all its streams.
/// @param out: Output stream.
/// @param recordFile: RecordFileReader with an open file.
/// @param details: Mask to tell which details to print out.
void printOverview(ostream& out, RecordFileReader& recordFile, Details details = Details::Overview);

/// Print a human readable overview information for a given VRS file,
/// showing data for subset of its streams.
/// @param out: Output stream.
/// @param recordFile: RecordFileReader with an open file.
/// @param streamIds: Set of StreamId of the streams to show information for.
/// @param details: Mask to tell which details to print out.
void printOverview(
    ostream& out,
    RecordFileReader& recordFile,
    const set<StreamId>& streamIds,
    Details details = Details::Overview);

/// Generate an overview description of given VRS file as json,
/// showing data for all its streams.
/// @param path: Path to the VRS file.
/// @param details: Mask to tell which details to print out.
/// @return A json description
string jsonOverview(const string& path, Details details = Details::Overview);

/// Generate an overview description of given VRS file as json,
/// showing data for all its streams.
/// @param recordFile: RecordFileReader with an open file.
/// @param details: Mask to tell which details to print out.
/// @return A json description
string jsonOverview(RecordFileReader& recordFile, Details details = Details::Overview);

/// Generate an overview description of given VRS file as json,
/// showing data for subset of its streams.
/// @param recordFile: RecordFileReader with an open file.
/// @param streams: set of streams to describe.
/// @param details: Mask to tell which details to print out.
/// @return A json description
string jsonOverview(
    RecordFileReader& recordFile,
    const set<StreamId>& streams,
    Details details = Details::Overview);

} // namespace RecordFileInfo
} // namespace vrs
