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

#include "Utils.h"

#include <vrs/os/Platform.h>
#include <vrs/utils/Validation.h>

#include "../VrsBindings.h"
#include "PyBuffer.h"
#include "PyExceptions.h"
#include "PyRecord.h"

namespace pyvrs {
namespace py = pybind11;
#if IS_VRS_OSS_CODE()
static string recordsChecksum(const string& path, bool showProgress) {
  initVrsBindings();
  return vrs::utils::recordsChecksum(path, showProgress);
}

static string verbatimChecksum(const string& path, bool showProgress) {
  initVrsBindings();
  return vrs::utils::verbatimChecksum(path, showProgress);
}

void pybind_utils(py::module& m) {
  py::enum_<vrs::CompressionPreset>(m, "CompressionPreset")
      .value("NONE", vrs::CompressionPreset::None)
      .value("LZ4_FAST", vrs::CompressionPreset::Lz4Fast)
      .value("LZ4_TIGHT", vrs::CompressionPreset::Lz4Tight)
      .value("ZSTD_FAST", vrs::CompressionPreset::ZstdFast)
      .value("ZSTD_LIGHT", vrs::CompressionPreset::ZstdLight)
      .value("ZSTD_MEDIUM", vrs::CompressionPreset::ZstdMedium)
      .value("ZSTD_TIGHT", vrs::CompressionPreset::ZstdTight)
      .value("ZSTD_MAX", vrs::CompressionPreset::ZstdMax)
      .value("DEFAULT", vrs::CompressionPreset::Default)
      .export_values();
  m.def("records_checksum", &recordsChecksum, "Calculate a VRS file's logical checksum");
  m.def("verbatim_checksum", &verbatimChecksum, "Calculate a file's checksum");
  pybind_exception(m);
  pybind_record(m);
  pybind_buffer(m);
}
#endif
} // namespace pyvrs
