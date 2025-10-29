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

#include <vrs/utils/Validation.h>

#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#define DEFAULT_LOG_CHANNEL "Validation"
#include <logging/Log.h>
#include <logging/Verify.h>

#include <vrs/helpers/Rapidjson.hpp>
#include <vrs/helpers/Strings.h>
#include <vrs/helpers/Throttler.h>
#include <vrs/os/Time.h>
#include <vrs/utils/FilterCopy.h>
#include <vrs/utils/PixelFrame.h>
#include <vrs/utils/VideoRecordFormatStreamPlayer.h>
#include <vrs/utils/xxhash/xxhash.h>

using namespace std;
using namespace vrs;

namespace {

using namespace vrs::utils;

Throttler& getThrottler() {
  static Throttler sThrottler;
  return sThrottler;
}

#pragma pack(push, 1)
struct PackedHeader {
  PackedHeader(const CurrentRecord& record, StreamId _sanitizedId)
      : timestamp{record.timestamp},
        formatVersion{record.formatVersion},
        recordSize{record.recordSize},
        typeId{_sanitizedId.getTypeId()},
        instanceId{_sanitizedId.getInstanceId()},
        recordType{record.recordType} {}
  double timestamp;
  uint32_t formatVersion;
  uint32_t recordSize;
  RecordableTypeId typeId;
  uint16_t instanceId;
  Record::Type recordType;
};
#pragma pack(pop)

inline string checksum(const stringstream& ss) {
  string tagsString = ss.str();
  XXH64Digester digester;
  digester.ingest(tagsString.c_str(), tagsString.size());
  return digester.digestToString();
}

string checksum(const map<string, string>& tags) {
  stringstream ss;
  for (const auto& tag : tags) {
    ss << tag.first << '=' << tag.second << '/';
  }
  return checksum(ss);
}

void hexdump(const vector<char>& buffer, int lineLength, bool printAscii = false) {
  const unsigned char* buf = reinterpret_cast<const unsigned char*>(buffer.data());
  const int bufLength = static_cast<int>(buffer.size());
  for (int i = 0; i < bufLength; i += lineLength) {
    printf("%06x: ", i);
    for (int j = 0; j < lineLength; j++) {
      bool space = lineLength < 24 && !printAscii;
      if (i + j < bufLength) {
        printf(space ? "%02x " : "%02x", buf[i + j]);
      } else {
        printf("   ");
      }
    }
    if (printAscii) {
      printf(" ");
      for (int j = 0; j < lineLength; j++) {
        if (i + j < bufLength) {
          printf("%c", isprint(buf[i + j]) ? buf[i + j] : '.');
        }
      }
    }
    printf("\n");
  }
}

class RecordChecker : public StreamPlayer {
 public:
  RecordChecker(StreamId id, uint16_t instance, CheckType checkType)
      : id_{id}, sanitizedId_{id.getTypeId(), instance}, checkType_{checkType} {}
  bool processRecordHeader(const CurrentRecord& record, DataReference& outDataReference) override {
    buffer_.resize(record.recordSize);
    outDataReference.useVector(buffer_);
    return true;
  }
  void processRecord(const CurrentRecord& record, uint32_t /*readSize*/) override {
    switch (checkType_) {
      case CheckType::Checksum:
      case CheckType::Checksums:
      case CheckType::HexDump: {
        PackedHeader packedHeader(record, sanitizedId_);
        headerChecksum_.ingest(&packedHeader, sizeof(packedHeader));
        payloadChecksum_.ingest(buffer_);
        if (checkType_ == CheckType::HexDump) {
          XXH64Digester csDigester_;
          csDigester_.ingest(buffer_);
          cout << sanitizedId_.getNumericName() << ": " << fixed << setprecision(3)
               << record.timestamp << " " << toString(record.recordType) << " s=" << buffer_.size()
               << " CS=" << csDigester_.digestToString() << "\n";
          hexdump(buffer_, 32);
        }
      } break;
      case CheckType::ChecksumVerbatim: // not handled here
      case CheckType::Decode: // not handled here
      case CheckType::None:
      case CheckType::Check:
      case CheckType::COUNT:
          /* do nothing */;
        break;
    }
  }
  StreamId getId() const {
    return id_;
  }
  StreamId getSanitizedId() const {
    return sanitizedId_;
  }
  string digestHeaderChecksum() {
    return headerChecksum_.digestToString();
  }
  string digestPayloadChecksum() {
    return payloadChecksum_.digestToString();
  }

