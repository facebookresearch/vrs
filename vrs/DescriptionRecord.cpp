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

#if defined(_MSC_VER)
#pragma warning(disable : 4503) // decorated name length exceeded, name was truncated
#endif

#include "DescriptionRecord.h"

#include <vrs/helpers/Serialization.h>

#define DEFAULT_LOG_CHANNEL "VRSDescriptionRecord"
#include <logging/Log.h>
#include <logging/Verify.h>

#include <vrs/helpers/FileMacros.h>
#include <vrs/utils/xxhash/xxhash.h>

#include "ErrorCode.h"
#include "FileHandler.h"
#include "IndexRecord.h"
#include "Recordable.h"

namespace vrs {

using namespace std;

/*
 *  Description Record format, v1 (VRS 1.0):
 *   RecordHeader (includes the size of the whole record)
 *   LittleEndian<uint32_t> streamDescriptions.size()
 *   for each {id, description} pair in map<StreamId, string> streamDescriptions {
 *     IndexRecord::DiskStreamId id
 *     LittleEndian<uint32_t> description.size()
 *     char[description.size()] description
 *   }
 *   LittleEndian<uint32_t> tagsAsJson.size()
 *   char[tagsAsJson.size()] tagsAsJson
 */

/*
 *  Description Record format, v2 (VRS 2.0):
 *   RecordHeader (includes the size of the whole record)
 *   LittleEndian<uint32_t> streamTags.size()
 *   for each {id, streamTags} pair in map<StreamId, StreamTags> streamTags {
 *     IndexRecord::DiskStreamId id
 *     LittleEndian<uint32_t> streamTags.size()
 *     for each streamTag in streamTags {
 *       LittleEndian<uint32_t> streamTag.size()
 *       LittleEndian<uint32_t> streamTag.user.size()
 *       for each {name, value} pair in streamTag.user
 *         LittleEndian<uint32_t> name.size()
 *         char[name.size()] name
 *         LittleEndian<uint32_t> value.size()
 *         char[value.size()] value
 *       }
 *       for each {name, value} pair in streamTag.vrs
 *         LittleEndian<uint32_t> name.size()
 *         char[name.size()] name
 *         LittleEndian<uint32_t> value.size()
 *         char[value.size()] value
 *       }
 *     }
 *   }
 *   LittleEndian<uint32_t> fileTags.size()
 *   for each {name, value} pair in map<string, string> fileTags {
 *     LittleEndian<uint32_t> name.size()
 *     char[name.size()] name
 *     LittleEndian<uint32_t> value.size()
 *     char[value.size()] value
 *   }
 */

// We used to store the device name, including the device instance number, which doesn't make sense,
// as the instance number may change. Let's remove the instance number. Also useful to make tag
// compares work as expected.
static string stripInstanceId(const string& oldName) {
  if (oldName.size() < 4) {
    return oldName; // "x #1" is the shortest imaginable.
  }
  size_t suffix = oldName.rfind(" #");
  // verify everything after is a digit
  if (suffix == string::npos) {
    return oldName;
  }
  for (size_t idx = suffix + 2; idx < oldName.size(); ++idx) {
    if (!isdigit(static_cast<int>(static_cast<uint8_t>(oldName[idx])))) {
      return oldName;
    }
  }
  return oldName.substr(0, suffix);
}

static const char* kNameLabel = "name";
static const char* kTagsLabel = "tags";

static void jsonToTags(const string& jsonTags, map<string, string>& outTags) {
  using namespace fb_rapidjson;
  outTags.clear();
  Document document;
  document.Parse(jsonTags.c_str());
  if (XR_VERIFY(document.IsObject(), "Improper tags: '{}'", jsonTags)) {
    for (Value::ConstMemberIterator itr = document.MemberBegin(); itr != document.MemberEnd();
         ++itr) {
      if (itr->name.IsString() && itr->value.IsString()) {
        outTags[itr->name.GetString()] = itr->value.GetString();
      }
    }
  }
}

static bool
jsonToNameAndTags(const string& jsonStr, string& outName, map<string, string>& outTags) {
  using namespace fb_rapidjson;
  Document document;
  document.Parse(jsonStr.c_str());
  if (!XR_VERIFY(document.IsObject(), "Improper name & tags")) {
    return false;
  }
  Value::ConstMemberIterator name = document.FindMember(kNameLabel);
  if (XR_VERIFY(
          name != document.MemberEnd() && name->value.IsString(), "missing name in description")) {
    outName = name->value.GetString();
  }
  Value::ConstMemberIterator tags = document.FindMember(kTagsLabel);
  if (tags != document.MemberEnd() && tags->value.IsObject()) {
    for (Value::ConstMemberIterator itr = tags->value.MemberBegin(); itr != tags->value.MemberEnd();
         ++itr) {
      if (itr->name.IsString() && itr->value.IsString()) {
        outTags[itr->name.GetString()] = itr->value.GetString();
      }
    }
  }
  return true;
}

static int writeSize(WriteFileHandler& file, size_t size) {
  FileFormat::LittleEndian<uint32_t> diskSize(static_cast<uint32_t>(size));
  WRITE_OR_LOG_AND_RETURN(file, &diskSize, sizeof(diskSize));
  return 0;
}

static int readSize(FileHandler& file, uint32_t& outSize, uint32_t& dataSizeLeft) {
  FileFormat::LittleEndian<uint32_t> diskSize;
  if (dataSizeLeft < sizeof(diskSize)) {
    return NOT_ENOUGH_DATA;
  }
  if (file.read(diskSize) != 0) {
    return file.getLastError();
  }
  outSize = diskSize.get();
  dataSizeLeft -= sizeof(diskSize);
  return 0;
}

static size_t stringSize(const string& str) {
  return sizeof(uint32_t) + str.size();
}

static int writeString(WriteFileHandler& file, const string& str) {
  IF_ERROR_LOG_AND_RETURN(writeSize(file, str.size()));
  WRITE_OR_LOG_AND_RETURN(file, str.c_str(), str.size());
  return 0;
}

static int readString(FileHandler& file, string& outString, uint32_t& dataSizeLeft) {
  uint32_t charCount = 0;
  IF_ERROR_LOG_AND_RETURN(readSize(file, charCount, dataSizeLeft));
  if (dataSizeLeft < charCount) {
    return NOT_ENOUGH_DATA;
  }
  dataSizeLeft -= charCount;
  outString.resize(charCount);
  if (charCount > 0) {
    if (file.read(&outString.front(), charCount) != 0) {
      return file.getLastError();
    }
  }
  return 0;
}

static size_t mapSize(const map<string, string>& m) {
  size_t size = sizeof(uint32_t);
  for (const auto& pair : m) {
    size += stringSize(pair.first) + stringSize(pair.second);
  }
  return size;
}

static int writeMap(WriteFileHandler& file, const map<string, string>& m) {
  IF_ERROR_LOG_AND_RETURN(writeSize(file, m.size()));
  for (const auto& pair : m) {
    IF_ERROR_LOG_AND_RETURN(writeString(file, pair.first));
    IF_ERROR_LOG_AND_RETURN(writeString(file, pair.second));
  }
  return 0;
}

static int readMap(FileHandler& file, map<string, string>& outMap, uint32_t& sizeLeft) {
  uint32_t count = 0;
  IF_ERROR_LOG_AND_RETURN(readSize(file, count, sizeLeft));
  while (count-- > 0) {
    string name;
    IF_ERROR_LOG_AND_RETURN(readString(file, name, sizeLeft));
    string value;
    IF_ERROR_LOG_AND_RETURN(readString(file, value, sizeLeft));
    outMap[name] = value;
  }
  return 0;
}

static int readMap(FileHandler& file, map<StreamId, string>& outMap, uint32_t& sizeLeft) {
  uint32_t count = 0;
  IF_ERROR_LOG_AND_RETURN(readSize(file, count, sizeLeft));
  while (count-- > 0) {
    IndexRecord::DiskStreamId id;
    if (file.read(id) != 0) {
      return file.getLastError();
    }
    sizeLeft -= sizeof(IndexRecord::DiskStreamId);
    IF_ERROR_LOG_AND_RETURN(readString(file, outMap[id.getStreamId()], sizeLeft));
  }
  return 0;
}

static size_t mapSize(const map<StreamId, const StreamTags*>& map) {
  size_t size = sizeof(uint32_t);
  for (const auto& pair : map) {
    size +=
        sizeof(IndexRecord::DiskStreamId) + mapSize(pair.second->user) + mapSize(pair.second->vrs);
  }
  return size;
}

static int writeMap(WriteFileHandler& file, const map<StreamId, const StreamTags*>& map) {
  IF_ERROR_LOG_AND_RETURN(writeSize(file, map.size()));
  for (const auto& pair : map) {
    IndexRecord::DiskStreamId id(pair.first);
    WRITE_OR_LOG_AND_RETURN(file, &id, sizeof(id));
    IF_ERROR_LOG_AND_RETURN(writeMap(file, pair.second->user));
    IF_ERROR_LOG_AND_RETURN(writeMap(file, pair.second->vrs));
  }
  return 0;
}

static int readMap(FileHandler& file, map<StreamId, StreamTags>& outMap, uint32_t& sizeLeft) {
  uint32_t count = 0;
  IF_ERROR_LOG_AND_RETURN(readSize(file, count, sizeLeft));
  while (count-- > 0) {
    IndexRecord::DiskStreamId id;
    if (file.read(id) != 0) {
      return file.getLastError();
    }
    sizeLeft -= sizeof(IndexRecord::DiskStreamId);
    StreamTags& tags = outMap[id.getStreamId()];
    IF_ERROR_LOG_AND_RETURN(readMap(file, tags.user, sizeLeft));
    IF_ERROR_LOG_AND_RETURN(readMap(file, tags.vrs, sizeLeft));
  }
  return 0;
}

int DescriptionRecord::writeDescriptionRecord(
    WriteFileHandler& file,
    const map<StreamId, const StreamTags*>& streamTags,
    const map<string, string>& fileTags,
    uint32_t& outPreviousRecordSize) {
  // Build & write VRS description record header
  FileFormat::RecordHeader descriptionRecordHeader;
  uint32_t recordSize = static_cast<uint32_t>(
      sizeof(descriptionRecordHeader) + mapSize(streamTags) + mapSize(fileTags));
  descriptionRecordHeader.initDescriptionHeader(
      kDescriptionFormatVersion, recordSize, outPreviousRecordSize);
  WRITE_OR_LOG_AND_RETURN(file, &descriptionRecordHeader, sizeof(descriptionRecordHeader));
  // Write actual description record data
  IF_ERROR_LOG_AND_RETURN(writeMap(file, streamTags));
  IF_ERROR_LOG_AND_RETURN(writeMap(file, fileTags));
  outPreviousRecordSize = recordSize;
  return 0;
}

int DescriptionRecord::readDescriptionRecord(
    FileHandler& file,
    uint32_t recordHeaderSize,
    uint32_t& outDescriptionRecordSize,
    map<StreamId, StreamTags>& outStreamTags,
    map<string, string>& outFileTags) {
  outStreamTags.clear();
  outFileTags.clear();
  // maybe headers are larger now: allocate a possibly larger buffer than FileFormat::RecordHeader
  vector<uint8_t> headerBuffer(recordHeaderSize);
  FileFormat::RecordHeader* recordHeader =
      reinterpret_cast<FileFormat::RecordHeader*>(headerBuffer.data());
  if (file.read(recordHeader, recordHeaderSize) != 0) {
    XR_LOGD(
        "Can't read description header. Read {} bytes, expected {} bytes.",
        file.getLastRWSize(),
        recordHeaderSize);
    return file.getLastError();
  }
  outDescriptionRecordSize = recordHeader->recordSize.get();
  if (outDescriptionRecordSize < recordHeaderSize + sizeof(uint32_t)) {
    XR_LOGD("Record size too small. Corrupt?");
    outDescriptionRecordSize = 0;
    return NOT_ENOUGH_DATA;
  }
  uint32_t dataSizeLeft = outDescriptionRecordSize - static_cast<uint32_t>(recordHeaderSize);
  switch (recordHeader->formatVersion.get()) {
    case kLegacyDescriptionFormatVersion: {
      map<StreamId, string> descriptions;
      IF_ERROR_LOG_AND_RETURN(readMap(file, descriptions, dataSizeLeft));
      for (const auto& description : descriptions) {
        string originalName;
        StreamTags& tags = outStreamTags[description.first];
        jsonToNameAndTags(description.second, originalName, tags.user);
        tags.vrs[Recordable::getOriginalNameTagName()] = stripInstanceId(originalName);
      }
      string jsonTags;
      IF_ERROR_LOG_AND_RETURN(readString(file, jsonTags, dataSizeLeft));
      if (dataSizeLeft != 0) {
        XR_LOGD("Description record bug: {} bytes left.", dataSizeLeft);
      }
      jsonToTags(jsonTags, outFileTags);
      createStreamSerialNumbers(outFileTags, outStreamTags);
      return 0;
    }

    case kDescriptionFormatVersion: {
      IF_ERROR_LOG_AND_RETURN(readMap(file, outStreamTags, dataSizeLeft));
      for (auto& streamTags : outStreamTags) {
        upgradeStreamTags(streamTags.second.vrs);
      }
      IF_ERROR_LOG_AND_RETURN(readMap(file, outFileTags, dataSizeLeft));
      createStreamSerialNumbers(outFileTags, outStreamTags);
      return 0;
    }

    default:
      XR_LOGD("Unsupported description record format.");
      return UNSUPPORTED_DESCRIPTION_FORMAT_VERSION;
  }
}

void DescriptionRecord::upgradeStreamTags(map<string, string>& vrsTags) {
  // strip instance ID of original device names
  auto iter = vrsTags.find(Recordable::getOriginalNameTagName());
  if (iter != vrsTags.end()) {
    iter->second = stripInstanceId(iter->second);
  }
}

static void
limitedIngest(XXH64Digester& digester, const map<string, string>& data, size_t maxLength) {
  const char* kSignature = "map<string, string>";
  digester.ingest(kSignature, strlen(kSignature));
  for (const auto& iter : data) {
    digester.ingest(iter.first);
    // some tags are gigantic (hundreds of KB), we don't need to process them entirely
    digester.ingest(iter.second.c_str(), min<size_t>(iter.second.size() + 1, maxLength));
  }
}

void DescriptionRecord::createStreamSerialNumbers(
    const map<string, string>& inFileTags,
    map<StreamId, StreamTags>& inOutStreamTags) {
  string fileTagsHash;
  const string& cSerialNumberTagName = Recordable::getSerialNumberTagName();
  map<RecordableTypeId, uint16_t> streamCounters;
  for (auto& streamTags : inOutStreamTags) {
    string& streamSerialNumber = streamTags.second.vrs[cSerialNumberTagName];
    if (streamSerialNumber.empty()) {
      // Some user tags are massive (hundreds of KB), let's limit how much data we hash
      const size_t kMaxLengthUserTags = 2000;
      if (fileTagsHash.empty()) {
        XXH64Digester digester;
        limitedIngest(digester, inFileTags, kMaxLengthUserTags);
        fileTagsHash = digester.digestToString();
      }
      XXH64Digester digester;
      digester.ingest(fileTagsHash);
      limitedIngest(digester, streamTags.second.user, kMaxLengthUserTags);
      // Hash whole VRS internal tags to capture any DataLayout definition difference
      digester.ingest(streamTags.second.vrs);
      StreamId id(streamTags.first.getTypeId(), ++streamCounters[streamTags.first.getTypeId()]);
      digester.ingest(&id, sizeof(id));
      streamSerialNumber = digester.digestToString();
    }
  }
}

} // namespace vrs
