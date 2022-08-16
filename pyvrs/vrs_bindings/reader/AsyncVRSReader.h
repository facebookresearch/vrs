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
#define PY_SSIZE_T_CLEAN
#include <Python.h>

#include <memory>
#include <string>

#include <pybind11/pybind11.h>

#include <vrs/RecordFileReader.h>
#include <vrs/RecordFormat.h>
#include <vrs/helpers/JobQueue.h>

#include "MultiVRSReader.h"
#include "VRSReader.h"

namespace pyvrs {
namespace py = pybind11;
using namespace vrs;
using namespace std;

class AsyncVRSReader;
class AsyncMultiVRSReader;

/// \brief The base class for asynchronous job
/// This class takes asyncio's event loop & future in constructor so that we can later call
/// loop.call_soon_threadsafe(future.set_result(<result>)) to set the result into future.
class AsyncJob {
 public:
  AsyncJob() = default;
  virtual ~AsyncJob() = default;

  AsyncJob(py::object& loop, py::object& future) : loop_{loop}, future_{future} {}

  AsyncJob(const AsyncJob&) = default;
  AsyncJob& operator=(const AsyncJob&) = default;

  virtual void performJob(AsyncVRSReader& reader) = 0;
  virtual void performJob(AsyncMultiVRSReader& reader) = 0;

 protected:
  py::object loop_, future_;
};

/// \brief Asynchronous job class for reading a record
class AsyncReadJob : public AsyncJob {
 public:
  AsyncReadJob(py::object& loop, py::object& fut, uint32_t index)
      : AsyncJob(loop, fut), index_(index) {}
  ~AsyncReadJob() override {}

  void performJob(AsyncVRSReader& reader) override;
  void performJob(AsyncMultiVRSReader& reader) override;

 private:
  uint32_t index_;
};

using AsyncJobQueue = JobQueue<std::unique_ptr<AsyncJob>>;

/// \brief Python awaitable record
/// This class only exposes __await__ method to Python which does
/// - Obtain Python event loop via asyncio.events.get_event_loop
/// - Create future to return to Python side via loop.create_future
/// - Create AsyncReadJob object with loop, future and record index
/// - Send a job to AsyncReader's background thread
/// - Call future.__await__() and Python side waits until set_result will be called by AsyncReader
class AwaitableRecord {
 public:
  AwaitableRecord(uint32_t index, AsyncJobQueue& queue) : index_{index}, queue_{queue} {}

  void scheduleJob(std::unique_ptr<AsyncJob>&& job) const {
    queue_.sendJob(move(job));
  }

  uint32_t getIndex() const {
    return index_;
  }

 private:
  uint32_t index_;
  AsyncJobQueue& queue_;
};

/// \brief The async VRSReader class
/// This class extends VRSReader and adds asynchronous APIs to read records.
/// AsyncVRSReader spawns a background thread to process the async jobs.
class AsyncVRSReader : public VRSReader {
 public:
  explicit AsyncVRSReader(bool autoReadConfigurationRecord)
      : VRSReader(autoReadConfigurationRecord),
        asyncThread_(&AsyncVRSReader::asyncThreadActivity, this) {}

  ~AsyncVRSReader() override;

  /// Read a stream's record, by record type & index.
  /// @param streamId: VRS stream id to read.
  /// @param recordType: record type to read, or "any".
  /// @param index: the index of the record to read.
  /// @return AwaitableRecord: by calling await, caller will receive the same data as calling
  /// readRecord
  AwaitableRecord asyncReadRecord(const string& streamId, const string& recordType, int index);

  /// Read a specifc record, by index.
  /// @param index: a record index.
  /// @return AwaitableRecord: by calling await, caller will receive the same data as calling
  /// readRecord
  AwaitableRecord asyncReadRecord(int index);

  /// Background thread function that waits for asynchronous job
  void asyncThreadActivity();

 private:
  AsyncJobQueue workerQueue_;
  atomic<bool> shouldEndAsyncThread_ = false;
  thread asyncThread_;
};

/// \brief The async MultiVRSReader class
/// This class extends VRSReader and adds asynchronous APIs to read records.
/// AsyncVRSReader spawns a background thread to process the async jobs.
class AsyncMultiVRSReader : public MultiVRSReader {
 public:
  explicit AsyncMultiVRSReader(bool autoReadConfigurationRecord)
      : MultiVRSReader(autoReadConfigurationRecord),
        asyncThread_(&AsyncMultiVRSReader::asyncThreadActivity, this) {}

  ~AsyncMultiVRSReader() override;

  /// Read a stream's record, by record type & index.
  /// @param streamId: VRS stream id to read.
  /// @param recordType: record type to read, or "any".
  /// @param index: the index of the record to read.
  /// @return AwaitableRecord: by calling await, caller will receive the same data as calling
  /// readRecord
  AwaitableRecord asyncReadRecord(const string& streamId, const string& recordType, int index);

  /// Read a specifc record, by index.
  /// @param index: a record index.
  /// @return AwaitableRecord: by calling await, caller will receive the same data as calling
  /// readRecord
  AwaitableRecord asyncReadRecord(int index);

  /// Background thread function that waits for asynchronous job
  void asyncThreadActivity();

 private:
  AsyncJobQueue workerQueue_;
  atomic<bool> shouldEndAsyncThread_ = false;
  thread asyncThread_;
};

/// Binds methods and classes for AsyncVRSReader.
void pybind_asyncvrsreaders(py::module& m);
} // namespace pyvrs
