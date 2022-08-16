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

#include <vrs/DataPieces.h>

#include "../utils/PyUtils.h"

// This file only exists to isolate the factory code of VRSReader.cpp in a separate file.
// The factory aspects of this machinery mean that we actually need to be able to resolve names
// without namespace specifications, hence these "using namespace" declarations in this file.
// If we remove those the stringifying macros won't generate the correct names.

namespace {

using namespace std;
using namespace pyvrs;
using namespace vrs;

using PyObjector = function<void(PyObject* dic, const DataPiece* piece)>;
using PyObjectorString =
    function<void(PyObject* dic, const DataPiece* piece, const string& encoding)>;

class PyObjectorRegistry : public map<string, PyObjector> {
 public:
  void map(PyObject* dic, const DataPiece* piece) {
    auto iter = find(piece->getElementTypeName());
    if (iter != end()) {
      iter->second(dic, piece);
    }
  }
};

class PyObjectorStringRegistry : public map<string, PyObjectorString> {
 public:
  void map(PyObject* dic, const DataPiece* piece, const string& encoding) {
    auto iter = find(piece->getElementTypeName());
    if (iter != end()) {
      iter->second(dic, piece, encoding);
    }
  }
};

template <class T>
void DataPieceValuePyObjector(PyObject* dic, const DataPiece* piece) {
  if (piece->isAvailable()) {
    string label = piece->getLabel();
    string type = typeName(piece);
    T v = reinterpret_cast<const DataPieceValue<T>*>(piece)->get();
    pyDict_SetItemWithDecRef(dic, Py_BuildValue("(s,s)", label.c_str(), type.c_str()), pyObject(v));
  }
}

static PyObjectorRegistry& getDataPieceValuePyObjectorRegistry() {
  static PyObjectorRegistry sInstance;
  return sInstance;
}

template <class T>
struct DataPieceValuePyObjectorRegistrerer {
  explicit DataPieceValuePyObjectorRegistrerer() {
    getDataPieceValuePyObjectorRegistry()[getTypeName<T>()] = DataPieceValuePyObjector<T>;
  }
};

template <class T>
void DataPieceArrayPyObjector(PyObject* dic, const DataPiece* piece) {
  if (piece->isAvailable()) {
    string label = piece->getLabel();
    string type = typeName(piece, "_array");
    vector<T> values;
    reinterpret_cast<const DataPieceArray<T>*>(piece)->get(values);
    PyObject* list = PyList_New(static_cast<Py_ssize_t>(values.size()));
    for (size_t i = 0; i < values.size(); i++) {
      PyList_SetItem(list, static_cast<Py_ssize_t>(i), pyObject(values[i]));
    }
    pyDict_SetItemWithDecRef(dic, Py_BuildValue("(s,s)", label.c_str(), type.c_str()), list);
  }
}

static PyObjectorRegistry& getDataPieceArrayPyObjectorRegistry() {
  static PyObjectorRegistry sInstance;
  return sInstance;
}

template <class T>
struct DataPieceArrayPyObjectorRegistrerer {
  explicit DataPieceArrayPyObjectorRegistrerer() {
    getDataPieceArrayPyObjectorRegistry()[getTypeName<T>()] = DataPieceArrayPyObjector<T>;
  }
};

template <class T>
void DataPieceVectorPyObjector(PyObject* dic, const DataPiece* piece) {
  if (piece->isAvailable()) {
    string label = piece->getLabel();
    string type = typeName(piece, "_vector");
    vector<T> values;
    reinterpret_cast<const DataPieceVector<T>*>(piece)->get(values);
    PyObject* list = PyList_New(static_cast<Py_ssize_t>(values.size()));
    for (size_t i = 0; i < values.size(); i++) {
      PyList_SetItem(list, static_cast<Py_ssize_t>(i), pyObject(values[i]));
    }
    pyDict_SetItemWithDecRef(dic, Py_BuildValue("(s,s)", label.c_str(), type.c_str()), list);
  }
}

static PyObjectorRegistry& getDataPieceVectorPyObjectorRegistry() {
  static PyObjectorRegistry sInstance;
  return sInstance;
}

template <class T>
struct DataPieceVectorPyObjectorRegistrerer {
  explicit DataPieceVectorPyObjectorRegistrerer() {
    getDataPieceVectorPyObjectorRegistry()[getTypeName<T>()] = DataPieceVectorPyObjector<T>;
  }
};

template <class T>
void DataPieceStringMapPyObjector(PyObject* dic, const DataPiece* piece, const string& encoding) {
  if (piece->isAvailable()) {
    string label = piece->getLabel();
    string type = typeName(piece, "_string_map");
    map<string, T> values;
    reinterpret_cast<const DataPieceStringMap<T>*>(piece)->get(values);
    PyObject* newDic = PyDict_New();
    string errors;
    for (const auto& iter : values) {
      pyDict_SetItemWithDecRef(
          newDic, unicodeDecode(iter.first, encoding, errors), pyObject(iter.second));
    }
    pyDict_SetItemWithDecRef(dic, Py_BuildValue("(s,s)", label.c_str(), type.c_str()), newDic);
  }
}

static PyObjectorStringRegistry& getDataPieceStringMapPyObjectorRegistry() {
  static PyObjectorStringRegistry sInstance;
  return sInstance;
}

template <class T>
struct DataPieceStringMapPyObjectorRegistrerer {
  explicit DataPieceStringMapPyObjectorRegistrerer() {
    getDataPieceStringMapPyObjectorRegistry()[getTypeName<T>()] = DataPieceStringMapPyObjector<T>;
  }
};

#define DEFINE_DATA_PIECE(DATA_PIECE_TYPE, TEMPLATE_TYPE)                 \
  static DataPiece##DATA_PIECE_TYPE##PyObjectorRegistrerer<TEMPLATE_TYPE> \
      sDataPiece##DATA_PIECE_TYPE##PyObjectorRegistrerer_##TEMPLATE_TYPE;

#define DEFINE_DATA_PIECE_TYPE(TEMPLATE_TYPE) \
  DEFINE_DATA_PIECE(Value, TEMPLATE_TYPE)     \
  DEFINE_DATA_PIECE(Array, TEMPLATE_TYPE)     \
  DEFINE_DATA_PIECE(Vector, TEMPLATE_TYPE)    \
  DEFINE_DATA_PIECE(StringMap, TEMPLATE_TYPE)

DEFINE_DATA_PIECE_TYPE(Bool)
DEFINE_DATA_PIECE_TYPE(char)
DEFINE_DATA_PIECE_TYPE(double)
DEFINE_DATA_PIECE_TYPE(float)
DEFINE_DATA_PIECE_TYPE(int64_t)
DEFINE_DATA_PIECE_TYPE(uint64_t)
DEFINE_DATA_PIECE_TYPE(int32_t)
DEFINE_DATA_PIECE_TYPE(uint32_t)
DEFINE_DATA_PIECE_TYPE(int16_t)
DEFINE_DATA_PIECE_TYPE(uint16_t)
DEFINE_DATA_PIECE_TYPE(int8_t)
DEFINE_DATA_PIECE_TYPE(uint8_t)

DEFINE_DATA_PIECE_TYPE(Point2Dd)
DEFINE_DATA_PIECE_TYPE(Point2Df)
DEFINE_DATA_PIECE_TYPE(Point2Di)
DEFINE_DATA_PIECE_TYPE(Point3Dd)
DEFINE_DATA_PIECE_TYPE(Point3Df)
DEFINE_DATA_PIECE_TYPE(Point3Di)
DEFINE_DATA_PIECE_TYPE(Point4Dd)
DEFINE_DATA_PIECE_TYPE(Point4Df)
DEFINE_DATA_PIECE_TYPE(Point4Di)

DEFINE_DATA_PIECE_TYPE(Matrix3Dd)
DEFINE_DATA_PIECE_TYPE(Matrix3Df)
DEFINE_DATA_PIECE_TYPE(Matrix3Di)
DEFINE_DATA_PIECE_TYPE(Matrix4Dd)
DEFINE_DATA_PIECE_TYPE(Matrix4Df)
DEFINE_DATA_PIECE_TYPE(Matrix4Di)

DEFINE_DATA_PIECE(Array, string)
DEFINE_DATA_PIECE(Vector, string)
DEFINE_DATA_PIECE(StringMap, string)

} // namespace
