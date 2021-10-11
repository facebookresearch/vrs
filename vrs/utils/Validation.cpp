// Copyright (c) Facebook, Inc. and its affiliates. Confidential and proprietary.

#include "Validation.h"

#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#define DEFAULT_LOG_CHANNEL "Validation"
#include <logging/Log.h>
#include <logging/Verify.h>

#include <vrs/FileHandlerFactory.h>
#include <vrs/helpers/Rapidjson.hpp>
#include <vrs/utils/xxhash/xxhash.h>

#include "CopyHelpers.h"

using namespace std;
using namespace vrs;

namespace {

using namespace vrs::utils;

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

inline std::string checksum(const stringstream& ss) {
  string tagsString = ss.str();
  XXH64Digester digester;
  digester.update(tagsString.c_str(), tagsString.size());
  return digester.digestToString();
}

std::string checksum(const std::map<string, string>& tags) {
  stringstream ss;
  for (auto tag : tags) {
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
  virtual bool processRecordHeader(const CurrentRecord& record, DataReference& outDataReference) {
    buffer_.resize(record.recordSize);
    outDataReference.useVector(buffer_);
    return true;
  }
  virtual void processRecord(const CurrentRecord& record, uint32_t /*readSize*/) {
    switch (checkType_) {
      case CheckType::Checksum:
      case CheckType::Checksums:
      case CheckType::HexDump: {
        PackedHeader packedHeader(record, sanitizedId_);
        headerChecksum_.update(&packedHeader, sizeof(packedHeader));
        payloadChecksum_.update(buffer_);
        if (checkType_ == CheckType::HexDump) {
          XXH64Digester csDigester_;
          csDigester_.update(buffer_);
          cout << sanitizedId_.getNumericName() << ": " << fixed << setprecision(3)
               << record.timestamp << " " << toString(record.recordType) << " s=" << buffer_.size()
               << " CS=" << csDigester_.digestToString() << endl;
          hexdump(buffer_, 32);
        }
      } break;
      case CheckType::ChecksumVerbatim: // not handled here
      case CheckType::None:
      case CheckType::Count:
      case CheckType::Check:
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
    FilteredVRSFileReader& reader,
    size_t& outDecodedRecords,
    bool& outNoError,
    ThrottledWriter* throttledWriter) {
  outDecodedRecords = 0;
  outNoError = true;
  if (!reader.resolveTimeConstraints()) {
    cerr << "Time Range invalid: " << reader.getTimeConstraintDescription() << endl;
    return false;
  }
  reader.iterate(
      [&outDecodedRecords, &outNoError](
          RecordFileReader& recordFileReader, const IndexRecord::RecordInfo& record) {
        outNoError &= (recordFileReader.readRecord(record) == 0);
        outDecodedRecords++;
        return outNoError;
      },
      throttledWriter);
  for (auto id : reader.filter.streams) {
    reader.reader.setStreamPlayer(id, nullptr);
  }
  return true;
}

string asJson(
    bool success,
    uint64_t recordCount,
    double duration,
    double mbPerSecond,
    uint64_t decodedCount,
    double percent) {
  using namespace fb_rapidjson;
  JDocument document;
  document.SetObject();
  JsonWrapper wrapper{document, document.GetAllocator()};
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

} // namespace

namespace vrs::utils {

string checkRecords(
    FilteredVRSFileReader& filteredReader,
    const CopyOptions& copyOptions,
    CheckType checkType) {
  if (!filteredReader.reader.isOpened()) {
    return "";
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

  double startTimestamp, endTimestamp;
  filteredReader.getConstrainedTimeRange(startTimestamp, endTimestamp);
  filteredReader.preRollConfigAndState(); // make sure to copy most recent config & state records

  ThrottledWriter throttledWriter(copyOptions);
  throttledWriter.initTimeRange(startTimestamp, endTimestamp);

  size_t decodedCount;
  bool noError;
  if (!iterateChecker(filteredReader, decodedCount, noError, &throttledWriter)) {
    return "<invalid timerange>";
  }
  throttledWriter.closeFile();

  if (noError && checkType != CheckType::Check) {
    stringstream ss;
    // calculate xxhash for each componenet, then calculate xxhash for all the hashes.
    XXH64Digester sum;

    string fileTagsChecksum = checksum(filteredReader.reader.getTags());

    sum.update(fileTagsChecksum);
    if (checkType == CheckType::Checksums) {
      ss << "FileTags: " << fileTagsChecksum << endl;
    }
    stringstream ids;
    for (auto& checker : checkers) {
      StreamId id = checker->getId();
      const StreamTags& tags = filteredReader.reader.getTags(id);
      switch (checkType) {
        case CheckType::Checksum:
          sum.update(checksum(tags.vrs))
              .update(checksum(tags.user))
              .update(checker->digestHeaderChecksum())
              .update(checker->digestPayloadChecksum());
          break;
        case CheckType::Checksums: {
          string vrsTagsChecksum = checksum(tags.vrs);
          string userTagsChecksum = checksum(tags.user);
          string headerChecksum = checker->digestHeaderChecksum();
          string payloadChecksum = checker->digestPayloadChecksum();
          ss << id.getNumericName() << " VRS tags: " << vrsTagsChecksum << endl;
          ss << id.getNumericName() << " User tags: " << userTagsChecksum << endl;
          ss << id.getNumericName() << " Headers: " << headerChecksum << endl;
          ss << id.getNumericName() << " Payload: " << payloadChecksum << endl;
          sum.update(vrsTagsChecksum)
              .update(userTagsChecksum)
              .update(headerChecksum)
              .update(payloadChecksum);
        } break;
        case CheckType::ChecksumVerbatim:
        case CheckType::HexDump:
        case CheckType::None:
        case CheckType::Count:
        case CheckType::Check:
            /* do nothing */;
      }
      ids << checker->getSanitizedId().getNumericName() << '/';
    }
    sum.update(checksum(ids));
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
  stringstream ss;
  if (noError) {
    ss << "Decoded " << decodedCount << " records, no errors.";
  } else {
    ss << "Failure! Decoded " << decodedCount << " records out of " << recordCount << ", " << fixed
       << setprecision(2) << successRate << "% good.";
  }
  return ss.str();
}

string recordsChecksum(const string& path, bool showProgress) {
  FilteredVRSFileReader reader(path);
  int status = reader.openFile();
  if (status != 0) {
    return "Error " + to_string(status) + ": " + errorCodeToMessage(status);
  }
  return checkRecords(reader, CopyOptions(showProgress), CheckType::Checksum);
}

string verbatimChecksum(const string& path, bool showProgress) {
  const char* kStatus = "Calculating ";
  const char* kReset = showProgress ? kResetCurrentLine : "";
  unique_ptr<FileHandler> file;
  if (FileHandlerFactory::getInstance().delegateOpen(path, file) != 0) {
    return "<file open error>";
  }
  XXH64Digester digester;
  size_t totalSize = static_cast<size_t>(file->getTotalSize());
  vector<char> buffer;
  for (size_t offset = 0; offset < totalSize; offset += kDownloadChunkSize) {
    size_t length = std::min<size_t>(kDownloadChunkSize, totalSize - offset);
    printProgress(kStatus, offset + length / 4, totalSize, showProgress);
    buffer.resize(length);
    int error = file->read(buffer.data(), length);
    if (error == 0) {
      digester.update(buffer);
    } else {
      cerr << kReset << "Read file error: " << errorCodeToMessage(error) << "." << endl;
      return "<read error>";
    }
  }
  cout << kReset;
  return digester.digestToString();
}

static void buildIdMap(
    FilteredVRSFileReader& filteredReader,
    std::map<StreamId, StreamId>& idMap,
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

static bool buildIdMap(
    FilteredVRSFileReader& first,
    FilteredVRSFileReader& second,
    std::map<StreamId, StreamId>& idMap) {
  std::map<StreamId, StreamId> firstToNormalizedMap, normalizedToSecondMap;
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
         << buffer.size() << " total (" << 100 * byteDiffs / buffer.size() << "%)." << endl;
    return;
  }
  cout << byteDiffs << " bytes and " << bitDiffs << " bits differ." << endl;

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
  cout << endl;
}

class RecordHolder : public StreamPlayer {
 public:
  virtual bool processRecordHeader(const CurrentRecord& record, DataReference& outDataReference) {
    buffer_.resize(record.recordSize);
    outDataReference.useVector(buffer_);
    return true;
  }
  virtual void processRecord(const CurrentRecord& record, uint32_t /*readSize*/) {
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
        index_{reader_.getIndex(id_)},
        lastRecord_{0},
        readCounter_{0} {}
  virtual bool processRecordHeader(const CurrentRecord& record, DataReference& outDataReference) {
    buffer_.resize(record.recordSize);
    outDataReference.useVector(buffer_);
    return true;
  }
  virtual void processRecord(const CurrentRecord& record, uint32_t /*readSize*/) {
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
             << " Error while reading the record." << endl;
      } else {
        if (holder_->getReadCounter() != readCounter_) {
          cerr << "Record counter is different." << endl;
          diffCounter_++;
        } else if (holder_->getBuffer() != buffer_) {
          diffCounter_++;
          cerr << "Record " << record.streamId.getNumericName() << " t: " << fixed
               << setprecision(3) << record.timestamp << " " << toString(record.recordType)
               << " payload mismatch." << endl;
          const vector<char>& otherBuffer = holder_->getBuffer();
          if (otherBuffer.size() != buffer_.size()) {
            cerr << "Payload sizes differ: " << buffer_.size() << " vs. " << otherBuffer.size()
                 << "." << endl;
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
  size_t lastRecord_;
  size_t readCounter_;
  vector<char> buffer_;
};

bool compareVRSfiles(
    FilteredVRSFileReader& first,
    FilteredVRSFileReader& second,
    const CopyOptions& copyOptions) {
  double startTimestamp, endTimestamp;
  first.getConstrainedTimeRange(startTimestamp, endTimestamp);
  first.preRollConfigAndState(); // make sure to copy most recent config & state records

  ThrottledWriter throttledWriter(copyOptions);
  throttledWriter.initTimeRange(startTimestamp, endTimestamp);

  map<StreamId, StreamId> idMap;
  if (!buildIdMap(first, second, idMap)) {
    cerr << "Streams don't match." << endl;
    return false;
  }
  bool match = true;
  if (first.reader.getTags() != second.reader.getTags()) {
    cerr << "File tags don't match." << endl;
    match = false;
  }
  for (auto ids : idMap) {
    const StreamTags& firstTags = first.reader.getTags(ids.first);
    const StreamTags& secondTags = second.reader.getTags(ids.second);
    if (firstTags.vrs != secondTags.vrs) {
      cerr << "The VRS tags of the stream " << ids.first.getNumericName() << '/'
           << ids.second.getNumericName() << " don't match." << endl;
      match = false;
    }
    if (firstTags.user != secondTags.user) {
      cerr << "The user tags of the stream " << ids.first.getNumericName() << '/'
           << ids.second.getNumericName() << " don't match." << endl;
      match = false;
    }
  }
  if (!match) {
    return false;
  }
  atomic<int> diffCounter{0};
  bool noError;
  vector<unique_ptr<RecordHolder>> holders;
  vector<unique_ptr<RecordMaster>> checkers;
  for (const auto& iter : idMap) {
    holders.emplace_back(make_unique<RecordHolder>());
    second.reader.setStreamPlayer(iter.second, holders.back().get());
    checkers.emplace_back(make_unique<RecordMaster>(
        diffCounter, noError, iter.second, second.reader, holders.back().get()));
    first.reader.setStreamPlayer(iter.first, checkers.back().get());
  }
  size_t decodedCount;
  if (!iterateChecker(first, decodedCount, noError, &throttledWriter)) {
    return false;
  }
  if (!noError) {
    cerr << "Errors happened while reading the files" << endl;
  }
  throttledWriter.closeFile();
  return noError && diffCounter == 0;
}

bool compareVerbatim(
    FilteredVRSFileReader& first,
    FilteredVRSFileReader& second,
    bool showProgress) {
  const char* kStatus = "Comparing ";
  const char* kReset = showProgress ? kResetCurrentLine : "";
  std::unique_ptr<FileHandler> source, dest;
  int status = first.openFile(source);
  if (status != 0 || (status = second.openFile(dest)) != 0) {
    cerr << "Can't open files to compare: " << errorCodeToMessage(status) << "." << endl;
    return false;
  }
  if (source->getTotalSize() != dest->getTotalSize()) {
    cout << "The files have different sizes: " << source->getTotalSize() << " vs. "
         << dest->getTotalSize() << " bytes. " << endl;
    return false;
  }
  size_t totalSize = static_cast<size_t>(source->getTotalSize());
  vector<char> srcBuffer, dstBuffer;
  for (size_t offset = 0; offset < totalSize; offset += kDownloadChunkSize) {
    size_t length = std::min<size_t>(kDownloadChunkSize, totalSize - offset);
    printProgress(kStatus, offset + length / 4, totalSize, showProgress);
    srcBuffer.resize(length);
    int error = source->read(srcBuffer.data(), length);
    if (error == 0) {
      printProgress(kStatus, offset + 3 * length / 4, totalSize, showProgress);
      dstBuffer.resize(length);
      error = dest->read(dstBuffer.data(), length);
      if (error == 0) {
        if (srcBuffer != dstBuffer) {
          cout << kReset << "Chunk #" << (offset / kDownloadChunkSize) + 1 << " is different."
               << endl;
          const size_t kLineSize = 16; // How many bytes per line
          const bool kPrintAscii = true;
          printDifferences(srcBuffer, dstBuffer, kLineSize, kPrintAscii);
          return false;
        }
      } else {
        cerr << kReset << "Read file error: " << errorCodeToMessage(error) << "." << endl;
        return false;
      }
    } else {
      cerr << kReset << "Read file error: " << errorCodeToMessage(error) << "." << endl;
      return false;
    }
  }
  cerr << kReset;
  return true;
}

} // namespace vrs::utils
