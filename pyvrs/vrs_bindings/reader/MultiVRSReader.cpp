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

#include "MultiVRSReader.h"

#include <algorithm>
#include <iterator>
#include <sstream>
#include <stdexcept>
#include <string>

#include <fmt/format.h>

#include <pybind11/attr.h>
#include <pybind11/cast.h>
#include <pybind11/operators.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#define DEFAULT_LOG_CHANNEL "MultiVRSReader"
#include <logging/Log.h>
#include <logging/Verify.h>

#include <vrs/ErrorCode.h>
#include <vrs/FileHandlerFactory.h>
#include <vrs/helpers/Strings.h>
#include <vrs/utils/PixelFrame.h>
#include <vrs/utils/VideoRecordFormatStreamPlayer.h>

#include "../VrsBindings.h"
#include "../utils/PyExceptions.h"
#include "../utils/PyRecord.h"
#include "../utils/PyUtils.h"
#include "FactoryHelper.hpp"

#if IS_VRS_OSS_CODE()
using pyMultiReader = pyvrs::MultiVRSReader;
#else
#include "MultiVRSReader_fb.h"
using pyMultiReader = pyvrs::fbMultiVRSReader;
#endif

using UniqueStreamId = vrs::MultiRecordFileReader::UniqueStreamId;

