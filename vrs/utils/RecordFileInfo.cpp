// Copyright (c) Facebook, Inc. and its affiliates. Confidential and proprietary.

#include "RecordFileInfo.h"

#include <cctype>
#include <cmath>

#include <iomanip>
#include <sstream>

#include <fmt/format.h>

#include <Serialization/Rapidjson.h>

#include <vrs/helpers/Strings.h>

#include <vrs/ErrorCode.h>
#include <vrs/IndexRecord.h>
#include <vrs/RecordFileReader.h>
#include <vrs/TagConventions.h>

using namespace std;
using namespace fb_rapidjson;

using JsonDocument =
    fb_rapidjson::GenericDocument<fb_rapidjson::UTF8<char>, fb_rapidjson::CrtAllocator>;
using JsonValue = fb_rapidjson::GenericValue<fb_rapidjson::UTF8<char>, fb_rapidjson::CrtAllocator>;

namespace vrs {
namespace RecordFileInfo {

using helpers::humanReadableDuration;
using helpers::humanReadableFileSize;
using helpers::humanReadableTimestamp;
using helpers::make_printable;

namespace {

void printCountedName(ostream& out, size_t count, const string& name) {
  if (count == 0) {
    out << "no " << name;
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
        out << ", " << setprecision(2)
            << (recordCount - 1.) / (lastRecord->timestamp - firstRecord->timestamp) << "rps";
      }
      out << ')';
    } else {
      // only one record
      out << "at " << humanReadableTimestamp(firstRecord->timestamp);
    }
  }
}

void printTags(ostream& out, const map<string, string>& tags) {
  for (const auto& iter : tags) {
    out << "  Tag: " << iter.first << " = " << make_printable(iter.second);
    if (iter.first == tag_conventions::kCaptureTimeEpoch) {
      time_t creationTimeSec = static_cast<time_t>(std::stoul(iter.second));
      if (creationTimeSec > 1000000) {
        cout << put_time(localtime(&creationTimeSec), " -- %c %Z");
      }
    }
    out << endl;
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
      out << "  No " << name << " record." << endl;
    } else {
      out << "  ";
      printCountedName(out, recordCount, name + " record");
      out << ", ";
      printTime(out, firstRecord, lastRecord, recordCount, showFps);
      out << "." << endl;
    }
  }
};

const size_t kMaxVRSTagLineLength = 150;

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
  string flavor = file.getFlavor(id);
  if (!flavor.empty()) {
    name << " - " << flavor;
  }
  name << " [" << id.getNumericName() << "] record";
  printCountedName(out, index.size(), name.str());
  if (!nowKnownAsName.empty()) {
    out << " (device now known as \"" << nowKnownAsName << "\")";
  }
  out << "." << endl;
  if (details && Details::StreamTags) {
    const StreamTags& tags = file.getTags(id);
    for (const auto& iter : tags.vrs) {
      stringstream ss;
      ss << "  VRS Tag: " << iter.first << " = " << iter.second;
      string t = ss.str();
      if (t.size() > kMaxVRSTagLineLength) {
        out << make_printable(t.substr(0, kMaxVRSTagLineLength)) << "..." << endl;
      } else {
        out << make_printable(t) << endl;
      }
    }
    printTags(out, tags.user);
  }
  if (details && Details::StreamRecordCounts) {
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
    out << "No open file." << endl;
    return;
  }
  const vector<std::pair<string, int64_t>> chunks = recordFile.getFileChunks();
  if (chunks.empty()) {
    out << "No chunk found." << endl;
  } else if (chunks.size() == 1) {
    const std::pair<string, int64_t>& file = chunks[0];
    out << "VRS file: '" << file.first << "', " << humanReadableFileSize(file.second) << "."
        << endl;
  } else {
    int64_t totalSize = 0;
    for (const auto& chunk : chunks) {
      totalSize += chunk.second;
    }
    out << "VRS file with " << chunks.size() << " chunks, " << humanReadableFileSize(totalSize)
        << " total";
    if (details && Details::ChunkList) {
      out << ":" << endl;
      for (size_t index = 0; index < chunks.size(); index++) {
        const std::pair<string, int64_t>& chunk = chunks[index];
        out << "  Chunk #" << index << ": '" << chunk.first << "', " << (chunk.second) << "."
            << endl;
      }
    } else {
      out << ", starting with " << chunks[0].first << '.' << endl;
    }
  }
  if (details && Details::ListFileTags) {
    const auto& tags = recordFile.getTags();
    printTags(out, tags);
  }
  RecordCounter recordCounter;
  const auto& index = recordFile.getIndex();
  for (const auto& record : index) {
    if (streamIds.find(record.streamId) != streamIds.end()) {
      recordCounter.count(&record);
    }
  }
  if (details && Details::MainCounters) {
    out << "Found ";
    printCountedName(out, streamIds.size(), "device");
    out << ", ";
    printCountedName(out, recordCounter.recordCount, "record");
    if (!index.empty()) {
      out << ", ";
      printTime(out, &index[0], &index[index.size() - 1], 0, false);
    }
    // using the time range of data records, calculate a record per second rate
    double first = 0;
    double last = 0;
    size_t skipCount = 0; // count of non-data records before the first data record
    for (const auto& record : index) {
      ++skipCount;
      if (record.recordType == Record::Type::DATA) {
        first = record.timestamp;
        break;
      }
    }
    for (auto iter = index.rbegin(); iter != index.rend(); ++iter) {
      if (iter->recordType == Record::Type::DATA) {
        last = iter->timestamp;
        break;
      }
    }
    if (last > first) {
      out << ", " << setprecision(2) << 1. * (index.size() - skipCount) / (last - first) << "rps";
    }
    out << "." << endl;
  }
  if (details && (Details::StreamNames | Details::StreamTags | Details::StreamRecordCounts)) {
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
  JsonValue recordData(kObjectType);
  const auto& index = file.getIndex(id);

  if (details && Details::StreamNames) {
    bool pub = details && Details::UsePublicNames;
    JsonValue recName = stringToJvalue(pub ? "device_name" : "recordable_name", allocator);
    recordData.AddMember(recName, stringToJvalue(id.getTypeName(), allocator), allocator);
    JsonValue recTypId = stringToJvalue(pub ? "device_type_id" : "recordable_id", allocator);
    recordData.AddMember(recTypId, static_cast<int>(id.getTypeId()), allocator);
    JsonValue recInstId = stringToJvalue(pub ? "device_instance_id" : "instance_id", allocator);
    recordData.AddMember(recInstId, id.getInstanceId(), allocator);
    string flavor = file.getFlavor(id);
    if (!flavor.empty()) {
      JsonValue recFlavor = stringToJvalue(pub ? "device_flavor" : "recordable_flavor", allocator);
      recordData.AddMember(recFlavor, stringToJvalue(flavor, allocator), allocator);
    }

    const string& name = file.getOriginalRecordableTypeName(id);
    if (name != id.getTypeName()) {
      recordData.AddMember(
          stringToJvalue(pub ? "device_original_name" : "recordable_original_name", allocator),
          stringToJvalue(name, allocator),
          allocator);
    }
  }

  if (details && Details::StreamTags) {
    const StreamTags& tags = file.getTags(id);

    JsonValue recordTags(kObjectType);
    for (const auto& iter : tags.user) {
      recordTags.AddMember(
          stringToJvalue(iter.first, allocator),
          stringToJvalue(make_printable(iter.second), allocator),
          allocator);
    }
    recordData.AddMember(stringToJvalue("tags", allocator), recordTags, allocator);

    JsonValue VRStags(kObjectType);
    for (const auto& iter : tags.vrs) {
      VRStags.AddMember(
          stringToJvalue(iter.first, allocator),
          stringToJvalue(make_printable(iter.second), allocator),
          allocator);
    }
    recordData.AddMember(stringToJvalue("vrs_tag", allocator), VRStags, allocator);
  }

  if (details && Details::StreamRecordCounts) {
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

    recordData.AddMember(stringToJvalue("configuration", allocator), configRecordsValue, allocator);
    recordData.AddMember(stringToJvalue("state", allocator), stateRecordsValue, allocator);
    recordData.AddMember(stringToJvalue("data", allocator), dataRecordsValue, allocator);
  }

  return recordData;
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

  fb_rapidjson::StringBuffer strbuf;
  fb_rapidjson::Writer<fb_rapidjson::StringBuffer> writer(strbuf);
  doc.Accept(writer);

  return strbuf.GetString();
}