 private:
  XXH64Digester headerChecksum_;
  XXH64Digester payloadChecksum_;
  vector<char> buffer_;
  StreamId id_;
  StreamId sanitizedId_;
  CheckType checkType_;
};

bool iterateChecker(
    FilteredFileReader& reader,
    size_t& outDecodedRecords,
    bool& outNoError,
    ThrottledWriter* throttledWriter,
    double& outDuration,
    double& outCpuTime) {
  outDecodedRecords = 0;
  outNoError = true;
  if (!reader.timeRangeValid()) {
    cerr << "Time Range invalid: " << reader.getTimeConstraintDescription() << "\n";
    outDuration = 0;
    return false;
  }
  double beforeTime = os::getTimestampSec();
  double beforeCpu = os::getTotalProcessCpuTime();
  reader.iterateAdvanced(
      [&outDecodedRecords, &outNoError](
          RecordFileReader& recordFileReader, const IndexRecord::RecordInfo& record) {
        outNoError &= (recordFileReader.readRecord(record) == 0);
        outDecodedRecords++;
        return outNoError;
      },
      throttledWriter);
  reader.reader.clearStreamPlayers();
  outDuration = os::getTimestampSec() - beforeTime;
  outCpuTime = os::getTotalProcessCpuTime() - beforeCpu;
  return true;
}

string asJson(
    bool success,
    uint64_t recordCount,
    double duration,
    double mbPerSecond,
    uint64_t decodedCount,
    double percent) {
  using namespace vrs_rapidjson;
  JDocument document;
  JsonWrapper wrapper{document};
  wrapper.addMember("good_file", success);
  wrapper.addMember("record_count", recordCount);
  wrapper.addMember("duration", duration);
  wrapper.addMember("mb_per_sec", mbPerSecond);
  if (decodedCount < recordCount) {
    wrapper.addMember("decoded_count", decodedCount);
    wrapper.addMember("good_percent", percent);
  }
  return jDocumentToJsonString(document);
}

string mapAsJson(const map<string, string>& strMap) {
  using namespace vrs_rapidjson;
  JDocument document;
  JsonWrapper jw{document};
  for (const auto& element : strMap) {
    document.AddMember(jw.jValue(element.first), jw.jValue(element.second), jw.alloc);
  }
  return jDocumentToJsonString(document);
}

class DecodeChecker : public VideoRecordFormatStreamPlayer {
 public:
  DecodeChecker(uint32_t& errorCount, uint32_t& imageCount)
      : errorCount_{errorCount}, imageCount_{imageCount} {}
  bool processRecordHeader(const CurrentRecord& record, DataReference& outDataReference) override {
    if (VideoRecordFormatStreamPlayer::processRecordHeader(record, outDataReference)) {
      return true;
    }
    if (record.recordSize > 0) {
      errorCount_++;
    }
    return false;
  }
  void processRecord(const CurrentRecord& record, uint32_t readSize) override {
    processSuccess = true;
    RecordFormatStreamPlayer::processRecord(record, readSize);
    if (!processSuccess) {
      THROTTLED_LOGW(
          record.fileReader,
          "{} - {} record #{} could not be decoded.",
          record.streamId.getNumericName(),
          Record::typeName(record.recordType),
          record.fileReader->getRecordIndex(record.recordInfo));
    } else if (record.reader->getUnreadBytes() > 0) {
      processSuccess = false;
      THROTTLED_LOGW(
          record.fileReader,
          "{} - {} record #{}: {} bytes unread out of {} bytes.",
          record.streamId.getNumericName(),
          Record::typeName(record.recordType),
          record.fileReader->getRecordIndex(record.recordInfo),
          record.reader->getUnreadBytes(),
          record.recordSize);
    }
    if (!processSuccess) {
      errorCount_++;
    }
  }
  bool onDataLayoutRead(const CurrentRecord&, size_t /*blkIdx*/, DataLayout&) override {
    return true;
  }
  bool onImageRead(const CurrentRecord& record, size_t /*blkIdx*/, const ContentBlock& cb)
      override {
    PixelFrame frame;
    return isSuccess(readFrame(frame, record, cb), cb.getContentType());
  }
  bool onAudioRead(const CurrentRecord& record, size_t blockIndex, const ContentBlock& cb)
      override {
    return onUnsupportedBlock(record, blockIndex, cb);
  }
  bool onCustomBlockRead(const CurrentRecord& rec, size_t blkIdx, const ContentBlock& cb) override {
    return onUnsupportedBlock(rec, blkIdx, cb);
  }
  bool onUnsupportedBlock(const CurrentRecord& rec, size_t /*blkIdx*/, const ContentBlock& cb)
      override {
    size_t blockSize = cb.getBlockSize();
    if (blockSize == ContentBlock::kSizeUnknown) {
      THROTTLED_LOGW(rec.fileReader, "Block size for {} unknown.", cb.asString());
      return isSuccess(false);
    }
    vector<char> data(blockSize);
    return isSuccess(rec.reader->read(data) == 0);
  }
  bool isSuccess(bool success, ContentType contentType = ContentType::EMPTY) {
    if (!success) {
      processSuccess = false;
    } else if (contentType == ContentType::IMAGE) {
      imageCount_++;
    }
    return success;
  }

