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

#include <cstdint>
#include <type_traits>

namespace vrs {

using std::enable_if;
using std::size_t;

#pragma pack(push, 1)

/// \brief Substitude for bool POD type, which can be used safely in DataPiece templates.
///
/// Shamelessly fake bool class, to workaround vector<bool> NOT being a regular container.
/// Should behave like a bool, except that vector<Bool> will be a regular vector.
/// vector<bool> has no data() method, which means it can't be used with DataPiece template code.
/// vector<Bool> does behave like other vector<T> classes. Yes, it's silly.
/// https://stackoverflow.com/questions/17794569/why-isnt-vectorbool-a-stl-container
class Bool {
 public:
  Bool(bool value = false) : value_(value) {}
  Bool& operator=(bool value) {
    value_ = value;
    return *this;
  }
  operator bool() const {
    return value_;
  }
  const bool* operator&() const {
    return &value_;
  }
  bool* operator&() {
    return &value_;
  }
  bool operator==(const Bool& rhs) const {
    return value_ == rhs.value_;
  }
  bool operator==(bool rhs) const {
    return value_ == rhs;
  }
  bool operator!=(const Bool& rhs) const {
    return value_ != rhs.value_;
  }
  bool operator!=(bool rhs) const {
    return value_ != rhs;
  }

 private:
  bool value_;
};

/// \brief POD type for of 2, 3 and 4 dimensions points, each for either int32_t, float or double.
///
/// Note how the coordinates can be accessed using x(), y(), z() and w() convenience methods, but
/// that the z() and w() methods are only available for larger dimensions.
template <typename T, size_t N>
struct PointND {
  using type = T;
  static constexpr size_t kSize = N;

  PointND() : dim{} {}
  PointND(const T arr[N]) {
    operator=(arr);
  }

  /// 2D point constructor
  template <typename Z = enable_if<(N == 2), T>>
  PointND(T x, T y) : dim{x, y} {}

  /// 3D point constructor
  template <typename Z = enable_if<(N == 3), T>>
  PointND(T x, T y, T z) : dim{x, y, z} {}

  /// 4D point constructor
  template <typename W = enable_if<(N == 4), T>>
  PointND(T x, T y, T z, T w) : dim{x, y, z, w} {}

  bool operator==(const PointND<T, N>& rhs) const {
    for (size_t s = 0; s < N; s++) {
      if (dim[s] != rhs.dim[s]) {
        return false;
      }
    }
    return true;
  }
  bool operator!=(const PointND<T, N>& rhs) const {
    return !operator==(rhs);
  }
  void operator=(const T rhs[N]) {
    for (size_t s = 0; s < N; s++) {
      dim[s] = rhs[s];
    }
  }

  // Convenience aliases for the dimensions that actually exist
  T& x() {
    return dim[0];
  }
  const T& x() const {
    return dim[0];
  }
  T& y() {
    return dim[1];
  }
  const T& y() const {
    return dim[1];
  }
  template <typename Z = enable_if<(N > 2), T>>
  T& z() {
    return dim[2];
  }
  template <typename Z = enable_if<(N > 2), T>>
  const T& z() const {
    return dim[2];
  }
  template <typename W = enable_if<(N > 3), T>>
  T& w() {
    return dim[3];
  }
  template <typename W = enable_if<(N > 3), T>>
  const T& w() const {
    return dim[3];
  }

  T& operator[](size_t n) {
    return dim[n];
  }
  const T& operator[](size_t n) const {
    return dim[n];
  }

  T dim[N];
};

/// 2D double point class.
using Point2Dd = PointND<double, 2>;
/// 2D float point class.
using Point2Df = PointND<float, 2>;
/// 2D int32_t point class.
using Point2Di = PointND<int32_t, 2>;

/// 3D double point class.
using Point3Dd = PointND<double, 3>;
/// 3D float point class.
using Point3Df = PointND<float, 3>;
/// 3D int32_t point class.
using Point3Di = PointND<int32_t, 3>;

/// 4D double point class.
using Point4Dd = PointND<double, 4>;
/// 4D float point class.
using Point4Df = PointND<float, 4>;
/// 4D int32_t point class.
using Point4Di = PointND<int32_t, 4>;

/// Class to represent matrices of 3 and 4 dimensions, each for either int32_t, float or double.
template <typename T, size_t N>
struct MatrixND {
  using type = T;
  static constexpr size_t kMatrixSize = N;

  MatrixND() {}
  MatrixND(const T arr[N][N]) {
    operator=(arr);
  }

  PointND<T, N>& operator[](size_t n) {
    return points[n];
  }
  const PointND<T, N>& operator[](size_t n) const {
    return points[n];
  }

  bool operator==(const MatrixND<T, N>& rhs) const {
    for (size_t s = 0; s < N; s++) {
      if (points[s] != rhs[s]) {
        return false;
      }
    }
    return true;
  }
  bool operator!=(const MatrixND<T, N>& rhs) const {
    return !operator==(rhs);
  }
  void operator=(const T rhs[N][N]) {
    for (size_t s = 0; s < N; s++) {
      points[s] = rhs[s];
    }
  }

  PointND<T, N> points[N];
};

/// 2D double matrix class.
using Matrix2Dd = MatrixND<double, 2>;
/// 2D float matrix class.
using Matrix2Df = MatrixND<float, 2>;
/// 2D int32_t matrix class.
using Matrix2Di = MatrixND<int32_t, 2>;

/// 3D double matrix class.
using Matrix3Dd = MatrixND<double, 3>;
/// 3D float matrix class.
using Matrix3Df = MatrixND<float, 3>;
/// 3D int32_t matrix class.
using Matrix3Di = MatrixND<int32_t, 3>;

/// 4D double matrix class.
using Matrix4Dd = MatrixND<double, 4>;
/// 4D float matrix class.
using Matrix4Df = MatrixND<float, 4>;
/// 4D int32_t matrix class.
using Matrix4Di = MatrixND<int32_t, 4>;

#pragma pack(pop)

} // namespace vrs
