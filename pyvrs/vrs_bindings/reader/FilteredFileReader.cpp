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

#include "FilteredFileReader.h"

#include "../VrsBindings.h"
#include "../utils/PyUtils.h"

namespace py = pybind11;

namespace pyvrs {

using namespace vrs::utils;

FilteredFileReader::FilteredFileReader(const std::string& filePath) {
  initVrsBindings();
  filteredReader_.setSource(filePath);
  filteredReader_.openFile();
}

void FilteredFileReader::after(double minTime, bool isRelativeMinTime) {
  filteredReader_.setMinTime(minTime, isRelativeMinTime);
}

void FilteredFileReader::before(double maxTime, bool isRelativeMaxTime) {
  filteredReader_.setMaxTime(maxTime, isRelativeMaxTime);
}

void FilteredFileReader::range(
    double minTime,
    double maxTime,
    bool isRelativeMinTime,
    bool isRelativeMaxTime) {
  filteredReader_.setMinTime(minTime, isRelativeMinTime);
  filteredReader_.setMaxTime(maxTime, isRelativeMaxTime);
}

vrs::utils::FilteredFileReader& FilteredFileReader::getFilteredReader() {
  filteredReader_.applyFilters(filters_);
  return filteredReader_;
}

void pybind_filtered_filereader(py::module& m) {
  auto filteredReader = py::class_<pyvrs::FilteredFileReader, std::shared_ptr<FilteredFileReader>>(
                            m, "FilteredFileReader")
                            .def(py::init<const std::string&>())
                            .def("after", &pyvrs::FilteredFileReader::after)
                            .def("before", &pyvrs::FilteredFileReader::before)
                            .def("range", &pyvrs::FilteredFileReader::range);

#if IS_VRS_FB_INTERNAL()
  pybind_fbfiltered_filereader(filteredReader);
#endif
}

} // namespace pyvrs
