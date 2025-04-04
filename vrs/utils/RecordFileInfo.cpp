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

#include "RecordFileInfo.h"

#include <iomanip>
#include <sstream>

#include <fmt/format.h>

#include <vrs/helpers/Rapidjson.hpp>
#include <vrs/helpers/Strings.h>
#include <vrs/os/System.h>

#include <vrs/ErrorCode.h>
#include <vrs/IndexRecord.h>
#include <vrs/RecordFileReader.h>
#include <vrs/TagConventions.h>

using namespace std;
using namespace vrs_rapidjson;

using JsonDocument =
    vrs_rapidjson::GenericDocument<vrs_rapidjson::UTF8<char>, vrs_rapidjson::CrtAllocator>;
using JsonValue =
    vrs_rapidjson::GenericValue<vrs_rapidjson::UTF8<char>, vrs_rapidjson::CrtAllocator>;

namespace vrs {
namespace RecordFileInfo {

using helpers::humanReadableDuration;
using helpers::humanReadableFileSize;
using helpers::humanReadableTimestamp;
using helpers::make_printable;

namespace {

void printCountedName(ostream& out, size_t count, const string& name, bool capital = false) {
  if (count == 0) {
    out << (capital ? "No " : "no ") << name << 's';
  } else if (count == 1) {
    out << "1 " << name;
  } else {
    out << count << ' ' << name << 's';
  }
}

void printTime(
    ostream& out,
    const IndexRecord::RecordInfo* firstRecord,
    const IndexRecord::RecordInfo* lastRecord,
    uint32_t recordCount,
    bool showFps) {
  if (firstRecord != nullptr) {
    if (lastRecord != nullptr && lastRecord != firstRecord) {
      // multiple records
      out << "from " << humanReadableTimestamp(firstRecord->timestamp) << " to "
          << humanReadableTimestamp(lastRecord->timestamp) << " ("
          << humanReadableDuration(lastRecord->timestamp - firstRecord->timestamp);
      if (showFps && recordCount > 1 && firstRecord->timestamp < lastRecord->timestamp) {
        out << fmt::format(
            ", {:.4g}rps", (recordCount - 1.) / (lastRecord->timestamp - firstRecord->timestamp));
      }
      out << ')';
    } else {
      // only one record
      out << "at " << humanReadableTimestamp(firstRecord->timestamp);
    }
  }
}

void appendTruncated(string& line, const string& extra, Details details, size_t width) {
  if (details & Details::CompleteTags) {
    line.append(extra);
  } else {
    if (line.size() >= width) {
      line.resize(width);
    } else {
      if (line.size() + extra.size() > width) {
        line.append(extra.substr(0, width - line.size()));
      } else {
        line.append(extra);
      }
    }
  }
}

void printTags(ostream& out, string_view prefix, const map<string, string>& tags, Details details) {
  const size_t width = os::getTerminalWidth();
  string line;
  line.reserve(width * 2);
  for (const auto& iter : tags) {
    line.clear();
    line.append(prefix).append(iter.first).append(" = ");
    appendTruncated(line, iter.second, details, width);
    if (iter.first == tag_conventions::kCaptureTimeEpoch) {
      time_t creationTimeSec = static_cast<time_t>(stoul(iter.second));
      if (creationTimeSec > 1000000) {
        stringstream ss;
        ss << put_time(localtime(&creationTimeSec), " -- %c %Z");
        appendTruncated(line, ss.str(), details, width);
      }
    }
    string printable = make_printable(line);
    if (printable.size() > width - 3 && !(details & Details::CompleteTags)) {
      out << printable.substr(0, width - 3) << "...\n";
    } else {
      out << printable << "\n";
    }
  }
}

struct RecordCounter {
  uint32_t recordCount = 0;

  const IndexRecord::RecordInfo* firstRecord = nullptr;
  const IndexRecord::RecordInfo* lastRecord = nullptr;