 private:
  uint32_t& errorCount_;
  uint32_t& imageCount_;
  bool processSuccess{false};
};

string decodeValidation(FilteredFileReader& filteredReader, const CopyOptions& copyOptions) {
  uint32_t decodeErrorCount{0};
  uint32_t imageCount{0};
  filteredReader.reader.clearStreamPlayers();
  vector<unique_ptr<DecodeChecker>> checkers;
  for (auto id : filteredReader.filter.streams) {
    checkers.emplace_back(make_unique<DecodeChecker>(decodeErrorCount, imageCount));
    filteredReader.reader.setStreamPlayer(id, checkers.back().get());
  }

  double startTimestamp{}, endTimestamp{};
  filteredReader.getConstrainedTimeRange(startTimestamp, endTimestamp);
  filteredReader.preRollConfigAndState(); // make sure to copy most recent config & state records

  ThrottledWriter throttledWriter(copyOptions);
  throttledWriter.initTimeRange(startTimestamp, endTimestamp, &filteredReader.reader);

  size_t readRecordCount = 0;
  bool noError = true;
  double timeSpent = 0;
  double cpuTime = 0;
  if (!iterateChecker(
          filteredReader, readRecordCount, noError, &throttledWriter, timeSpent, cpuTime)) {
    return "<invalid timerange>";
  }
  throttledWriter.closeFile();

  size_t decodedCount =
      readRecordCount >= decodeErrorCount ? readRecordCount - decodeErrorCount : 0;
  double successRate = decodedCount * 100. / readRecordCount;
  if (copyOptions.jsonOutput) {
    double duration = endTimestamp - startTimestamp;
    double mbPerSec =
        duration > 0 ? filteredReader.reader.getTotalSourceSize() / (duration * 1024 * 1024) : -1;
    return asJson(noError, readRecordCount, duration, mbPerSec, decodedCount, successRate);
  }
  if (noError && decodeErrorCount == 0) {
    return fmt::format(
        "Decoded {} records, {} images, in {} wall clock time and {} CPU time, no errors.",
        decodedCount,
        imageCount,
        helpers::humanReadableDuration(timeSpent),
        helpers::humanReadableDuration(cpuTime));
  }
  return fmt::format(
      "Failure! Decoded {} records out of {}, {:.2f}% good.",
      decodedCount,
      readRecordCount,
      successRate);
}

} // namespace

