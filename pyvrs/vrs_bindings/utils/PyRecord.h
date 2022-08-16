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

#include <vector>

#include <pybind11/pybind11.h>

#include <vrs/IndexRecord.h>
#include <vrs/RecordFormat.h>

#include "PyBuffer.h"

namespace pyvrs {

namespace py = pybind11;
using namespace vrs;
using namespace std;

/// \brief Cache for VRS Record's content.
///
/// When a record is read, RecordCache is used to hold the record content.
struct RecordCache {
  uint32_t recordFormatVersion;
  vector<py::object> datalayoutBlocks;
  vector<ContentBlockBuffer> images;
  vector<ContentBlockBuffer> audioBlocks;
  vector<ContentBlockBuffer> customBlocks;
  vector<ContentBlockBuffer> unsupportedBlocks;

  void clear() {
    recordFormatVersion = 0;
    datalayoutBlocks.clear();
    images.clear();
    audioBlocks.clear();
    customBlocks.clear();
    unsupportedBlocks.clear();
  }
};

/// \brief The VRS Record class
///
/// This class is a VRS record for Python bindings.
/// The fields are exposed as a read only property but the methods required for dict interface is
/// supported so that user can treat PyRecord object as Python Dictionary to iterate on.
struct PyRecord {
  explicit PyRecord(const IndexRecord::RecordInfo& info, int32_t recordIndex_, RecordCache& record);

  int32_t recordIndex;
  string recordType;
  double recordTimestamp;
  string streamId;

  uint32_t recordFormatVersion;
  vector<py::object> datalayoutBlocks;
  vector<ContentBlockBuffer> imageBlocks;
  vector<ContentBlockBuffer> audioBlocks;
  vector<ContentBlockBuffer> customBlocks;
  vector<ContentBlockBuffer> unsupportedBlocks;

  vector<PyAudioContentBlockSpec> audioSpecs;
  vector<PyContentBlock> customBlockSpecs;
  vector<PyImageContentBlockSpec> imageSpecs;

  /// members and methods to set up dictionary interface.
  void initMap();
  map<string, py::object> map;
};

/// Binds methods and classes for PyRecord.
void pybind_record(py::module& m);
} // namespace pyvrs