namespace pyvrs {

bool MultiVRSReader::MultiVideoRecordFormatStreamPlayer::checkSkipTrailingBlocks(
    const CurrentRecord& record,
    size_t blockIndex) {
  // check whether we should stop reading further as the next content block will be considered
  // trailing for this device type and currently processed record
  const UniqueStreamId streamId =
      multiVRSReader_.getUniqueStreamIdForRecordIndex(multiVRSReader_.nextRecordIndex_);
  auto trailingBlockCount = multiVRSReader_.firstSkippedTrailingBlockIndex_.find(
      {streamId.getTypeId(), record.recordType});
  if (trailingBlockCount != multiVRSReader_.firstSkippedTrailingBlockIndex_.end()) {
    return (blockIndex + 1) < trailingBlockCount->second;
  } else {
    return true;
  }
}

bool MultiVRSReader::MultiVideoRecordFormatStreamPlayer::processRecordHeader(
    const CurrentRecord& record,
    DataReference& outDataRef) {
  multiVRSReader_.lastRecord_.recordFormatVersion = record.formatVersion;
  return RecordFormatStreamPlayer::processRecordHeader(record, outDataRef);
}

bool MultiVRSReader::MultiVideoRecordFormatStreamPlayer::onDataLayoutRead(
    const CurrentRecord& record,
    size_t blkIdx,
    DataLayout& dl) {
  PyObject* dic = PyDict_New();
  multiVRSReader_.lastRecord_.datalayoutBlocks.emplace_back(pyWrap(dic));
  dl.forEachDataPiece(
      [dic](const DataPiece* piece) { getDataPieceValuePyObjectorRegistry().map(dic, piece); },
      DataPieceType::Value);

  dl.forEachDataPiece(
      [dic](const DataPiece* piece) { getDataPieceArrayPyObjectorRegistry().map(dic, piece); },
      DataPieceType::Array);

  dl.forEachDataPiece(
      [dic](const DataPiece* piece) { getDataPieceVectorPyObjectorRegistry().map(dic, piece); },
      DataPieceType::Vector);

  dl.forEachDataPiece(
      [dic, &encoding = multiVRSReader_.encoding_](const DataPiece* piece) {
        getDataPieceStringMapPyObjectorRegistry().map(dic, piece, encoding);
      },
      DataPieceType::StringMap);

  dl.forEachDataPiece(
      [dic, &encoding = multiVRSReader_.encoding_](const DataPiece* piece) {
        const auto& value = reinterpret_cast<const DataPieceString*>(piece)->get();
        std::string errors;
        pyDict_SetItemWithDecRef(
            dic,
            Py_BuildValue("(s,s)", piece->getLabel().c_str(), "string"),
            unicodeDecode(value, encoding, errors));
      },
      DataPieceType::String);

  return checkSkipTrailingBlocks(record, blkIdx);
}

bool MultiVRSReader::MultiVideoRecordFormatStreamPlayer::onImageRead(
    const CurrentRecord& record,
    size_t blkIdx,
    const ContentBlock& cb) {
  return setBlock(multiVRSReader_.lastRecord_.images, record, blkIdx, cb);
}

bool MultiVRSReader::MultiVideoRecordFormatStreamPlayer::onAudioRead(
    const CurrentRecord& record,
    size_t blkIdx,
    const ContentBlock& cb) {
  return setBlock(multiVRSReader_.lastRecord_.audioBlocks, record, blkIdx, cb);
}

bool MultiVRSReader::MultiVideoRecordFormatStreamPlayer::onCustomBlockRead(
    const CurrentRecord& rec,
    size_t bix,
    const ContentBlock& cb) {
  return setBlock(multiVRSReader_.lastRecord_.customBlocks, rec, bix, cb);
}

bool MultiVRSReader::MultiVideoRecordFormatStreamPlayer::onUnsupportedBlock(
    const CurrentRecord& cr,
    size_t bix,
    const ContentBlock& cb) {
  return setBlock(multiVRSReader_.lastRecord_.unsupportedBlocks, cr, bix, cb);
}

bool MultiVRSReader::MultiVideoRecordFormatStreamPlayer::setBlock(
    vector<ContentBlockBuffer>& blocks,
    const CurrentRecord& record,
    size_t blockIndex,
    const ContentBlock& contentBlock) {
  // When we are reading video encoded files "and" we jump to certain frame, this method will be
  // called for the non-requested frame.
  // This is because if there is a missing frames (we need to start reading from the key frame), we
  // call readMissingFrames via recordReadComplete method.
  // This directly invokes the readRecord in RecordFileReader and doesn't go through any of
  // readRecord method in VRSReader class, which doesn't reset the last read records.
  // We need to make sure that the lastRecord_ only contains the block that the requested record's
  // block.
  if (blocks.size() >= blockIndex) {
    blocks.clear();
  }
  blocks.emplace_back(contentBlock);
  size_t blockSize = contentBlock.getBlockSize();
  if (blockSize == ContentBlock::kSizeUnknown) {
    XR_LOGW("Block size unknown for {}", contentBlock.asString());
    return false;
  }
  if (blockSize > 0) {
    ContentBlockBuffer& block = blocks.back();
    ImageConversion imageConversion = multiVRSReader_.getImageConversion(
        multiVRSReader_.getUniqueStreamIdForRecordIndex(multiVRSReader_.nextRecordIndex_));
    if (contentBlock.getContentType() != ContentType::IMAGE ||
        imageConversion == ImageConversion::Off) {
      // default handling
      auto& data = block.bytes;
      data.resize(blockSize);
      record.reader->read(data);
      block.bytesAdjusted = false;
      if (contentBlock.getContentType() == ContentType::IMAGE) {
        // for raw images, we return a structured array based on the image format
        block.structuredArray = (contentBlock.image().getImageFormat() == vrs::ImageFormat::RAW);
      } else if (contentBlock.getContentType() != ContentType::AUDIO) {
        block.structuredArray = true;
      } else {
        block.structuredArray = false;
      }
    } else if (imageConversion == ImageConversion::RawBuffer) {
      // default handling
      auto& data = block.bytes;
      data.resize(blockSize);
      record.reader->read(data);
      block.bytesAdjusted = false;
      block.structuredArray = false;
    } else {
      // image conversion handling
      if (imageConversion == ImageConversion::RecordUnreadBytesBackdoor) {
        // Grab all remaining bytes in the record (NOTE: including
        // the bytes of any subsequent content blocks!) and return
        // them as a byte image of height 1. This is a backdoor for
        // accessing image content block data in legacy VRS files
        // with incorrect image specs that cannot practically be
        // rewritten to be compliant. This backdoor should be used
        // with care, and only as a last resort.
        uint32_t unreadBytes = record.reader->getUnreadBytes();
        block.spec = ContentBlock(
            ImageContentBlockSpec(
                ImageFormat::RAW,
                PixelFormat::GREY8,
                unreadBytes, // width
                1), // height
            unreadBytes);
        auto& data = block.bytes;
        data.resize(unreadBytes);
        record.reader->read(data);
        block.structuredArray = false;
      } else {
        std::shared_ptr<utils::PixelFrame> frame;
        bool frameValid;
        if (contentBlock.image().getImageFormat() == vrs::ImageFormat::VIDEO) {
          frame = make_shared<utils::PixelFrame>(contentBlock.image());
          frameValid = (tryToDecodeFrame(*frame, record, contentBlock) == 0);
        } else {
          frameValid = utils::PixelFrame::readFrame(frame, record.reader, contentBlock);
        }
        if (XR_VERIFY(frameValid)) {
          block.structuredArray = true;
          // the image was read & maybe decompressed. Does it need to be converted, too?
          if (imageConversion == ImageConversion::Normalize ||
              imageConversion == ImageConversion::NormalizeGrey8) {
            std::shared_ptr<utils::PixelFrame> convertedFrame;
            bool grey16supported = (imageConversion == ImageConversion::Normalize);
            utils::PixelFrame::normalizeFrame(frame, convertedFrame, grey16supported);
            block.spec = convertedFrame->getSpec();
            block.bytes.swap(convertedFrame->getBuffer());
          } else {
            block.spec = frame->getSpec();
            block.bytes.swap(frame->getBuffer());
          }
        } else {
          // we failed to produce something usable, just return the raw buffer...
          block.structuredArray = false;
        }
      }
    }
  }
  return checkSkipTrailingBlocks(record, blockIndex);
}

void MultiVRSReader::init() {
  initVrsBindings();
}

void MultiVRSReader::open(const std::string& path) {
  std::vector<std::string> pathVector;
  pathVector.emplace_back(path);
  open(pathVector);
}

void MultiVRSReader::open(const std::vector<std::string>& paths) {
  nextRecordIndex_ = 0;
  int status = reader_.open(paths);
  if (status != 0) {
    stringstream ss;
    const std::unique_ptr<FileHandler> fileHandler = reader_.getFileHandler();
    const std::string fileHandlerName =
        fileHandler == nullptr ? "none" : fileHandler->getFileHandlerName();
    ss << "Could not open \"" << fmt::format("{}", fmt::join(paths, ", "))
       << "\" using fileHandler \"" << fileHandlerName
       << "\" : " << errorCodeToMessageWithCode(status);
    close();
    throw std::runtime_error(ss.str());
  }
  if (autoReadConfigurationRecord_) {
    for (const auto& streamId : reader_.getStreams()) {
      lastReadConfigIndex_[streamId] = std::numeric_limits<uint32_t>::max();
    }
  }
}

int MultiVRSReader::close() {
  return reader_.close();
}

void MultiVRSReader::setEncoding(const string& encoding) {
  encoding_ = encoding;
}

string MultiVRSReader::getEncoding() {
  return encoding_;
}

py::object MultiVRSReader::getFileChunks() const {
  vector<std::pair<string, int64_t>> chunks = reader_.getFileChunks();
  Py_ssize_t listSize = static_cast<Py_ssize_t>(chunks.size());
  PyObject* list = PyList_New(listSize);
  Py_ssize_t index = 0;
  for (const auto& chunk : chunks) {
    PyList_SetItem(
        list,
        index++,
        Py_BuildValue("{s:s,s:i}", "path", chunk.first.c_str(), "size", chunk.second));
  }
  return pyWrap(list);
}

double MultiVRSReader::getMaxAvailableTimestamp() {
  const uint32_t recordCount = reader_.getRecordCount();
  if (recordCount == 0) {
    return 0;
  }
  return reader_.getRecord(recordCount - 1)->timestamp;
}

double MultiVRSReader::getMinAvailableTimestamp() {
  if (reader_.getRecordCount() == 0) {
    return 0;
  }
  return reader_.getRecord(0)->timestamp;
}

size_t MultiVRSReader::getAvailableRecordsSize() {
  return reader_.getRecordCount();
}

std::set<string> MultiVRSReader::getAvailableRecordTypes() {
  if (recordTypes_.empty()) {
    initRecordSummaries();
  }
  return recordTypes_;
}

std::set<string> MultiVRSReader::getAvailableStreamIds() {
  std::set<string> streamIds;
  for (const auto& streamId : reader_.getStreams()) {
    streamIds.insert(streamId.getNumericName());
  }
  return streamIds;
}

std::map<string, int> MultiVRSReader::recordCountByTypeFromStreamId(const string& streamId) {
  if (recordCountsByTypeAndStreamIdMap_.empty()) {
    initRecordSummaries();
  }
  StreamId id = getStreamId(streamId);
  return recordCountsByTypeAndStreamIdMap_[id];
}

py::object MultiVRSReader::getTags() {
  const auto& tags = reader_.getTags();
  PyObject* dic = _PyDict_NewPresized(tags.size());
  std::string errors;
  for (const auto& iter : tags) {
    PyDict_SetItem(
        dic,
        unicodeDecode(iter.first, encoding_, errors),
        unicodeDecode(iter.second, encoding_, errors));
  }
  return pyWrap(dic);
}
py::object MultiVRSReader::getTags(const string& streamId) {
  StreamId id = getStreamId(streamId);
  const auto& tags = reader_.getTags(id).user;
  PyObject* dic = _PyDict_NewPresized(tags.size());
  std::string errors;
  for (const auto& iter : tags) {
    PyDict_SetItem(
        dic,
        unicodeDecode(iter.first, encoding_, errors),
        unicodeDecode(iter.second, encoding_, errors));
  }
  return pyWrap(dic);
}

std::vector<string> MultiVRSReader::getStreams() {
  const auto& streams = reader_.getStreams();
  std::vector<string> streamIds;
  streamIds.reserve(streams.size());
  for (const auto& id : streams) {
    streamIds.emplace_back(id.getNumericName());
  }
  return streamIds;
}
std::vector<string> MultiVRSReader::getStreams(RecordableTypeId recordableTypeId) {
  const auto& streams = reader_.getStreams();
  std::vector<string> streamIds;
  for (const auto& id : streams) {
    if (id.getTypeId() == recordableTypeId) {
      streamIds.emplace_back(id.getNumericName());
    }
  }
  return streamIds;
}
std::vector<string> MultiVRSReader::getStreams(
    RecordableTypeId recordableTypeId,
    const string& flavor) {
  auto streams = reader_.getStreams(recordableTypeId, flavor);
  std::vector<string> streamIds;
  streamIds.reserve(streams.size());
  for (const auto& id : streams) {
    streamIds.emplace_back(id.getNumericName());
  }
  return streamIds;
}

string MultiVRSReader::findStream(
    RecordableTypeId recordableTypeId,
    const string& tagName,
    const string& tagValue) {
  StreamId id = reader_.getStreamForTag(tagName, tagValue, recordableTypeId);
  if (!id.isValid()) {
    throw StreamNotFoundError(recordableTypeId, reader_.getStreams());
  }
  return id.getNumericName();
}

py::object MultiVRSReader::getStreamInfo(const string& streamId) {
  StreamId id = getStreamId(streamId);
  PyObject* dic = PyDict_New();
  int config = 0;
  int state = 0;
  int data = 0;
  for (const auto& recordInfo : reader_.getIndex(id)) {
    if (recordInfo->recordType == Record::Type::DATA) {
      data++;
    } else if (recordInfo->recordType == Record::Type::CONFIGURATION) {
      config++;
    } else if (recordInfo->recordType == Record::Type::STATE) {
      state++;
    }
  }
  PyDict_SetItem(dic, pyObject("configuration_records_count"), pyObject(config));
  PyDict_SetItem(dic, pyObject("state_records_count"), pyObject(state));
  PyDict_SetItem(dic, pyObject("data_records_count"), pyObject(data));
  PyDict_SetItem(dic, pyObject("device_name"), pyObject(reader_.getOriginalRecordableTypeName(id)));
  string flavor = reader_.getFlavor(id);
  if (!flavor.empty()) {
    PyDict_SetItem(dic, pyObject("flavor"), pyObject(flavor));
  }
  addStreamInfo(dic, id, Record::Type::CONFIGURATION);
  addStreamInfo(dic, id, Record::Type::STATE);
  addStreamInfo(dic, id, Record::Type::DATA);
  return pyWrap(dic);
}

void MultiVRSReader::addStreamInfo(PyObject* dic, const StreamId& id, Record::Type recordType) {
  addRecordInfo(dic, "first_", recordType, reader_.getRecord(id, recordType, 0));
  addRecordInfo(dic, "last_", recordType, reader_.getLastRecord(id, recordType));
}

void MultiVRSReader::addRecordInfo(
    PyObject* dic,
    const string& prefix,
    Record::Type recordType,
    const IndexRecord::RecordInfo* record) {
  if (record) {
    string type = lowercaseTypeName(recordType);
    int32_t recordIndex = static_cast<int32_t>(reader_.getRecordIndex(record));
    PyDict_SetItem(dic, pyObject(prefix + type + "_record_index"), pyObject(recordIndex));
    PyDict_SetItem(dic, pyObject(prefix + type + "_record_timestamp"), pyObject(record->timestamp));
  }
}

void MultiVRSReader::enableStream(const StreamId& id) {
  auto playerByStreamIdIt = playerByStreamIdMap_.emplace(id, *this).first;
  reader_.setStreamPlayer(id, &playerByStreamIdIt->second);
  enabledStreams_.insert(id);
}

void MultiVRSReader::enableStream(const string& streamId) {
  enableStream(getStreamId(streamId));
}

int MultiVRSReader::enableStreams(RecordableTypeId recordableTypeId, const std::string& flavor) {
  int count = 0;
  auto streams = reader_.getStreams(recordableTypeId, flavor);
  for (auto id : streams) {
    enableStream(id);
    count++;
  }
  return count;
}

int MultiVRSReader::enableStreamsByIndexes(const std::vector<int>& indexes) {
  int count = 0;
  const auto& recordables = reader_.getStreams();
  std::vector<StreamId> playable_streams;
  playable_streams.reserve(recordables.size());

  for (const auto& id : recordables) {
    RecordFormatMap formats;
    if (reader_.getRecordFormats(id, formats) > 0) {
      for (const auto& format : formats) {
        if (format.second.getBlocksOfTypeCount(ContentType::IMAGE) > 0) {
          playable_streams.push_back(id);
          break;
        }
      }
    }
  }

  for (const auto& index : indexes) {
    if (index >= playable_streams.size()) {
      continue;
    }
    auto it = playable_streams.begin();
    std::advance(it, index);
    enableStream(*it);
    count++;
  }
  return count;
}

int MultiVRSReader::enableAllStreams() {
  const auto& recordables = reader_.getStreams();
  for (const auto& id : recordables) {
    enableStream(id);
  }
  return static_cast<int>(recordables.size());
}

std::vector<string> MultiVRSReader::getEnabledStreams() {
  vector<string> streamIds;
  streamIds.reserve(enabledStreams_.size());
  for (const auto& id : enabledStreams_) {
    streamIds.emplace_back(id.getNumericName());
  }
  return streamIds;
}

ImageConversion MultiVRSReader::getImageConversion(const StreamId& id) {
  auto iter = streamImageConversion_.find(id);
  return iter == streamImageConversion_.end() ? imageConversion_ : iter->second;
}

void MultiVRSReader::setImageConversion(ImageConversion conversion) {
  imageConversion_ = conversion;
  streamImageConversion_.clear();
  resetVideoFrameHandler();
}

void MultiVRSReader::setImageConversion(const StreamId& id, ImageConversion conversion) {
  if (!id.isValid()) {
    throw py::value_error("Invalid recordable ID");
  }
  streamImageConversion_[id] = conversion;
  resetVideoFrameHandler(id);
}

void MultiVRSReader::setImageConversion(const string& streamId, ImageConversion conversion) {
  setImageConversion(getStreamId(streamId), conversion);
}

int MultiVRSReader::setImageConversion(
    RecordableTypeId recordableTypeId,
    ImageConversion conversion) {
  int count = 0;
  for (const auto& id : reader_.getStreams()) {
    if (id.getTypeId() == recordableTypeId) {
      setImageConversion(id, conversion);
      count++;
    }
  }
  return count;
}

int MultiVRSReader::getRecordsCount(const string& streamId, const Record::Type recordType) {
  if (recordCountsByTypeAndStreamIdMap_.empty()) {
    initRecordSummaries();
  }
  StreamId id = getStreamId(streamId);
  return recordCountsByTypeAndStreamIdMap_[id][lowercaseTypeName(recordType)];
}

py::object MultiVRSReader::getAllRecordsInfo() {
  const uint32_t recordCount = reader_.getRecordCount();
  Py_ssize_t listSize = static_cast<Py_ssize_t>(recordCount);
  PyObject* list = PyList_New(listSize);
  int32_t recordIndex = 0;
  for (uint32_t i = 0; i < recordCount; ++i, ++recordIndex) {
    const auto pyRecordInfo = getRecordInfo(*reader_.getRecord(i), recordIndex);
    PyList_SetItem(list, recordIndex, pyRecordInfo);
  }
  return pyWrap(list);
}

py::object MultiVRSReader::getRecordsInfo(int32_t firstIndex, int32_t count) {
  const uint32_t recordCount = reader_.getRecordCount();
  uint32_t first = static_cast<uint32_t>(firstIndex);
  if (first >= recordCount) {
    throw py::stop_iteration("No more records");
  }
  if (count <= 0) {
    throw py::value_error("invalid number of records requested: " + to_string(count));
  }
  uint32_t last = std::min<uint32_t>(recordCount, first + static_cast<uint32_t>(count));
  PyObject* list = PyList_New(static_cast<size_t>(last - first));
  int32_t recordIndex = 0;
  for (uint32_t sourceIndex = first; sourceIndex < last; ++sourceIndex, ++recordIndex) {
    const auto pyRecordInfo = getRecordInfo(*reader_.getRecord(sourceIndex), recordIndex);
    PyList_SetItem(list, recordIndex, pyRecordInfo);
  }
  return pyWrap(list);
}

py::object MultiVRSReader::getEnabledStreamsRecordsInfo() {
  if (enabledStreams_.size() == reader_.getStreams().size()) {
    return getAllRecordsInfo(); // we're reading everything: use faster version
  }
  PyObject* list = PyList_New(0);
  if (enabledStreams_.size() > 0) {
    int32_t recordIndex = 0;
    for (uint32_t i = 0; i < reader_.getRecordCount(); ++i, ++recordIndex) {
      const auto* record = reader_.getRecord(i);
      if (enabledStreams_.find(reader_.getUniqueStreamId(record)) != enabledStreams_.end()) {
        auto pyRecordInfo = getRecordInfo(*record, recordIndex);
        PyList_Append(list, pyRecordInfo);
        Py_DECREF(pyRecordInfo);
      }
    }
  }
  return pyWrap(list);
}

py::object MultiVRSReader::gotoRecord(int index) {
  nextRecordIndex_ = static_cast<uint32_t>(index);
  return getNextRecordInfo("Invalid record index");
}

py::object MultiVRSReader::gotoTime(double timestamp) {
  const IndexRecord::RecordInfo* record = reader_.getRecordByTime(timestamp);
  nextRecordIndex_ = reader_.getRecordIndex(record);
  return getNextRecordInfo("No record found for given time");
}

py::object MultiVRSReader::getNextRecordInfo(const char* errorMessage) {
  const uint32_t recordCount = reader_.getRecordCount();
  if (nextRecordIndex_ >= recordCount) {
    nextRecordIndex_ = recordCount;
    throw py::index_error(errorMessage);
  }
  const auto* record = reader_.getRecord(nextRecordIndex_);
  if (record == nullptr) {
    throw py::index_error(errorMessage);
  }
  return pyWrap(getRecordInfo(*record, static_cast<int32_t>(nextRecordIndex_)));
}

py::object MultiVRSReader::readNextRecord() {
  skipIgnoredRecords();
  return readNextRecordInternal();
}

py::object MultiVRSReader::readNextRecord(const string& streamId) {
  return readNextRecord(streamId, "any");
}

py::object MultiVRSReader::readNextRecord(RecordableTypeId recordableTypeId) {
  return readNextRecord(recordableTypeId, "any");
}

py::object MultiVRSReader::readNextRecord(const string& streamId, const string& recordType) {
  StreamId id = getStreamId(streamId);
  if (enabledStreams_.find(id) == enabledStreams_.end()) {
    throw py::value_error(
        "Stream " + streamId + " is not enabled. To read record you need to enable it first.");
  }
  Record::Type type = toEnum<Record::Type>(recordType);
  if (type == Record::Type::UNDEFINED && vrs::helpers::strcasecmp(recordType.c_str(), "any") != 0) {
    throw py::value_error("Unsupported record type filter: " + recordType);
  }
  while (nextRecordIndex_ < reader_.getRecordCount() &&
         !match(*reader_.getRecord(nextRecordIndex_), id, type)) {
    ++nextRecordIndex_;
  }
  return readNextRecordInternal();
}

py::object MultiVRSReader::readNextRecord(RecordableTypeId typeId, const string& recordType) {
  Record::Type type = toEnum<Record::Type>(recordType);
  if (type == Record::Type::UNDEFINED && vrs::helpers::strcasecmp(recordType.c_str(), "any") != 0) {
    throw py::value_error("Unsupported record type filter: " + recordType);
  }
  bool candidateStreamFound = false;
  for (const auto& id : enabledStreams_) {
    if (id.getTypeId() == typeId) {
      candidateStreamFound = true;
      break;
    }
  }
  if (!candidateStreamFound) {
    throw StreamNotFoundError(typeId, reader_.getStreams());
  }
  while (nextRecordIndex_ < reader_.getRecordCount() &&
         !match(*reader_.getRecord(nextRecordIndex_), typeId, type)) {
    ++nextRecordIndex_;
  }
  return readNextRecordInternal();
}

py::object MultiVRSReader::readRecord(const string& streamId, const string& recordType, int index) {
  StreamId id = getStreamId(streamId);
  if (enabledStreams_.find(id) == enabledStreams_.end()) {
    throw py::value_error(
        "Stream " + streamId + " is not enabled. To read record you need to enable it first.");
  }
  Record::Type type = toEnum<Record::Type>(recordType);
  if (type == Record::Type::UNDEFINED && vrs::helpers::strcasecmp(recordType.c_str(), "any") != 0) {
    throw py::value_error("Unsupported record type filter: " + recordType);
  }
  const IndexRecord::RecordInfo* record;
  if (vrs::helpers::strcasecmp(recordType.c_str(), "any") == 0) {
    record = reader_.getRecord(id, static_cast<uint32_t>(index));
  } else {
    record = reader_.getRecord(id, type, static_cast<uint32_t>(index));
  }
  if (record == nullptr) {
    nextRecordIndex_ = static_cast<uint32_t>(reader_.getRecordCount());

    throw py::index_error("Invalid record index");
  }
  nextRecordIndex_ = reader_.getRecordIndex(record);
  return readNextRecordInternal();
}

py::object MultiVRSReader::readRecord(int index) {
  if (index < 0 || static_cast<uint32_t>(index) >= reader_.getRecordCount()) {
    throw py::index_error("No record at index: " + to_string(index));
  }
  nextRecordIndex_ = static_cast<uint32_t>(index);
  return readNextRecordInternal();
}

py::object MultiVRSReader::readNextRecordInternal() {
  if (nextRecordIndex_ >= reader_.getRecordCount()) {
    throw py::stop_iteration("No more records");
  }

  // Video codecs require sequential decoding of images.
  // Keep the index of last read record and if the nextRecordIndex_ matches, reuse lastRecord_.
  const IndexRecord::RecordInfo& record = *reader_.getRecord(nextRecordIndex_);

  // Automatically read configuration record if specified.
  if (autoReadConfigurationRecord_ && record.recordType == Record::Type::DATA) {
    readConfigurationRecord(record.streamId, nextRecordIndex_);
  }

  int status = reader_.readRecord(record);
  if (status != 0) {
    throw std::runtime_error("Read error: " + errorCodeToMessageWithCode(status));
  }

  auto r = PyRecord(record, nextRecordIndex_, lastRecord_);
  nextRecordIndex_++;
  return py::cast(r);
}

void MultiVRSReader::skipTrailingBlocks(
    RecordableTypeId recordableTypeId,
    Record::Type recordType,
    size_t firstTrailingContentBlockIndex) {
  resetVideoFrameHandler();
  if (recordType != Record::Type::UNDEFINED) {
    if (firstTrailingContentBlockIndex) {
      firstSkippedTrailingBlockIndex_[{recordableTypeId, recordType}] =
          firstTrailingContentBlockIndex;
    } else {
      firstSkippedTrailingBlockIndex_.erase({recordableTypeId, recordType});
    }
  } else {
    // "any" value of recordType
    for (auto t :
         {Record::Type::STATE,
          Record::Type::DATA,
          Record::Type::CONFIGURATION,
          Record::Type::TAGS}) {
      if (firstTrailingContentBlockIndex) {
        firstSkippedTrailingBlockIndex_[{recordableTypeId, t}] = firstTrailingContentBlockIndex;
      } else {
        firstSkippedTrailingBlockIndex_.erase({recordableTypeId, t});
      }
    }
  }
}

std::vector<int32_t> MultiVRSReader::regenerateEnabledIndices(
    const std::set<string>& recordTypes,
    const std::set<string>& streamIds,
    double minEnabledTimestamp,
    double maxEnabledTimestamp) {
  std::vector<int32_t> enabledIndices;
  std::set<StreamId> streamIdSet;
  std::vector<int> recordTypeExist(static_cast<int>(Record::Type::COUNT));
  for (const auto& recordType : recordTypes) {
    Record::Type type = toEnum<Record::Type>(recordType);
    recordTypeExist[static_cast<int>(type)] = 1;
  }

  for (const auto& streamId : streamIds) {
    const StreamId id = StreamId::fromNumericName(streamId);
    streamIdSet.insert(id);
  }

  for (uint32_t recordIndex = 0; recordIndex < reader_.getRecordCount(); ++recordIndex) {
    const IndexRecord::RecordInfo* record = reader_.getRecord(recordIndex);
    if (record->timestamp > maxEnabledTimestamp) {
      break;
    }
    if (record->timestamp >= minEnabledTimestamp) {
      if (recordTypeExist[static_cast<int>(record->recordType)] &&
          streamIdSet.find(reader_.getUniqueStreamId(record)) != streamIdSet.end()) {
        enabledIndices.push_back(recordIndex);
      }
    }
  }
  return enabledIndices;
}

double MultiVRSReader::getTimestampForIndex(int idx) {
  const IndexRecord::RecordInfo* record = reader_.getRecord(static_cast<uint32_t>(idx));
  if (record == nullptr) {
    throw py::index_error("Index out of range.");
  }
  return record->timestamp;
}

string MultiVRSReader::getStreamIdForIndex(int recordIndex) {
  if (recordIndex < 0 || recordIndex >= reader_.getRecordCount()) {
    throw py::index_error("Index out of range.");
  }
  return getUniqueStreamIdForRecordIndex(static_cast<uint32_t>(recordIndex)).getNumericName();
}

int32_t MultiVRSReader::getRecordIndexByTime(const string& streamId, double timestamp) {
  StreamId id = getStreamId(streamId);
  auto record = reader_.getRecordByTime(id, timestamp);
  if (record == nullptr) {
    throw py::value_error(
        "No record at timestamp " + to_string(timestamp) + " in stream " + streamId);
  }
  return reader_.getRecordIndex(record);
}

int32_t MultiVRSReader::getNearestRecordIndexByTime(
    double timestamp,
    double epsilon,
    const string& streamId) {
  StreamId id = getStreamId(streamId);
  auto record = reader_.getNearestRecordByTime(timestamp, epsilon, id);
  if (record == nullptr) {
    throw TimestampNotFoundError(timestamp, epsilon, id);
  }
  return reader_.getRecordIndex(record);
}

std::vector<double> MultiVRSReader::getTimestampListForIndices(
    const std::vector<int32_t>& indices) {
  std::vector<double> timestamps;
  timestamps.reserve(indices.size());
  for (const auto idx : indices) {
    const IndexRecord::RecordInfo* record = reader_.getRecord(static_cast<uint32_t>(idx));
    if (record == nullptr) {
      throw py::index_error("Index out of range.");
    }
    timestamps.push_back(record->timestamp);
  }
  return timestamps;
}

int32_t MultiVRSReader::getNextIndex(const string& streamId, const string& recordType, int index) {
  const StreamId id = getStreamId(streamId);
  Record::Type type = toEnum<Record::Type>(recordType);
  uint32_t nextIndex = static_cast<uint32_t>(index);
  const IndexRecord::RecordInfo* record;
  while ((record = reader_.getRecord(nextIndex)) != nullptr && !match(*record, id, type)) {
    nextIndex++;
  }
  if (record == nullptr) {
    throw py::index_error("There are no record for " + streamId + " after " + to_string(index));
  }
  return nextIndex;
}

int32_t MultiVRSReader::getPrevIndex(const string& streamId, const string& recordType, int index) {
  StreamId id = getStreamId(streamId);
  Record::Type type = toEnum<Record::Type>(recordType);
  int prevIndex = index;
  const IndexRecord::RecordInfo* record = nullptr;
  while (prevIndex >= 0 &&
         (record = reader_.getRecord(static_cast<uint32_t>(prevIndex))) != nullptr &&
         !match(*record, id, type)) {
    prevIndex--;
  }
  if (record == nullptr) {
    throw py::index_error("There are no record for " + streamId + " before " + to_string(index));
  }
  return prevIndex;
}

void MultiVRSReader::readConfigurationRecord(const StreamId& streamId, uint32_t idx) {
  if (configIndex_.empty()) {
    for (uint32_t i = 0; i < reader_.getRecordCount(); i++) {
      const auto* record = reader_.getRecord(i);
      if (record->recordType == Record::Type::CONFIGURATION) {
        configIndex_[record->streamId].push_back(i);
      }
    }
  }
  auto it = lower_bound(configIndex_[streamId].begin(), configIndex_[streamId].end(), idx);
  if (it != configIndex_[streamId].begin()) {
    it--;
  } else {
    XR_LOGE("{} doesn't have config record before reading {}", streamId.getNumericName(), idx);
    return;
  }

  if (lastReadConfigIndex_[streamId] == *it) {
    return;
  }

  const IndexRecord::RecordInfo& record = *reader_.getRecord(*it);
  int status = reader_.readRecord(record);
  if (status != 0) {
    throw py::index_error("Failed to read prior configuration record.");
  }
  lastReadConfigIndex_[streamId] = *it;
  // Need to clear lastRecord_ here to avoid having content block for both config record & data
  // record.
  lastRecord_.clear();
}

void MultiVRSReader::resetVideoFrameHandler() {
  for (auto& player : playerByStreamIdMap_) {
    player.second.resetVideoFrameHandler();
  }
}

void MultiVRSReader::resetVideoFrameHandler(const StreamId& id) {
  for (auto& player : playerByStreamIdMap_) {
    player.second.resetVideoFrameHandler(id);
  }
}

void MultiVRSReader::skipIgnoredRecords() {
  while (nextRecordIndex_ < reader_.getRecordCount() &&
         enabledStreams_.find(getUniqueStreamIdForRecordIndex(nextRecordIndex_)) ==
             enabledStreams_.end()) {
    nextRecordIndex_++;
  }
}

void MultiVRSReader::initRecordSummaries() {
  recordCountsByTypeAndStreamIdMap_.clear();
  recordTypes_.clear();

  int recordTypeSize = static_cast<int>(Record::Type::COUNT);
  std::vector<int> countsByRecordType(recordTypeSize);
  std::map<StreamId, std::vector<int>> recordCountsByTypeAndStreamId;
  for (const auto& streamId : reader_.getStreams()) {
    recordCountsByTypeAndStreamId[streamId] = std::vector<int>(recordTypeSize);
  }

  for (uint32_t recordIndex = 0; recordIndex < reader_.getRecordCount(); ++recordIndex) {
    const IndexRecord::RecordInfo& record = *reader_.getRecord(recordIndex);
    recordCountsByTypeAndStreamId[reader_.getUniqueStreamId(&record)]
                                 [static_cast<int>(record.recordType)]++;
    countsByRecordType[static_cast<int>(record.recordType)]++;
  }

  for (int i = 0; i < recordTypeSize; i++) {
    if (countsByRecordType[i] > 0) {
      string type = lowercaseTypeName(static_cast<Record::Type>(i));
      recordTypes_.insert(type);
    }
  }

  for (const auto& recordCounts : recordCountsByTypeAndStreamId) {
    recordCountsByTypeAndStreamIdMap_[recordCounts.first][lowercaseTypeName(
        Record::Type::CONFIGURATION)] =
        recordCounts.second[static_cast<int>(Record::Type::CONFIGURATION)];
    recordCountsByTypeAndStreamIdMap_[recordCounts.first][lowercaseTypeName(Record::Type::DATA)] =
        recordCounts.second[static_cast<int>(Record::Type::DATA)];
    recordCountsByTypeAndStreamIdMap_[recordCounts.first][lowercaseTypeName(Record::Type::STATE)] =
        recordCounts.second[static_cast<int>(Record::Type::STATE)];
  }
}

StreamId MultiVRSReader::getStreamId(const string& streamId) {
  // Quick parsing of "NNN-DDD", two uint numbers separated by a '-'.
  const StreamId id = StreamId::fromNumericName(streamId);
  const auto& recordables = reader_.getStreams();
  if (id.getTypeId() == RecordableTypeId::Undefined) {
    throw py::value_error("Invalid stream ID: " + streamId);
  }
  if (recordables.find(id) != recordables.end()) {
    return id;
  }
  throw StreamNotFoundError(id.getTypeId(), recordables);
}

PyObject* MultiVRSReader::getRecordInfo(
    const IndexRecord::RecordInfo& record,
    int32_t recordIndex) {
  PyObject* dic = PyDict_New();
  pyDict_SetItemWithDecRef(dic, pyObject("record_index"), pyObject(recordIndex));
  string type = lowercaseTypeName(record.recordType);
  pyDict_SetItemWithDecRef(dic, pyObject("record_type"), pyObject(type));
  pyDict_SetItemWithDecRef(dic, pyObject("record_timestamp"), pyObject(record.timestamp));
  const std::string streamIdName = reader_.getUniqueStreamId(&record).getNumericName();
  pyDict_SetItemWithDecRef(dic, pyObject("stream_id"), pyObject(streamIdName));
  pyDict_SetItemWithDecRef(dic, pyObject("recordable_id"), pyObject(streamIdName));
  return dic;
}

bool MultiVRSReader::match(
    const IndexRecord::RecordInfo& record,
    StreamId id,
    Record::Type recordType) const {
  return reader_.getUniqueStreamId(&record) == id &&
      enabledStreams_.find(id) != enabledStreams_.end() &&
      (recordType == Record::Type::UNDEFINED || record.recordType == recordType);
}
bool MultiVRSReader::match(
    const IndexRecord::RecordInfo& record,
    RecordableTypeId typeId,
    Record::Type recordType) const {
  const UniqueStreamId steamId = reader_.getUniqueStreamId(&record);
  return steamId.getTypeId() == typeId && enabledStreams_.find(steamId) != enabledStreams_.end() &&
      (recordType == Record::Type::UNDEFINED || record.recordType == recordType);
}

void pybind_multivrsreader(py::module& m) {
  // WARNING: Do not use `MultiReader` for production code yet as the behavior/APIs might change.
  auto reader =
      py::class_<pyMultiReader>(m, "MultiReader")
          .def(py::init<bool>())
          .def("open", py::overload_cast<const std::string&>(&pyMultiReader::open))
          .def("open", py::overload_cast<const std::vector<std::string>&>(&pyMultiReader::open))
          .def("close", &pyMultiReader::close)
          .def("set_encoding", &pyMultiReader::setEncoding)
          .def("get_encoding", &pyMultiReader::getEncoding)
          .def(
              "set_image_conversion",
              py::overload_cast<pyvrs::ImageConversion>(&pyMultiReader::setImageConversion))
          .def(
              "set_image_conversion",
              py::overload_cast<const std::string&, pyvrs::ImageConversion>(
                  &pyMultiReader::setImageConversion))
          .def(
              "set_image_conversion",
              py::overload_cast<vrs::RecordableTypeId, pyvrs::ImageConversion>(
                  &pyMultiReader::setImageConversion))
          .def("get_file_chunks", &pyMultiReader::getFileChunks)
          .def("get_streams", py::overload_cast<>(&pyMultiReader::getStreams))
          .def("get_streams", py::overload_cast<vrs::RecordableTypeId>(&pyMultiReader::getStreams))
          .def(
              "get_streams",
              py::overload_cast<vrs::RecordableTypeId, const std::string&>(
                  &pyMultiReader::getStreams))
          .def("find_stream", &pyMultiReader::findStream)
          .def("get_stream_info", &pyMultiReader::getStreamInfo)
          .def("enable_stream", py::overload_cast<const string&>(&pyMultiReader::enableStream))
          .def("enable_streams", &pyMultiReader::enableStreams)
          .def("enable_streams_by_indexes", &pyMultiReader::enableStreamsByIndexes)
          .def("enable_all_streams", &pyMultiReader::enableAllStreams)
          .def("get_enabled_streams", &pyMultiReader::getEnabledStreams)
          .def("get_all_records_info", &pyMultiReader::getAllRecordsInfo)
          .def("get_records_info", &pyMultiReader::getRecordsInfo)
          .def("get_enabled_streams_records_info", &pyMultiReader::getEnabledStreamsRecordsInfo)
          .def("goto_record", &pyMultiReader::gotoRecord)
          .def("goto_time", &pyMultiReader::gotoTime)
          .def("read_next_record", py::overload_cast<>(&pyMultiReader::readNextRecord))
          .def(
              "read_next_record",
              py::overload_cast<const std::string&>(&pyMultiReader::readNextRecord))
          .def(
              "read_next_record",
              py::overload_cast<const std::string&, const std::string&>(
                  &pyMultiReader::readNextRecord))
          .def(
              "read_next_record",
              py::overload_cast<vrs::RecordableTypeId>(&pyMultiReader::readNextRecord))
          .def(
              "read_next_record",
              py::overload_cast<vrs::RecordableTypeId, const std::string&>(
                  &pyMultiReader::readNextRecord))
          .def(
              "read_record",
              py::overload_cast<const std::string&, const std::string&, int>(
                  &pyMultiReader::readRecord))
          .def("read_record", py::overload_cast<int>(&pyMultiReader::readRecord))
          .def("skip_trailing_blocks", &pyMultiReader::skipTrailingBlocks)
          .def("get_tags", py::overload_cast<>(&pyMultiReader::getTags))
          .def("get_tags", py::overload_cast<const std::string&>(&pyMultiReader::getTags))
          .def("get_max_available_timestamp", &pyMultiReader::getMaxAvailableTimestamp)
          .def("get_min_available_timestamp", &pyMultiReader::getMinAvailableTimestamp)
          .def("get_available_record_types", &pyMultiReader::getAvailableRecordTypes)
          .def("get_available_stream_ids", &pyMultiReader::getAvailableStreamIds)
          .def("get_timestamp_for_index", &pyMultiReader::getTimestampForIndex)
          .def("get_records_count", &pyMultiReader::getRecordsCount)
          .def("get_available_records_size", &pyMultiReader::getAvailableRecordsSize)
          .def("record_count_by_type_from_stream_id", &pyMultiReader::recordCountByTypeFromStreamId)
          .def("regenerate_enabled_indices", &pyMultiReader::regenerateEnabledIndices)
          .def("get_stream_id_for_index", &pyMultiReader::getStreamIdForIndex)
          .def("get_record_index_by_time", &pyMultiReader::getRecordIndexByTime)
          .def("get_nearest_record_index_by_time", &pyMultiReader::getNearestRecordIndexByTime)
          .def("get_timestamp_list_for_indices", &pyMultiReader::getTimestampListForIndices)
          .def("get_next_index", &pyMultiReader::getNextIndex)
          .def("get_prev_index", &pyMultiReader::getPrevIndex);
#if IS_VRS_FB_INTERNAL()
  pybind_fbmultivrsreader(reader);
#endif
}

} // namespace pyvrs
