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

// Includes needed for bindings (including marshalling STL containers)
#include <pybind11/pybind11.h>

#include <vrs/os/Platform.h>

#include "VrsBindings.h"
#include "reader/Reader.h"
#include "utils/Utils.h"

#if IS_VRS_FB_INTERNAL()
#include "archive/Archive.h"
#include "fb/FbInternal.h"
#include "filter/Filter.h" // Disable filter internally until AsyncImageFilter is reworked.
#include "writer/Writer.h"
#endif

#ifndef PYBIND_MODULE_NAME
#define PYBIND_MODULE_NAME vrsbindings
#endif

namespace py = pybind11;

PYBIND11_MODULE(PYBIND_MODULE_NAME, m) {
  m.doc() = R"pbdoc(
          Python bindings for VRS
          ---------------------------------
          .. currentmodule:: vrsbindings
          .. autosummary::
            :toctree: _generate
      )pbdoc";

  // Register submodules.
  pyvrs::pybind_reader(m);
  pyvrs::pybind_utils(m);

#if IS_VRS_FB_INTERNAL()
  pyvrs::pybind_filter(m); // Disable filter internally until AsyncImageFilter is reworked.
  pyvrs::pybind_writer(m);
  pyvrs::pybind_archive(m);
  pyvrs::pybind_fbinternal(m);
#endif

  // We want to call pyvrs::uninitVrsBindings to clean up when this module becomes no longer needed.
  auto atexit = py::module_::import("atexit");
  atexit.attr("register")(py::cpp_function([]() { pyvrs::uninitVrsBindings(); }));

#ifdef VERSION_INFO
  m.attr("__version__") = VERSION_INFO;
#else
  m.attr("__version__") = "dev";
#endif
}