namespace vrs::utils {

string checkRecords(
    FilteredFileReader& filteredReader,
    const CopyOptions& copyOptions,
    CheckType checkType) {
  if (!filteredReader.reader.isOpened()) {
    return {};
  }
  filteredReader.reader.clearStreamPlayers();
  if (checkType == CheckType::Decode) {
    return decodeValidation(filteredReader, copyOptions);
  }
  vector<unique_ptr<RecordChecker>> checkers;
  // Instance ids are assigned by VRS and can not be relied upon, though ordering is
  // guaranteed. We need to sanitize our instance ids to achieve an equivalence that will not
  // depend on the actual instance id values, but only on the order of the instances.
  map<RecordableTypeId, uint16_t> instanceIds;
  for (auto id : filteredReader.filter.streams) {
    checkers.emplace_back(make_unique<RecordChecker>(id, ++instanceIds[id.getTypeId()], checkType));
    filteredReader.reader.setStreamPlayer(id, checkers.back().get());
  }

  double startTimestamp{}, endTimestamp{};
  filteredReader.getConstrainedTimeRange(startTimestamp, endTimestamp);
  filteredReader.preRollConfigAndState(); // make sure to copy most recent config & state records

  ThrottledWriter throttledWriter(copyOptions);
  throttledWriter.initTimeRange(startTimestamp, endTimestamp, &filteredReader.reader);

  size_t decodedCount = 0;
  bool noError = true;
  double timeSpent = 0;
  double cpuTime = 0;
  if (!iterateChecker(
          filteredReader, decodedCount, noError, &throttledWriter, timeSpent, cpuTime)) {
    return "<invalid timerange>";
  }
  throttledWriter.closeFile();

  if (noError && checkType != CheckType::Check) {
    stringstream ss;
    map<string, string> checksums;
    // calculate xxhash for each component, then calculate xxhash for all the hashes.
    XXH64Digester sum;

    string fileTagsChecksum = checksum(filteredReader.reader.getTags());

    sum.ingest(fileTagsChecksum);
    if (checkType == CheckType::Checksums) {
      if (copyOptions.jsonOutput) {
        checksums["filetags"] = fileTagsChecksum;
      } else {
        ss << "FileTags: " << fileTagsChecksum << "\n";
      }
    }
    stringstream ids;
    for (auto& checker : checkers) {
      StreamId id = checker->getId();
      const StreamTags& tags = filteredReader.reader.getTags(id);
      switch (checkType) {
        case CheckType::Checksum:
          sum.ingest(checksum(tags.vrs))
              .ingest(checksum(tags.user))
              .ingest(checker->digestHeaderChecksum())
              .ingest(checker->digestPayloadChecksum());
          break;
        case CheckType::Checksums: {
          string vrsTagsChecksum = checksum(tags.vrs);
          string userTagsChecksum = checksum(tags.user);
          string headerChecksum = checker->digestHeaderChecksum();
          string payloadChecksum = checker->digestPayloadChecksum();
          if (copyOptions.jsonOutput) {
            checksums[id.getNumericName() + "_vrstags"] = vrsTagsChecksum;
            checksums[id.getNumericName() + "_usertags"] = userTagsChecksum;
            checksums[id.getNumericName() + "_headers"] = headerChecksum;
            checksums[id.getNumericName() + "_payload"] = payloadChecksum;
          } else {
            ss << id.getNumericName() << " VRS tags: " << vrsTagsChecksum << "\n";
            ss << id.getNumericName() << " User tags: " << userTagsChecksum << "\n";
            ss << id.getNumericName() << " Headers: " << headerChecksum << "\n";
            ss << id.getNumericName() << " Payload: " << payloadChecksum << "\n";
          }
          sum.ingest(vrsTagsChecksum)
              .ingest(userTagsChecksum)
              .ingest(headerChecksum)
              .ingest(payloadChecksum);
        } break;
        case CheckType::ChecksumVerbatim:
        case CheckType::HexDump:
        case CheckType::Decode:
        case CheckType::None:
        case CheckType::Check:
        case CheckType::COUNT:
            /* do nothing */;
      }
      ids << checker->getSanitizedId().getNumericName() << '/';
    }
    sum.ingest(checksum(ids));
    if (copyOptions.jsonOutput) {
      checksums["checksum"] = sum.digestToString();
      return mapAsJson(checksums);
    }
    ss << sum.digestToString();
    return ss.str();
  }
  size_t recordCount = filteredReader.reader.getIndex().size();
  double successRate = decodedCount * 100. / recordCount;
  if (copyOptions.jsonOutput) {
    double duration = endTimestamp - startTimestamp;
    double mbPerSec =
        duration > 0 ? filteredReader.reader.getTotalSourceSize() / (duration * 1024 * 1024) : -1;
    return asJson(noError, recordCount, duration, mbPerSec, decodedCount, successRate);
  }
  if (noError) {
    return fmt::format(
        "Checked {} records in {}, no errors.",
        decodedCount,
        helpers::humanReadableDuration(timeSpent));
  }
  return fmt::format(
      "Failure! Checked {} records out of {}, {:.2f}% good.",
      decodedCount,
      recordCount,
      successRate);
}

string recordsChecksum(const string& path, bool showProgress) {
  FilteredFileReader reader(path);
  int status = reader.openFile();
  if (status != 0) {
    return "Error " + to_string(status) + ": " + errorCodeToMessage(status);
  }
  return checkRecords(reader, CopyOptions(showProgress), CheckType::Checksum);
}

string verbatimChecksum(const string& path, bool showProgress) {
  const char* kStatus = "Calculating ";
  const char* kReset = showProgress ? kResetCurrentLine : "";
  unique_ptr<FileHandler> file = FileHandler::makeOpen(path);
  if (!file) {
    return "<file open error>";
  }
  XXH64Digester digester;
  size_t totalSize = static_cast<size_t>(file->getTotalSize());
  vector<char> buffer;
  for (size_t offset = 0; offset < totalSize; offset += kDownloadChunkSize) {
    size_t length = min<size_t>(kDownloadChunkSize, totalSize - offset);
    printProgress(kStatus, offset + length / 4, totalSize, showProgress);
    buffer.resize(length);
    int error = file->read(buffer.data(), length);
    if (error == 0) {
      digester.ingest(buffer);
    } else {
      cerr << kReset << "Read file error: " << errorCodeToMessage(error) << ".\n";
      return "<read error>";
    }
  }
  cout << kReset;
  return digester.digestToString();
}

static void buildIdMap(
    FilteredFileReader& filteredReader,
    map<StreamId, StreamId>& idMap,
    bool idToNormalizedId) {
  map<RecordableTypeId, uint16_t> instanceIds;
  for (auto id : filteredReader.filter.streams) {
    StreamId normalizedId{id.getTypeId(), ++instanceIds[id.getTypeId()]};
    if (idToNormalizedId) {
      idMap[id] = normalizedId;
    } else {
      idMap[normalizedId] = id;
    }
  }
}

static bool
buildIdMap(FilteredFileReader& first, FilteredFileReader& second, map<StreamId, StreamId>& idMap) {
  map<StreamId, StreamId> firstToNormalizedMap, normalizedToSecondMap;
  buildIdMap(first, firstToNormalizedMap, true); // stream id -> normalized id
  buildIdMap(second, normalizedToSecondMap, false); // normalized id -> stream id
  idMap.clear();
  while (!firstToNormalizedMap.empty()) {
    auto iter = firstToNormalizedMap.begin();
    auto res = normalizedToSecondMap.find(iter->second);
    if (res == normalizedToSecondMap.end()) {
      return false; // stream in first file has no equivalent in second file: fail!
    }
    idMap[iter->first] = res->second;
    firstToNormalizedMap.erase(iter);
    normalizedToSecondMap.erase(res);
  }
  // if normalizedToSecond is not empty, a stream in the second file has no equivalent in the first
  // one, and the files aren't equivalent.
  return normalizedToSecondMap.empty();
}

static bool
isSameLine(const vector<char>& first, const vector<char>& second, size_t offset, size_t lineSize) {
  if (offset + lineSize < first.size()) {
    size_t endOffset = offset + lineSize;
    for (size_t idx = offset; idx < endOffset; idx++) {
      if (first[idx] != second[idx]) {
        return false;
      }
    }
  } else {
    const int64_t* firstPtr = reinterpret_cast<const int64_t*>(first.data() + offset);
    const int64_t* secondPtr = reinterpret_cast<const int64_t*>(second.data() + offset);
    const int64_t* endPtr = firstPtr + lineSize / sizeof(int64_t);
    while (firstPtr < endPtr) {
      if (*firstPtr != *secondPtr) {
        return false;
      }
      ++firstPtr;
      ++secondPtr;
    }
  }
  return true;
}
static void printLine(const vector<char>& v, size_t offset, size_t lineSize, bool printAscii) {
  size_t maxOffset = min<size_t>(offset + lineSize, v.size());
  const unsigned char* buf = reinterpret_cast<const unsigned char*>(v.data());
  printf("%08x: ", static_cast<unsigned int>(offset));
  for (size_t k = offset; k < maxOffset; k++) {
    printf("%02x", buf[k]);
    if ((k + 1) % 8 == 0) {
      printf(" ");
    }
  }
  if (printAscii) {
    printf(" ");
    for (size_t k = offset; k < maxOffset; k++) {
      printf("%c", isprint(buf[k]) ? buf[k] : '.');
    }
  }
  printf("\n");
}

static void printDifferences(
    const vector<char>& buffer,
    const vector<char>& otherBuffer,
    size_t lineSize,
    bool printAscii) {
  size_t byteDiffs = 0;
  size_t bitDiffs = 0;
  for (size_t k = 0; k < buffer.size(); k++) {
    if (buffer[k] != otherBuffer[k]) {
      byteDiffs++;
      char diff = buffer[k] ^ otherBuffer[k]; // get different bits
      // count bits set using Brian Kernighan’s Algorithm
      while (diff) {
        diff &= diff - 1;
        bitDiffs++;
      }
    }
  }
  // if there are too many differences, don't bother printing the details...
  if (byteDiffs > 500) {
    cout << "Too many differences to print: " << byteDiffs << " bytes differ out of "
         << buffer.size() << " total (" << 100 * byteDiffs / buffer.size() << "%).\n";
    return;
  }
  cout << byteDiffs << " bytes and " << bitDiffs << " bits differ.\n";

  size_t offset = 0;
  while (offset < buffer.size()) {
    // skip identical "lines"
    while (offset < buffer.size() && isSameLine(buffer, otherBuffer, offset, lineSize)) {
      offset += lineSize;
    }
    if (offset < buffer.size()) {
      uint32_t diffCount = 1;
      while (offset + diffCount * lineSize < buffer.size() &&
             !isSameLine(buffer, otherBuffer, offset + diffCount * lineSize, lineSize)) {
        diffCount++;
      }
      for (uint32_t k = 0; k < diffCount; k++) {
        printf("< ");
        printLine(buffer, offset + k * lineSize, lineSize, printAscii);
      }
      printf("----\n");
      for (uint32_t k = 0; k < diffCount; k++) {
        printf("> ");
        printLine(otherBuffer, offset + k * lineSize, lineSize, printAscii);
      }
      offset += diffCount * lineSize;
    }
  }
  cout << "\n";
}

namespace {

class RecordHolder : public StreamPlayer {
 public:
  bool processRecordHeader(const CurrentRecord& record, DataReference& outDataReference) override {
    buffer_.resize(record.recordSize);
    outDataReference.useVector(buffer_);
    return true;
  }
  void processRecord(const CurrentRecord& /*record*/, uint32_t /*readSize*/) override {
    readCounter_++;
  }
  const vector<char>& getBuffer() const {
    return buffer_;
  }
  size_t getReadCounter() const {
    return readCounter_;
  }

