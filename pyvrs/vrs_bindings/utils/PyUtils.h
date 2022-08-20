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

#include <sstream>
#include <string>

#include <pybind11/pybind11.h>

#include <vrs/DataPieces.h>
#include <vrs/RecordFormat.h>

namespace pyvrs {

using namespace std;
using namespace vrs;
namespace py = pybind11;

string typeName(const DataPiece* piece, const char* suffix = nullptr);

string lowercaseTypeName(Record::Type type);

string toupper(const string& s);

/// Convinent methods for pybind layer.
/// PyDict_SetItem doesn't decrement the key and the value, this function decrease the reference
/// count for key & value as well.
void pyDict_SetItemWithDecRef(PyObject* dict, PyObject* key, PyObject* value);

PyObject* unicodeDecode(const string& str, const string& encoding, const string& errors);

/// pyWrap converts PyObject -> py::object
inline py::object pyWrap(PyObject* object) {
  return py::reinterpret_steal<py::object>(object);
}

/// pyObject converts C++ object into PyObject.
template <class T>
PyObject* pyObject(const T&);

template <>
inline PyObject* pyObject<Bool>(const Bool& v) {
  PyObject* result = v.operator bool() ? Py_True : Py_False;
  Py_INCREF(result);
  return result;
}

template <>
inline PyObject* pyObject<char>(const char& v) {
  return PyBytes_FromStringAndSize(&v, 1);
}

template <>
inline PyObject* pyObject<uint8_t>(const uint8_t& v) {
  return PyLong_FromUnsignedLong(v);
}

template <>
inline PyObject* pyObject<int8_t>(const int8_t& v) {
  return PyLong_FromLong(v);
}

template <>
inline PyObject* pyObject<uint16_t>(const uint16_t& v) {
  return PyLong_FromUnsignedLong(v);
}

template <>
inline PyObject* pyObject<int16_t>(const int16_t& v) {
  return PyLong_FromLong(v);
}

template <>
inline PyObject* pyObject<uint32_t>(const uint32_t& v) {
  return PyLong_FromUnsignedLong(v);
}

template <>
inline PyObject* pyObject<int32_t>(const int32_t& v) {
  return PyLong_FromLong(v);
}

template <>
inline PyObject* pyObject<uint64_t>(const uint64_t& v) {
  return PyLong_FromUnsignedLongLong(v);
}

template <>
inline PyObject* pyObject<int64_t>(const int64_t& v) {
  return PyLong_FromLongLong(v);
}

template <>
inline PyObject* pyObject<double>(const double& d) {
  return PyFloat_FromDouble(d);
}

template <>
inline PyObject* pyObject<float>(const float& f) {
  return PyFloat_FromDouble(static_cast<double>(f));
}

template <>
inline PyObject* pyObject<Point2Df>(const Point2Df& p) {
  return Py_BuildValue("(d,d)", static_cast<double>(p.x()), static_cast<double>(p.y()));
}

template <>
inline PyObject* pyObject<Point2Dd>(const Point2Dd& p) {
  return Py_BuildValue("(d,d)", p.x(), p.y());
}

template <>
inline PyObject* pyObject<Point2Di>(const Point2Di& p) {
  return Py_BuildValue("(i,i)", p.x(), p.y());
}

template <>
inline PyObject* pyObject<Point3Df>(const Point3Df& p) {
  return Py_BuildValue(
      "(d,d,d)",
      static_cast<double>(p.x()),
      static_cast<double>(p.y()),
      static_cast<double>(p.z()));
}

template <>
inline PyObject* pyObject<Point3Dd>(const Point3Dd& p) {
  return Py_BuildValue("(d,d,d)", p.x(), p.y(), p.z());
}

template <>
inline PyObject* pyObject<Point3Di>(const Point3Di& p) {
  return Py_BuildValue("(i,i,i)", p.x(), p.y(), p.z());
}

template <>
inline PyObject* pyObject<Point4Df>(const Point4Df& p) {
  return Py_BuildValue(
      "(d,d,d,d)",
      static_cast<double>(p.x()),
      static_cast<double>(p.y()),
      static_cast<double>(p.z()),
      static_cast<double>(p.w()));
}

template <>
inline PyObject* pyObject<Point4Dd>(const Point4Dd& p) {
  return Py_BuildValue("(d,d,d,d)", p.x(), p.y(), p.z(), p.w());
}

template <>
inline PyObject* pyObject<Point4Di>(const Point4Di& p) {
  return Py_BuildValue("(i,i,i,i)", p.x(), p.y(), p.z(), p.w());
}

template <>
inline PyObject* pyObject<Matrix3Df>(const Matrix3Df& p) {
  return Py_BuildValue(
      "((d,d,d),(d,d,d),(d,d,d))",
      static_cast<double>(p[0][0]),
      static_cast<double>(p[0][1]),
      static_cast<double>(p[0][2]),
      static_cast<double>(p[1][0]),
      static_cast<double>(p[1][1]),
      static_cast<double>(p[1][2]),
      static_cast<double>(p[2][0]),
      static_cast<double>(p[2][1]),
      static_cast<double>(p[2][2]));
}

template <>
inline PyObject* pyObject<Matrix3Dd>(const Matrix3Dd& p) {
  return Py_BuildValue(
      "((d,d,d),(d,d,d),(d,d,d))",
      p[0][0],
      p[0][1],
      p[0][2],
      p[1][0],
      p[1][1],
      p[1][2],
      p[2][0],
      p[2][1],
      p[2][2]);
}

template <>
inline PyObject* pyObject<Matrix3Di>(const Matrix3Di& p) {
  return Py_BuildValue(
      "((i,i,i),(i,i,i),(i,i,i))",
      p[0][0],
      p[0][1],
      p[0][2],
      p[1][0],
      p[1][1],
      p[1][2],
      p[2][0],
      p[2][1],
      p[2][2]);
}

template <>
inline PyObject* pyObject<Matrix4Df>(const Matrix4Df& p) {
  return Py_BuildValue(
      "((d,d,d,d),(d,d,d,d),(d,d,d,d),(d,d,d,d))",
      static_cast<double>(p[0][0]),
      static_cast<double>(p[0][1]),
      static_cast<double>(p[0][2]),
      static_cast<double>(p[0][3]),
      static_cast<double>(p[1][0]),
      static_cast<double>(p[1][1]),
      static_cast<double>(p[1][2]),
      static_cast<double>(p[1][3]),
      static_cast<double>(p[2][0]),
      static_cast<double>(p[2][1]),
      static_cast<double>(p[2][2]),
      static_cast<double>(p[2][3]),
      static_cast<double>(p[3][0]),
      static_cast<double>(p[3][1]),
      static_cast<double>(p[3][2]),
      static_cast<double>(p[3][3]));
}

template <>
inline PyObject* pyObject<Matrix4Dd>(const Matrix4Dd& p) {
  return Py_BuildValue(
      "((d,d,d,d),(d,d,d,d),(d,d,d,d),(d,d,d,d))",
      p[0][0],
      p[0][1],
      p[0][2],
      p[0][3],
      p[1][0],
      p[1][1],
      p[1][2],
      p[1][3],
      p[2][0],
      p[2][1],
      p[2][2],
      p[2][3],
      p[3][0],
      p[3][1],
      p[3][2],
      p[3][3]);
}

template <>
inline PyObject* pyObject<Matrix4Di>(const Matrix4Di& p) {
  return Py_BuildValue(
      "((i,i,i,i),(i,i,i,i),(i,i,i,i),(i,i,i,i))",
      p[0][0],
      p[0][1],
      p[0][2],
      p[0][3],
      p[1][0],
      p[1][1],
      p[1][2],
      p[1][3],
      p[2][0],
      p[2][1],
      p[2][2],
      p[2][3],
      p[3][0],
      p[3][1],
      p[3][2],
      p[3][3]);
}

template <>
inline PyObject* pyObject(const string& s) {
  return Py_BuildValue("s", s.c_str());
}

inline PyObject* pyObject(const char* s) {
  return Py_BuildValue("s", s);
}

#define PYWRAP(val) pyWrap(pyObject(val))
#define DEF_GETITEM(m, class)                                    \
  m.def("__getitem__", [](class& dict, const std::string& key) { \
    dict.initAttributesMap();                                    \
    try {                                                        \
      return dict.attributesMap.at(key);                         \
    } catch (const std::out_of_range&) {                         \
      throw py::key_error("key '" + key + "' does not exist.");  \
    }                                                            \
  });

#define DEF_LEN(m, class)                 \
  m.def(                                  \
      "__len__",                          \
      [](class& dict) {                   \
        dict.initAttributesMap();         \
        return dict.attributesMap.size(); \
      },                                  \
      py::keep_alive<0, 1>());
#define DEF_ITER(m, class)                                                                  \
  m.def(                                                                                    \
      "__iter__",                                                                           \
      [](class& dict) {                                                                     \
        dict.initAttributesMap();                                                           \
        return py::make_key_iterator(dict.attributesMap.begin(), dict.attributesMap.end()); \
      },                                                                                    \
      py::keep_alive<0, 1>());
#define DEF_ITEM(m, class)                                                              \
  m.def(                                                                                \
      "items",                                                                          \
      [](class& dict) {                                                                 \
        dict.initAttributesMap();                                                       \
        return py::make_iterator(dict.attributesMap.begin(), dict.attributesMap.end()); \
      },                                                                                \
      py::keep_alive<0, 1>());
#define DEF_DICT_FUNC(m, class) \
  DEF_GETITEM(m, class)         \
  DEF_LEN(m, class)             \
  DEF_ITEM(m, class)

} // namespace pyvrs
