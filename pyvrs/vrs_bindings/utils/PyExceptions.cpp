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

#include "PyExceptions.h"

namespace pyvrs {

void pybind_exception(py::module& m) {
  // Register custom exceptions
  static py::exception<pyvrs::TimestampNotFoundError> timestampNotFoundError(
      m, "TimestampNotFoundError");
  static py::exception<pyvrs::StreamNotFoundError> streamNotFoundError(m, "StreamNotFoundError");
  py::register_exception_translator([](std::exception_ptr p) {
    try {
      if (p) {
        std::rethrow_exception(p);
      }
    } catch (const pyvrs::TimestampNotFoundError& e) {
      timestampNotFoundError(e.what());
    } catch (const pyvrs::StreamNotFoundError& e) {
      streamNotFoundError(e.what());
    }
  });
}

} // namespace pyvrs