 private:
  size_t readCounter_ = 0;
  vector<char> buffer_;
};

class RecordMaster : public StreamPlayer {
 public:
  RecordMaster(
      atomic<int>& diffCounter,
      bool& noError,
      StreamId matchingId,
      RecordFileReader& matchingReader,
      RecordHolder* holder)
      : diffCounter_{diffCounter},
        noError_{noError},
        id_{matchingId},
        reader_{matchingReader},
        holder_{holder},
        index_{reader_.getIndex(id_)} {}
  bool processRecordHeader(const CurrentRecord& record, DataReference& outDataReference) override {
    buffer_.resize(record.recordSize);
    outDataReference.useVector(buffer_);
    return true;
  }
  void processRecord(const CurrentRecord& record, uint32_t /*readSize*/) override {
    readCounter_++;
    while (lastRecord_ < index_.size() && index_[lastRecord_]->timestamp < record.timestamp) {
      lastRecord_++;
    }
    if (lastRecord_ >= index_.size() || index_[lastRecord_]->timestamp > record.timestamp) {
      diffCounter_++;
      return;
    }
    // we have the first record with the same timestamp, but is the type the right one?
    size_t typeIndex = lastRecord_;
    while (typeIndex < index_.size() && index_[typeIndex]->timestamp <= record.timestamp &&
           index_[typeIndex]->recordType != record.recordType) {
      typeIndex++;
    }
    if (typeIndex < index_.size() && index_[typeIndex]->timestamp <= record.timestamp &&
        index_[typeIndex]->recordType == record.recordType) {
      // found a match, read that record & compare the buffers
      if (reader_.readRecord(*index_[typeIndex]) != 0) {
        noError_ = false;
        cerr << "Record " << record.streamId.getNumericName() << " t: " << fixed << setprecision(3)
             << record.timestamp << " " << toString(record.recordType)
             << " Error while reading the record.\n";
      } else {
        if (holder_->getReadCounter() != readCounter_) {
          cerr << "Record counter is different.\n";
          diffCounter_++;
        } else if (holder_->getBuffer() != buffer_) {
          diffCounter_++;
          cerr << "Record " << record.streamId.getNumericName() << " t: " << fixed
               << setprecision(3) << record.timestamp << " " << toString(record.recordType)
               << " payload mismatch.\n";
          const vector<char>& otherBuffer = holder_->getBuffer();
          if (otherBuffer.size() != buffer_.size()) {
            cerr << "Payload sizes differ: " << buffer_.size() << " vs. " << otherBuffer.size()
                 << ".\n";
          } else {
            const size_t kLineSize = 8; // How many bytes per line
            const bool kPrintAscii = false;
            printDifferences(buffer_, otherBuffer, kLineSize, kPrintAscii);
          }
        }
      }
    }
  }

