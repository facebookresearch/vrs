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

#include "Reader.h"

#include <vrs/os/Platform.h>

#if IS_VRS_OSS_CODE()
#include "AsyncVRSReader.h"
#include "MultiVRSReader.h"
#include "VRSReader.h"
#else
#include "AsyncVRSReader_fb.h"
#include "FilteredFileReader.h"
#include "MultiVRSReader_fb.h"
#include "VRSReader_fb.h"
#endif

namespace pyvrs {
namespace py = pybind11;

void pybind_reader(py::module& m) {
  py::enum_<pyvrs::ImageConversion>(m, "ImageConversion")
      .value("OFF", pyvrs::ImageConversion::Off)
      .value("DECOMPRESS", pyvrs::ImageConversion::Decompress)
      .value("NORMALIZE", pyvrs::ImageConversion::Normalize)
      .value("NORMALIZE_GREY8", pyvrs::ImageConversion::NormalizeGrey8)
      .value("RAW_BUFFER", pyvrs::ImageConversion::RawBuffer)
      .value("RECORD_UNREAD_BYTES_BACKDOOR", pyvrs::ImageConversion::RecordUnreadBytesBackdoor)
      .export_values();

  pybind_vrsreader(m);
  pybind_multivrsreader(m);
  pybind_asyncvrsreaders(m);

#if IS_VRS_FB_INTERNAL()
  pybind_filtered_filereader(m);
#endif
}
} // namespace pyvrs