string jsonOverview(RecordFileReader& recordFile, Details details) {
  return jsonOverview(recordFile, recordFile.getStreams(), details);
}

string jsonOverview(RecordFileReader& recordFile, const set<StreamId>& streams, Details details) {
  JsonDocument doc;
  doc.SetObject();
  JsonDocument::AllocatorType& allocator = doc.GetAllocator();

  int64_t fileSize;
  const vector<std::pair<string, int64_t>> chunks = recordFile.getFileChunks();
  const std::pair<string, int64_t>& file = chunks[0];
  if (details && Details::Basics) {
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
  if (details && Details::ChunkList) {
    JsonValue fileChunks(kArrayType);
    fileChunks.Reserve(static_cast<SizeType>(chunks.size()), allocator);
    for (const auto& chunk : chunks) {
      fileChunks.PushBack(stringToJvalue(chunk.first, allocator), allocator);
    }
    doc.AddMember(stringToJvalue("file_chunks", allocator), fileChunks, allocator);
  }
  if (details && Details::Basics) {
    JsonValue fileSizeShortId = stringToJvalue("file_size_short", allocator);
    doc.AddMember(
        fileSizeShortId, stringToJvalue(humanReadableFileSize(fileSize), allocator), allocator);
    JsonValue fileSizeId = stringToJvalue("file_size", allocator);
    doc.AddMember(fileSizeId, fileSize, allocator);
  }

  if (details && Details::ListFileTags) {
    JsonValue recordTags(kObjectType);
    const auto& tags = recordFile.getTags();
    for (const auto& tag : tags) {
      JsonValue jstringFirst = stringToJvalue(tag.first, allocator);
      JsonValue jstringSecond = stringToJvalue(make_printable(tag.second), allocator);
      recordTags.AddMember(jstringFirst, jstringSecond, allocator);
    }
    doc.AddMember(stringToJvalue("tags", allocator), recordTags, allocator);
  }

  if (details && Details::MainCounters) {
    JsonValue numOfDevices = stringToJvalue("number_of_devices", allocator);
    doc.AddMember(numOfDevices, static_cast<uint64_t>(streams.size()), allocator);
    size_t recordCount = 0;
    double startTime = std::numeric_limits<double>::max();
    double endTime = std::numeric_limits<double>::lowest();
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

  if (details && (Details::StreamNames | Details::StreamTags | Details::StreamRecordCounts)) {
    JsonValue devices(kArrayType);
    for (auto id : streams) {
      devices.PushBack(devicesOverView(recordFile, id, details, allocator), allocator);
    }
    doc.AddMember(stringToJvalue("devices", allocator), devices, allocator);
  }

  fb_rapidjson::StringBuffer strbuf;
  fb_rapidjson::Writer<fb_rapidjson::StringBuffer> writer(strbuf);
  doc.Accept(writer);

  string docStr = strbuf.GetString();

  return docStr;
}

} // namespace RecordFileInfo
} // namespace vrs