  void count(const IndexRecord::RecordInfo* record) {
    recordCount++;
    if (firstRecord == nullptr) {
      firstRecord = record;
    }
    lastRecord = record;
  }
  void print(ostream& out, const string& name, bool showFps) const {
    if (recordCount == 0) {
      out << "  No " << name << " records.\n";
    } else {
      out << "  ";
      printCountedName(out, recordCount, name + " record");
      out << ", ";
      printTime(out, firstRecord, lastRecord, recordCount, showFps);
      out << ".\n";
    }
  }
};

void overView(ostream& out, RecordFileReader& file, StreamId id, Details details) {
  const auto& index = file.getIndex(id);
  stringstream name;
  string currentName = id.getTypeName();
  const string& originalName = file.getOriginalRecordableTypeName(id);
  string nowKnownAsName;
  if (currentName == originalName) {
    name << currentName;
  } else {
    name << originalName;
    // If the device name in the recording is different that the currently used name,
    // let's surface that.
    if (StreamId::isKnownTypeId(id.getTypeId())) {
      nowKnownAsName = currentName;
    }
  }
  name << " #" << static_cast<int>(id.getInstanceId());
  const string& flavor = file.getFlavor(id);
  if (!flavor.empty()) {
    name << " - " << flavor;
  }
  name << " [" << id.getNumericName() << "] record";
  printCountedName(out, index.size(), name.str(), true);
  if (!nowKnownAsName.empty()) {
    out << " (device now known as \"" << nowKnownAsName << "\")";
  }
  if (details & Details::StreamRecordSizes) {
    size_t size = 0;
    for (const auto& record : index) {
      size += file.getRecordSize(file.getRecordIndex(record));
    }
    out << ", " << humanReadableFileSize(size);
  }
  out << ".\n";
  if (details & Details::StreamTags) {
    const StreamTags& tags = file.getTags(id);
    printTags(out, "  VRS Tag: ", tags.vrs, details);
    printTags(out, "  Tag: ", tags.user, details);
  }
  if (details & Details::StreamRecordCounts) {
    RecordCounter configRecords, stateRecords, dataRecords;
    for (const auto& record : index) {
      switch (record->recordType) {
        case Record::Type::DATA:
          dataRecords.count(record);
          break;
        case Record::Type::CONFIGURATION:
          configRecords.count(record);
          break;
        case Record::Type::STATE:
          stateRecords.count(record);
          break;
        default:
          break;
      }
    }
    configRecords.print(out, "configuration", false);
    stateRecords.print(out, "state", false);
    dataRecords.print(out, "data", true);
  }
}

} // namespace

int printOverview(ostream& out, const string& path, Details details) {
  RecordFileReader recordFile;
  int status = recordFile.openFile(path);
  if (status == 0) {
    printOverview(out, recordFile, recordFile.getStreams(), details);
  }
  return status;
}

void printOverview(ostream& out, RecordFileReader& recordFile, Details details) {
  printOverview(out, recordFile, recordFile.getStreams(), details);
}

void printOverview(
    ostream& out,
    RecordFileReader& recordFile,
    const set<StreamId>& streamIds,
    Details details) {
  if (!recordFile.isOpened()) {
    out << "No open file.\n";
    return;
  }
  const vector<pair<string, int64_t>> chunks = recordFile.getFileChunks();
  if (chunks.empty()) {
    out << "No chunks found.\n";
  } else if (chunks.size() == 1) {
    const pair<string, int64_t>& file = chunks[0];
    out << "VRS file: '" << file.first << "', " << humanReadableFileSize(file.second) << ".\n";
  } else {
    int64_t totalSize = 0;
    for (const auto& chunk : chunks) {
      totalSize += chunk.second;
    }
    out << "VRS file with " << chunks.size() << " chunks, " << humanReadableFileSize(totalSize)
        << " total";
    if (details & Details::ChunkList) {
      out << ":\n";
      for (size_t index = 0; index < chunks.size(); index++) {
        const pair<string, int64_t>& chunk = chunks[index];
        out << "  Chunk #" << index << ": '" << chunk.first << "', "
            << humanReadableFileSize(chunk.second) << ".\n";
      }
    } else {
      out << ", starting with " << chunks[0].first << ".\n";
    }
  }
  RecordCounter recordCounter;
  const auto& index = recordFile.getIndex();
  for (const auto& record : index) {
    if (streamIds.find(record.streamId) != streamIds.end()) {
      recordCounter.count(&record);
    }
  }
  if (details & Details::MainCounters) {
    out << "Found ";
    printCountedName(out, streamIds.size(), "stream");
    out << ", ";
    printCountedName(out, recordCounter.recordCount, "record");

    // considering only data records, calculate a data record range and data record per second
    uint32_t first = 0; // count of non-data records before the first data record
    while (first < index.size() && index[first].recordType != Record::Type::DATA) {
      ++first;
    }
    if (first < index.size()) {
      uint32_t last = 0;
      uint32_t recordCount = 0;
      for (uint32_t k = first; k < index.size(); ++k) {
        if (index[k].recordType == Record::Type::DATA) {
          last = k;
          ++recordCount;
        }
      }
      if (recordCount == 1) {
        out << ", 1 data record ";
      } else {
        out << ", " << recordCount << " data records ";
      }
      printTime(out, &index[first], &index[last], recordCount, true);
    } else {
      out << ", no data records";
    }
    out << ".\n";
  }
  if (details & Details::ListFileTags) {
    const auto& tags = recordFile.getTags();
    printTags(out, "  Tag: ", tags, details);
  }
  if (details & (Details::StreamNames | Details::StreamTags | Details::StreamRecordCounts)) {
    for (auto id : streamIds) {
      overView(out, recordFile, id, details);
    }
  }
}

static double getTimestamp(const IndexRecord::RecordInfo* record) {
  if (record != nullptr) {
    return record->timestamp;
  }
  return -1;
}
static JsonValue stringToJvalue(const string& str, JsonDocument::AllocatorType& allocator) {
  JsonValue jstring;
  jstring.SetString(str.c_str(), static_cast<SizeType>(str.length()), allocator);
  return jstring;
}

static void addTimeFrameMembers(
    JsonValue& jValue,
    RecordCounter& recordData,
    JsonDocument::AllocatorType& allocator) {
  JsonValue numOfRec = stringToJvalue("number_of_records", allocator);
  jValue.AddMember(numOfRec, recordData.recordCount, allocator);
  if (recordData.recordCount > 0) {
    JsonValue sTime = stringToJvalue("start_time", allocator);
    jValue.AddMember(sTime, getTimestamp(recordData.firstRecord), allocator);
    JsonValue eTime = stringToJvalue("end_time", allocator);
    jValue.AddMember(eTime, getTimestamp(recordData.lastRecord), allocator);
  }
}

static JsonValue devicesOverView(
    RecordFileReader& file,
    StreamId id,
    Details details,
    JsonDocument::AllocatorType& allocator) {
  JsonValue streamData(kObjectType);
  const auto& index = file.getIndex(id);

  if (details & Details::StreamNames) {
    bool pub = details & Details::UsePublicNames;
    JsonValue recName = stringToJvalue(pub ? "device_name" : "recordable_name", allocator);
    streamData.AddMember(recName, stringToJvalue(id.getTypeName(), allocator), allocator);
    JsonValue recTypId = stringToJvalue(pub ? "device_type_id" : "recordable_id", allocator);
    streamData.AddMember(recTypId, static_cast<int>(id.getTypeId()), allocator);
    JsonValue recInstId = stringToJvalue(pub ? "device_instance_id" : "instance_id", allocator);
    streamData.AddMember(recInstId, id.getInstanceId(), allocator);
    const string& flavor = file.getFlavor(id);
    if (!flavor.empty()) {
      JsonValue recFlavor = stringToJvalue(pub ? "device_flavor" : "recordable_flavor", allocator);
      streamData.AddMember(recFlavor, stringToJvalue(flavor, allocator), allocator);
    }

    const string& name = file.getOriginalRecordableTypeName(id);
    if (name != id.getTypeName()) {
      streamData.AddMember(
          stringToJvalue(pub ? "device_original_name" : "recordable_original_name", allocator),
          stringToJvalue(name, allocator),
          allocator);
    }
  }

  if (details & Details::StreamTags) {
    const StreamTags& tags = file.getTags(id);

    JsonValue recordTags(kObjectType);
    for (const auto& iter : tags.user) {
      recordTags.AddMember(
          stringToJvalue(iter.first, allocator),
          stringToJvalue(make_printable(iter.second), allocator),
          allocator);
    }
    streamData.AddMember(stringToJvalue("tags", allocator), recordTags, allocator);

    JsonValue VRStags(kObjectType);
    for (const auto& iter : tags.vrs) {
      VRStags.AddMember(
          stringToJvalue(iter.first, allocator),
          stringToJvalue(make_printable(iter.second), allocator),
          allocator);
    }
    streamData.AddMember(stringToJvalue("vrs_tag", allocator), VRStags, allocator);
  }

  if (details & Details::StreamRecordCounts) {
    RecordCounter configRecords, stateRecords, dataRecords;
    JsonValue configRecordsValue(kObjectType);
    JsonValue stateRecordsValue(kObjectType);
    JsonValue dataRecordsValue(kObjectType);
    for (const auto& record : index) {
      switch (record->recordType) {
        case Record::Type::DATA:
          dataRecords.count(record);
          break;
        case Record::Type::CONFIGURATION:
          configRecords.count(record);
          break;
        case Record::Type::STATE:
          stateRecords.count(record);
          break;
        default:
          break;
      }
    }
    addTimeFrameMembers(dataRecordsValue, dataRecords, allocator);
    addTimeFrameMembers(configRecordsValue, configRecords, allocator);
    addTimeFrameMembers(stateRecordsValue, stateRecords, allocator);

    streamData.AddMember(stringToJvalue("configuration", allocator), configRecordsValue, allocator);
    streamData.AddMember(stringToJvalue("state", allocator), stateRecordsValue, allocator);
    streamData.AddMember(stringToJvalue("data", allocator), dataRecordsValue, allocator);
  }

  if (details & Details::StreamRecordSizes) {
    int64_t size = 0;
    for (const auto& record : index) {
      size += file.getRecordSize(file.getRecordIndex(record));
    }
    JsonValue streamSize = stringToJvalue("stream_size", allocator);
    streamData.AddMember(streamSize, size, allocator);
  }

  return streamData;
}

string jsonOverview(const string& path, Details details) {
  RecordFileReader recordFile;
  int status = recordFile.openFile(path);
  if (status == 0) {
    return jsonOverview(recordFile, recordFile.getStreams(), details);
  }

  // We can't open the file, generate a json string that contains the error
  JsonDocument doc;
  doc.SetObject();
  JsonDocument::AllocatorType& allocator = doc.GetAllocator();
  doc.AddMember(stringToJvalue("file_name", allocator), stringToJvalue(path, allocator), allocator);
  JsonValue errorCodeId = stringToJvalue("error_code", allocator);
  doc.AddMember(errorCodeId, status, allocator);
  string error = errorCodeToMessage(status);
  doc.AddMember(
      stringToJvalue("error_message", allocator), stringToJvalue(error, allocator), allocator);

  return jDocumentToJsonString(doc);
}

string jsonOverview(RecordFileReader& recordFile, Details details) {
  return jsonOverview(recordFile, recordFile.getStreams(), details);
}

string jsonOverview(RecordFileReader& recordFile, const set<StreamId>& streams, Details details) {
  JsonDocument doc;
  doc.SetObject();
  JsonDocument::AllocatorType& allocator = doc.GetAllocator();

  int64_t fileSize = 0;
  const vector<pair<string, int64_t>> chunks = recordFile.getFileChunks();
  const pair<string, int64_t>& file = chunks[0];
  if (details & Details::Basics) {
    doc.AddMember(
        stringToJvalue("file_name", allocator), stringToJvalue(file.first, allocator), allocator);
  }
  if (chunks.size() == 1) {
    fileSize = file.second;
  } else {
    fileSize = 0;
    for (const auto& chunk : chunks) {
      fileSize += chunk.second;
    }
  }
  if (details & Details::ChunkList) {
    JsonValue fileChunks(kArrayType);
    fileChunks.Reserve(static_cast<SizeType>(chunks.size()), allocator);
    for (const auto& chunk : chunks) {
      fileChunks.PushBack(stringToJvalue(chunk.first, allocator), allocator);
    }
    doc.AddMember(stringToJvalue("file_chunks", allocator), fileChunks, allocator);
  }
  if (details & Details::Basics) {
    JsonValue fileSizeShortId = stringToJvalue("file_size_short", allocator);
    doc.AddMember(
        fileSizeShortId, stringToJvalue(humanReadableFileSize(fileSize), allocator), allocator);
    JsonValue fileSizeId = stringToJvalue("file_size", allocator);
    doc.AddMember(fileSizeId, fileSize, allocator);
  }

  if (details & Details::ListFileTags) {
    JsonValue recordTags(kObjectType);
    const auto& tags = recordFile.getTags();
    for (const auto& tag : tags) {
      JsonValue jstringFirst = stringToJvalue(tag.first, allocator);
      JsonValue jstringSecond = stringToJvalue(make_printable(tag.second), allocator);
      recordTags.AddMember(jstringFirst, jstringSecond, allocator);
    }
    doc.AddMember(stringToJvalue("tags", allocator), recordTags, allocator);
  }

  if (details & Details::MainCounters) {
    JsonValue numOfDevices = stringToJvalue("number_of_devices", allocator);
    doc.AddMember(numOfDevices, static_cast<uint64_t>(streams.size()), allocator);
    size_t recordCount = 0;
    double startTime = numeric_limits<double>::max();
    double endTime = numeric_limits<double>::lowest();
    bool addTimes = false;
    for (auto id : streams) {
      const auto& index = recordFile.getIndex(id);
      if (!index.empty()) {
        recordCount += index.size();
        if (index.front()->timestamp < startTime) {
          startTime = index.front()->timestamp;
        }
        if (index.back()->timestamp > endTime) {
          endTime = index.back()->timestamp;
        }
        addTimes = true;
      }
    }
    JsonValue numOfRec = stringToJvalue("number_of_records", allocator);
    doc.AddMember(numOfRec, static_cast<uint64_t>(recordCount), allocator);
    if (addTimes) {
      JsonValue sTime = stringToJvalue("start_time", allocator);
      doc.AddMember(sTime, startTime, allocator);
      JsonValue eTime = stringToJvalue("end_time", allocator);
      doc.AddMember(eTime, endTime, allocator);
    }
  }

  if (details & (Details::StreamNames | Details::StreamTags | Details::StreamRecordCounts)) {
    JsonValue devices(kArrayType);
    for (auto id : streams) {
      devices.PushBack(devicesOverView(recordFile, id, details, allocator), allocator);
    }
    doc.AddMember(stringToJvalue("devices", allocator), devices, allocator);
  }

  return jDocumentToJsonString(doc);
}

} // namespace RecordFileInfo
} // namespace vrs
