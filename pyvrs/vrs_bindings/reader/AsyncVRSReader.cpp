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

#include "AsyncVRSReader.h"

#define DEFAULT_LOG_CHANNEL "AsyncVRSReader"
#include <logging/Log.h>
#include <logging/Verify.h>

#include <pybind11/attr.h>
#include <pybind11/cast.h>
#include <pybind11/operators.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include <vrs/helpers/Strings.h>
#include <vrs/utils/PixelFrame.h>

#if IS_VRS_OSS_CODE()
using pyAsyncReader = pyvrs::AsyncVRSReader;
using pyAsyncMultiReader = pyvrs::AsyncMultiVRSReader;
#else
#include "AsyncVRSReader_fb.h"
using pyAsyncReader = pyvrs::fbAsyncVRSReader;
using pyAsyncMultiReader = pyvrs::fbAsyncMultiVRSReader;
#endif

namespace pyvrs {

void AsyncReadJob::performJob(AsyncVRSReader& reader) {
  py::gil_scoped_acquire acquire;
  py::object record = reader.readRecord(index_);
  loop_.attr("call_soon_threadsafe")(future_.attr("set_result"), record);
}

void AsyncReadJob::performJob(AsyncMultiVRSReader& reader) {
  py::gil_scoped_acquire acquire;
  py::object record = reader.readRecord(index_);
  loop_.attr("call_soon_threadsafe")(future_.attr("set_result"), record);
}

AwaitableRecord
AsyncVRSReader::asyncReadRecord(const string& streamId, const string& recordType, int index) {
  StreamId id = getStreamId(streamId);
  if (enabledStreams_.find(id) == enabledStreams_.end()) {
    throw py::value_error("Stream not setup for reading");
  }
  Record::Type type = toEnum<Record::Type>(recordType);
  if (type == Record::Type::UNDEFINED && vrs::helpers::strcasecmp(recordType.c_str(), "any") != 0) {
    throw py::value_error("Unsupported record type filter");
  }
  const IndexRecord::RecordInfo* record;
  if (vrs::helpers::strcasecmp(recordType.c_str(), "any") == 0) {
    record = reader_.getRecord(id, static_cast<uint32_t>(index));
  } else {
    record = reader_.getRecord(id, type, static_cast<uint32_t>(index));
  }
  if (record == nullptr) {
    nextRecordIndex_ = static_cast<uint32_t>(reader_.getIndex().size());
    throw py::index_error("Invalid record index");
  }
  return AwaitableRecord(static_cast<uint32_t>(record - reader_.getIndex().data()), workerQueue_);
}

AwaitableRecord AsyncVRSReader::asyncReadRecord(int index) {
  if (static_cast<size_t>(index) >= reader_.getIndex().size()) {
    throw py::index_error("No record for this index");
  }
  return AwaitableRecord(static_cast<uint32_t>(index), workerQueue_);
}

AsyncVRSReader::~AsyncVRSReader() {
  shouldEndAsyncThread_ = true;
  if (asyncThread_.joinable()) {
    asyncThread_.join();
  }
  reader_.closeFile();
}

void AsyncVRSReader::asyncThreadActivity() {
  std::unique_ptr<AsyncJob> job;
  while (!shouldEndAsyncThread_) {
    if (workerQueue_.waitForJob(job, 1) && !shouldEndAsyncThread_) {
      job->performJob(*this);
    }
  }
}

AwaitableRecord
AsyncMultiVRSReader::asyncReadRecord(const string& streamId, const string& recordType, int index) {
  StreamId id = getStreamId(streamId);
  if (enabledStreams_.find(id) == enabledStreams_.end()) {
    throw py::value_error("Stream not setup for reading");
  }
  bool anyRecordType = vrs::helpers::strcasecmp(recordType.c_str(), "any") == 0;
  Record::Type type = toEnum<Record::Type>(recordType);
  if (type == Record::Type::UNDEFINED && !anyRecordType) {
    throw py::value_error("Unsupported record type filter");
  }
  const IndexRecord::RecordInfo* record;
  if (anyRecordType) {
    record = reader_.getRecord(id, static_cast<uint32_t>(index));
  } else {
    record = reader_.getRecord(id, type, static_cast<uint32_t>(index));
  }
  if (record == nullptr) {
    nextRecordIndex_ = reader_.getRecordCount();
    throw py::index_error("Invalid record index: " + to_string(index));
  }
  return AwaitableRecord(reader_.getRecordIndex(record), workerQueue_);
}

AwaitableRecord AsyncMultiVRSReader::asyncReadRecord(int index) {
  if (static_cast<uint32_t>(index) >= reader_.getRecordCount()) {
    throw py::index_error("No record for this index");
  }
  return AwaitableRecord(static_cast<uint32_t>(index), workerQueue_);
}

AsyncMultiVRSReader::~AsyncMultiVRSReader() {
  shouldEndAsyncThread_ = true;
  if (asyncThread_.joinable()) {
    asyncThread_.join();
  }
  reader_.close();
}

void AsyncMultiVRSReader::asyncThreadActivity() {
  std::unique_ptr<AsyncJob> job;
  while (!shouldEndAsyncThread_) {
    if (workerQueue_.waitForJob(job, 1) && !shouldEndAsyncThread_) {
      job->performJob(*this);
    }
  }
}

void pybind_asyncvrsreaders(py::module& m) {
  auto reader =
      py::class_<pyAsyncReader>(m, "AsyncReader")
          .def(py::init<bool&>())
          .def("open", py::overload_cast<const std::string&>(&pyAsyncReader::open))
          .def("close", &pyAsyncReader::close)
          .def("set_encoding", &pyAsyncReader::setEncoding)
          .def("get_encoding", &pyAsyncReader::getEncoding)
          .def(
              "set_image_conversion",
              py::overload_cast<pyvrs::ImageConversion>(&pyAsyncReader::setImageConversion))
          .def(
              "set_image_conversion",
              py::overload_cast<const std::string&, pyvrs::ImageConversion>(
                  &pyAsyncReader::setImageConversion))
          .def(
              "set_image_conversion",
              py::overload_cast<vrs::RecordableTypeId, pyvrs::ImageConversion>(
                  &pyAsyncReader::setImageConversion))
          .def("get_file_chunks", &pyAsyncReader::getFileChunks)
          .def("get_streams", py::overload_cast<>(&pyAsyncReader::getStreams))
          .def("get_streams", py::overload_cast<vrs::RecordableTypeId>(&pyAsyncReader::getStreams))
          .def(
              "get_streams",
              py::overload_cast<vrs::RecordableTypeId, const std::string&>(
                  &pyAsyncReader::getStreams))
          .def("find_stream", &pyAsyncReader::findStream)
          .def("get_stream_info", &pyAsyncReader::getStreamInfo)
          .def("enable_stream", py::overload_cast<const string&>(&pyAsyncReader::enableStream))
          .def("enable_streams", &pyAsyncReader::enableStreams)
          .def("enable_streams_by_indexes", &pyAsyncReader::enableStreamsByIndexes)
          .def("enable_all_streams", &pyAsyncReader::enableAllStreams)
          .def("get_enabled_streams", &pyAsyncReader::getEnabledStreams)
          .def("get_all_records_info", &pyAsyncReader::getAllRecordsInfo)
          .def("get_records_info", &pyAsyncReader::getRecordsInfo)
          .def("get_enabled_streams_records_info", &pyAsyncReader::getEnabledStreamsRecordsInfo)
          .def("might_contain_images", &pyAsyncReader::mightContainImages)
          .def("might_contain_audio", &pyAsyncReader::mightContainAudio)
          .def(
              "async_read_record",
              py::overload_cast<const std::string&, const std::string&, int>(
                  &pyAsyncReader::asyncReadRecord))
          .def("async_read_record", py::overload_cast<int>(&pyAsyncReader::asyncReadRecord))
          .def("skip_trailing_blocks", &pyAsyncReader::skipTrailingBlocks)
          .def("get_tags", py::overload_cast<>(&pyAsyncReader::getTags))
          .def("get_tags", py::overload_cast<const std::string&>(&pyAsyncReader::getTags))
          .def("get_max_available_timestamp", &pyAsyncReader::getMaxAvailableTimestamp)
          .def("get_min_available_timestamp", &pyAsyncReader::getMinAvailableTimestamp)
          .def("get_available_record_types", &pyAsyncReader::getAvailableRecordTypes)
          .def("get_available_stream_ids", &pyAsyncReader::getAvailableStreamIds)
          .def("get_timestamp_for_index", &pyAsyncReader::getTimestampForIndex)
          .def("get_records_count", &pyAsyncReader::getRecordsCount)
          .def("get_available_records_size", &pyAsyncReader::getAvailableRecordsSize)
          .def("record_count_by_type_from_stream_id", &pyAsyncReader::recordCountByTypeFromStreamId)
          .def("regenerate_enabled_indices", &pyAsyncReader::regenerateEnabledIndices)
          .def("get_stream_id_for_index", &pyAsyncReader::getStreamIdForIndex)
          .def("get_record_index_by_time", &pyAsyncReader::getRecordIndexByTime)
          .def("get_nearest_record_index_by_time", &pyAsyncReader::getNearestRecordIndexByTime)
          .def("get_timestamp_list_for_indices", &pyAsyncReader::getTimestampListForIndices)
          .def("get_next_index", &pyAsyncReader::getNextIndex)
          .def("get_prev_index", &pyAsyncReader::getPrevIndex);
  auto multiReader =
      py::class_<pyAsyncMultiReader>(m, "AsyncMultiReader")
          .def(py::init<bool>())
          .def("open", py::overload_cast<const std::string&>(&pyAsyncMultiReader::open))
          .def(
              "open", py::overload_cast<const std::vector<std::string>&>(&pyAsyncMultiReader::open))
          .def("close", &pyAsyncMultiReader::close)
          .def("set_encoding", &pyAsyncMultiReader::setEncoding)
          .def("get_encoding", &pyAsyncMultiReader::getEncoding)
          .def(
              "set_image_conversion",
              py::overload_cast<pyvrs::ImageConversion>(&pyAsyncMultiReader::setImageConversion))
          .def(
              "set_image_conversion",
              py::overload_cast<const std::string&, pyvrs::ImageConversion>(
                  &pyAsyncMultiReader::setImageConversion))
          .def(
              "set_image_conversion",
              py::overload_cast<vrs::RecordableTypeId, pyvrs::ImageConversion>(
                  &pyAsyncMultiReader::setImageConversion))
          .def("get_file_chunks", &pyAsyncMultiReader::getFileChunks)
          .def("get_streams", py::overload_cast<>(&pyAsyncMultiReader::getStreams))
          .def(
              "get_streams",
              py::overload_cast<vrs::RecordableTypeId>(&pyAsyncMultiReader::getStreams))
          .def(
              "get_streams",
              py::overload_cast<vrs::RecordableTypeId, const std::string&>(
                  &pyAsyncMultiReader::getStreams))
          .def("find_stream", &pyAsyncMultiReader::findStream)
          .def("get_stream_info", &pyAsyncMultiReader::getStreamInfo)
          .def("enable_stream", py::overload_cast<const string&>(&pyAsyncMultiReader::enableStream))
          .def("enable_streams", &pyAsyncMultiReader::enableStreams)
          .def("enable_streams_by_indexes", &pyAsyncMultiReader::enableStreamsByIndexes)
          .def("enable_all_streams", &pyAsyncMultiReader::enableAllStreams)
          .def("get_enabled_streams", &pyAsyncMultiReader::getEnabledStreams)
          .def("get_all_records_info", &pyAsyncMultiReader::getAllRecordsInfo)
          .def("get_records_info", &pyAsyncMultiReader::getRecordsInfo)
          .def(
              "get_enabled_streams_records_info", &pyAsyncMultiReader::getEnabledStreamsRecordsInfo)
          .def(
              "async_read_record",
              py::overload_cast<const std::string&, const std::string&, int>(
                  &pyAsyncMultiReader::asyncReadRecord))
          .def("async_read_record", py::overload_cast<int>(&pyAsyncMultiReader::asyncReadRecord))
          .def("skip_trailing_blocks", &pyAsyncMultiReader::skipTrailingBlocks)
          .def("get_tags", py::overload_cast<>(&pyAsyncMultiReader::getTags))
          .def("get_tags", py::overload_cast<const std::string&>(&pyAsyncMultiReader::getTags))
          .def("get_max_available_timestamp", &pyAsyncMultiReader::getMaxAvailableTimestamp)
          .def("get_min_available_timestamp", &pyAsyncMultiReader::getMinAvailableTimestamp)
          .def("get_available_record_types", &pyAsyncMultiReader::getAvailableRecordTypes)
          .def("get_available_stream_ids", &pyAsyncMultiReader::getAvailableStreamIds)
          .def("get_timestamp_for_index", &pyAsyncMultiReader::getTimestampForIndex)
          .def("get_records_count", &pyAsyncMultiReader::getRecordsCount)
          .def("get_available_records_size", &pyAsyncMultiReader::getAvailableRecordsSize)
          .def(
              "record_count_by_type_from_stream_id",
              &pyAsyncMultiReader::recordCountByTypeFromStreamId)
          .def("regenerate_enabled_indices", &pyAsyncMultiReader::regenerateEnabledIndices)
          .def("get_stream_id_for_index", &pyAsyncMultiReader::getStreamIdForIndex)
          .def("get_record_index_by_time", &pyAsyncMultiReader::getRecordIndexByTime)
          .def("get_nearest_record_index_by_time", &pyAsyncMultiReader::getNearestRecordIndexByTime)
          .def("get_timestamp_list_for_indices", &pyAsyncMultiReader::getTimestampListForIndices)
          .def("get_next_index", &pyAsyncMultiReader::getNextIndex)
          .def("get_prev_index", &pyAsyncMultiReader::getPrevIndex);

#if IS_VRS_FB_INTERNAL()
  pybind_fbasyncvrsreaders(reader, multiReader);
#endif

  py::class_<AwaitableRecord>(m, "AwaitableRecord")
      .def("__await__", [](const AwaitableRecord& awaitable) {
        py::object loop = py::module_::import("asyncio.events").attr("get_event_loop")();
        py::object fut = loop.attr("create_future")();
        unique_ptr<AsyncJob> job = make_unique<AsyncReadJob>(loop, fut, awaitable.getIndex());
        awaitable.scheduleJob(std::move(job));
        return fut.attr("__await__")();
      });
}

} // namespace pyvrs
