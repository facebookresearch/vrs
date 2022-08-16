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

#include "PyUtils.h"

#define DEFAULT_LOG_CHANNEL "PyUtils"
#include <logging/Log.h>

#include <vrs/helpers/Strings.h>
#include <vrs/os/Utils.h>

namespace {
const char* kUtf8 = "utf-8";
}

namespace pyvrs {

string typeName(const DataPiece* piece, const char* suffix) {
  string name = piece->getElementTypeName();
  // Remove trailing "_t", if any..
  if (name.length() >= 2 && name.back() == 't' && name[name.length() - 2] == '_') {
    name.resize(name.length() - 2);
  }
  return suffix == nullptr ? name : name + suffix;
}

string lowercaseTypeName(Record::Type type) {
  string typeName = toString(type);
  transform(typeName.begin(), typeName.end(), typeName.begin(), ::tolower);
  return typeName;
}

string toupper(const string& s) {
  string t{s};
  transform(t.begin(), t.end(), t.begin(), ::toupper);
  return t;
}

void pyDict_SetItemWithDecRef(PyObject* dict, PyObject* key, PyObject* value) {
  PyDict_SetItem(dict, key, value);
  Py_DECREF(key);
  Py_DECREF(value);
}

PyObject* unicodeDecode(const string& str, const string& encoding, const string& errors) {
  const char* s = str.c_str();
  Py_ssize_t size = str.size();

  if (encoding == "utf-8-safe") {
    // magic encoding name to try utf-8, but not fail if the encoding is not recognized.
    PyObject* decodedStr = PyUnicode_Decode(s, size, kUtf8, errors.c_str());
    if (decodedStr != nullptr) {
      return decodedStr;
    }
    PyErr_Clear();
    // if utf-8 fails, make a safe string, even if it's transformed...
    return pyObject(helpers::make_printable(str));
  }
  PyObject* decodedStr = PyUnicode_Decode(s, size, encoding.c_str(), errors.c_str());
  if (decodedStr != nullptr) {
    return decodedStr;
  }

  stringstream ss;
  ss << "Failed to decode \"" << s << "\" with encoding \"" << encoding << "\".";
  // Try to decode in the few most common encodings so we can give a better hint for how to resolve
  // the issue.
  if (PyUnicode_DecodeASCII(s, size, nullptr)) {
    ss << " Try using \"ascii\" for encoding instead.";
  } else if (PyUnicode_DecodeLatin1(s, size, nullptr)) {
    ss << " Try using \"latin1\" for encoding instead.";
  } else {
    ss << " Encoding is neither \"ascii\" or \"latin1\"";
  }

  XR_LOGE("{}", ss.str());

  PyErr_Clear();
  PyUnicodeDecodeError_Create(encoding.c_str(), s, size, 0, size, ss.str().c_str());
  throw pybind11::error_already_set();
}

} // namespace pyvrs