 private:
  atomic<int>& diffCounter_;
  bool& noError_;
  StreamId id_;
  RecordFileReader& reader_;
  RecordHolder* holder_;
  const vector<const IndexRecord::RecordInfo*>& index_;
  size_t lastRecord_{};
  size_t readCounter_{};
  vector<char> buffer_;
};

} // namespace

bool compareVRSfiles(
    FilteredFileReader& first,
    FilteredFileReader& second,
    const CopyOptions& copyOptions) {
  double startTimestamp{}, endTimestamp{};
  first.getConstrainedTimeRange(startTimestamp, endTimestamp);
  first.preRollConfigAndState(); // make sure to copy most recent config & state records

  ThrottledWriter throttledWriter(copyOptions);
  throttledWriter.initTimeRange(startTimestamp, endTimestamp, &first.reader);

  map<StreamId, StreamId> idMap;
  if (!buildIdMap(first, second, idMap)) {
    cerr << "Streams don't match.\n";
    return false;
  }
  bool match = true;
  if (first.reader.getTags() != second.reader.getTags()) {
    cerr << "File tags don't match.\n";
    match = false;
  }
  for (auto ids : idMap) {
    const StreamTags& firstTags = first.reader.getTags(ids.first);
    const StreamTags& secondTags = second.reader.getTags(ids.second);
    if (firstTags.vrs != secondTags.vrs) {
      cerr << "The VRS tags of the stream " << ids.first.getNumericName() << '/'
           << ids.second.getNumericName() << " don't match.\n";
      match = false;
    }
    if (firstTags.user != secondTags.user) {
      cerr << "The user tags of the stream " << ids.first.getNumericName() << '/'
           << ids.second.getNumericName() << " don't match.\n";
      match = false;
    }
  }
  if (!match) {
    return false;
  }
  first.reader.clearStreamPlayers();
  second.reader.clearStreamPlayers();
  atomic<int> diffCounter{0};
  bool noError = true;
  vector<unique_ptr<RecordHolder>> holders;
  vector<unique_ptr<RecordMaster>> checkers;
  for (const auto& iter : idMap) {
    holders.emplace_back(make_unique<RecordHolder>());
    second.reader.setStreamPlayer(iter.second, holders.back().get());
    checkers.emplace_back(
        make_unique<RecordMaster>(
            diffCounter, noError, iter.second, second.reader, holders.back().get()));
    first.reader.setStreamPlayer(iter.first, checkers.back().get());
  }
  size_t decodedCount = 0;
  double timeSpent = 0;
  double cpuTime = 0;
  if (!iterateChecker(first, decodedCount, noError, &throttledWriter, timeSpent, cpuTime)) {
    return false;
  }
  if (!noError) {
    cerr << "Errors happened while reading the files\n";
  }
  throttledWriter.closeFile();
  return noError && diffCounter == 0;
}

bool compareVerbatim(const FileSpec& first, const FileSpec& second, bool showProgress) {
  const char* kStatus = "Comparing ";
  const char* kReset = showProgress ? kResetCurrentLine : "";
  unique_ptr<FileHandler> source = make_unique<DiskFile>();
  int status = source->openSpec(first);
  if (status != 0) {
    cerr << "Can't open source file to compare: " << errorCodeToMessage(status) << ".\n";
    return false;
  }
  unique_ptr<FileHandler> dest = make_unique<DiskFile>();
  if ((status = dest->openSpec(second)) != 0) {
    cerr << "Can't open second file to compare: " << errorCodeToMessage(status) << ".\n";
    return false;
  }
  if (source->getTotalSize() != dest->getTotalSize()) {
    cout << "The files have different sizes: " << source->getTotalSize() << " vs. "
         << dest->getTotalSize() << " bytes.\n";
    return false;
  }
  size_t totalSize = static_cast<size_t>(source->getTotalSize());
  vector<char> srcBuffer, dstBuffer;
  for (size_t offset = 0; offset < totalSize; offset += kDownloadChunkSize) {
    size_t length = min<size_t>(kDownloadChunkSize, totalSize - offset);
    printProgress(kStatus, offset + length / 4, totalSize, showProgress);
    srcBuffer.resize(length);
    int error = source->read(srcBuffer.data(), length);
    if (error == 0) {
      printProgress(kStatus, offset + 3 * length / 4, totalSize, showProgress);
      dstBuffer.resize(length);
      error = dest->read(dstBuffer.data(), length);
      if (error == 0) {
        if (srcBuffer != dstBuffer) {
          cout << kReset << "Chunk #" << (offset / kDownloadChunkSize) + 1 << " is different.\n";
          const size_t kLineSize = 16; // How many bytes per line
          const bool kPrintAscii = true;
          printDifferences(srcBuffer, dstBuffer, kLineSize, kPrintAscii);
          return false;
        }
      } else {
        cerr << kReset << "Read file error: " << errorCodeToMessage(error) << ".\n";
        return false;
      }
    } else {
      cerr << kReset << "Read file error: " << errorCodeToMessage(error) << ".\n";
      return false;
    }
  }
  cerr << kReset;
  return true;
}

} // namespace vrs::utils
