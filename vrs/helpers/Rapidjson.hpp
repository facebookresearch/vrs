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

#include <cmath>
#include <cstring>

#include <map>
#include <type_traits>
#include <vector>

#include <logging/Checks.h>

#include <vrs/DataPieces.h>
#include <vrs/helpers/Serialization.h>

namespace vrs {

using std::is_floating_point;
using std::is_integral;
using std::is_same;
using std::is_signed;
using std::max;
using std::string;

/// rapidjson::Document's default MemoryPoolAllocator crashes on some platforms
/// as documented in https://github.com/cocos2d/cocos2d-x/issues/16492
using JUtf8Encoding = vrs_rapidjson::UTF8<>;
using JCrtAllocator = vrs_rapidjson::CrtAllocator;
using JDocument = vrs_rapidjson::GenericDocument<JUtf8Encoding, JCrtAllocator>;
using JValue = vrs_rapidjson::GenericValue<JUtf8Encoding, JCrtAllocator>;
using JStringRef = vrs_rapidjson::GenericStringRef<char>;

static inline JStringRef jStringRef(const char* str) {
  return JStringRef(str, strlen(str));
}
static inline JStringRef jStringRef(const string& str) {
  return JStringRef(str.c_str(), str.size());
}
template <class T>
static inline void jParse(JDocument& document, const T& str) {
  document.Parse(str.data(), str.size());
}

/// Helper class to generate json messages using RapidJson.
/// For use by VRS only.
/// @internal
struct JsonWrapper {
  explicit JsonWrapper(JDocument& doc) : value{doc}, alloc{doc.GetAllocator()} {
    doc.SetObject();
  }
  JsonWrapper(JValue& v, JDocument::AllocatorType& a) : value{v}, alloc{a} {}

  JValue& value;
  JDocument::AllocatorType& alloc;

  template <typename T>
  inline JValue jValue(const T& v) {
    return JValue(v);
  }

  template <typename T>
  inline JValue jValue(const std::vector<T>& vect) {
    JValue jv(vrs_rapidjson::kArrayType);
    jv.Reserve(static_cast<vrs_rapidjson::SizeType>(vect.size()), alloc);
    for (const auto& v : vect) {
      jv.PushBack(jValue(v), alloc);
    }
    return jv;
  }

  template <typename T, size_t N>
  inline JValue jValue(const PointND<T, N>& point) {
    JValue jv(vrs_rapidjson::kArrayType);
    jv.Reserve(static_cast<vrs_rapidjson::SizeType>(N), alloc);
    for (size_t n = 0; n < N; ++n) {
      jv.PushBack(point.dim[n], alloc);
    }
    return jv;
  }

  template <typename T, size_t N>
  JValue jValue(const MatrixND<T, N>& matrix) {
    JValue jv(vrs_rapidjson::kArrayType);
    jv.Reserve(static_cast<vrs_rapidjson::SizeType>(N), alloc);
    for (size_t n = 0; n < N; ++n) {
      jv.PushBack(jValue(matrix.points[n]), alloc);
    }
    return jv;
  }

  template <typename JSTR>
  inline void addMember(const JSTR& name, JValue& v) {
    value.AddMember(jStringRef(name), v, alloc);
  }

  template <typename JSTR>
  inline void addMember(const JSTR& name, const char* str) {
    value.AddMember(jStringRef(name), jStringRef(str), alloc);
  }

  template <typename JSTR, typename T>
  inline void addMember(const JSTR& name, const T& v) {
    value.AddMember(jStringRef(name), jValue(v), alloc);
  }
};

template <>
inline JValue JsonWrapper::jValue<Bool>(const Bool& v) {
  return JValue(v.operator bool());
}

template <>
inline JValue JsonWrapper::jValue<string>(const string& str) {
  JValue jstring;
  jstring.SetString(str.c_str(), static_cast<vrs_rapidjson::SizeType>(str.length()), alloc);
  return jstring;
}

template <typename T, typename JSTR>
inline void serializeMap(const map<string, T>& amap, JsonWrapper& rj, const JSTR& name) {
  using namespace vrs_rapidjson;
  if (amap.size() > 0) {
    JValue mapValues(kObjectType);
    for (const auto& element : amap) {
      mapValues.AddMember(rj.jValue(element.first), rj.jValue(element.second), rj.alloc);
    }
    rj.addMember(name, mapValues);
  }
}

// when the map<string, string> will live as long as the json serialization (avoid string copies)
template <typename JSTR>
inline void
serializeStringRefMap(const map<string, string>& stringMap, JsonWrapper& rj, const JSTR& name) {
  using namespace vrs_rapidjson;
  if (stringMap.size() > 0) {
    JValue mapValues(kObjectType);
    for (const auto& element : stringMap) {
      mapValues.AddMember(jStringRef(element.first), jStringRef(element.second), rj.alloc);
    }
    rj.addMember(name, mapValues);
  }
}

template <typename T, typename JSTR>
inline void serializeVector(const vector<T>& vect, JsonWrapper& rj, const JSTR& name) {
  using namespace vrs_rapidjson;
  if (vect.size() > 0) {
    JValue arrayValues(kArrayType);
    arrayValues.Reserve(static_cast<SizeType>(vect.size()), rj.alloc);
    for (const auto& element : vect) {
      arrayValues.PushBack(rj.jValue(element), rj.alloc);
    }
    rj.addMember(name, arrayValues);
  }
}

// when the vector<string> will live as long as the json serialization (avoid string copies)
template <typename JSTR>
inline void
serializeStringRefVector(const vector<string>& vect, JsonWrapper& rj, const JSTR& name) {
  using namespace vrs_rapidjson;
  if (vect.size() > 0) {
    JValue arrayValues(kArrayType);
    arrayValues.Reserve(static_cast<SizeType>(vect.size()), rj.alloc);
    for (const auto& str : vect) {
      arrayValues.PushBack(jStringRef(str), rj.alloc);
    }
    rj.addMember(name, arrayValues);
  }
}

template <typename JSON_TYPE, typename OUT_TYPE>
inline bool getJValueAs(const JValue& value, OUT_TYPE& outValue) {
  if (value.Is<JSON_TYPE>()) {
    JSON_TYPE jsonValue = value.Get<JSON_TYPE>();
    outValue = static_cast<OUT_TYPE>(jsonValue);
    return true;
  }
  return false;
}

template <typename T>
inline bool getFromJValue(const JValue& value, T& outValue) {
  if (is_floating_point<T>::value) {
    if (is_same<T, double>::value) {
      return getJValueAs<double, T>(value, outValue) || getJValueAs<float, T>(value, outValue) ||
          getJValueAs<int64_t, T>(value, outValue);
    } else {
      return getJValueAs<float, T>(value, outValue) || getJValueAs<double, T>(value, outValue) ||
          getJValueAs<int64_t, T>(value, outValue);
    }
  }
  if (is_same<T, Bool>::value) {
    return getJValueAs<bool, T>(value, outValue) || getJValueAs<int, T>(value, outValue);
  } else if (is_integral<T>::value) {
    if (is_signed<T>::value) {
      return getJValueAs<int, T>(value, outValue) || getJValueAs<int64_t, T>(value, outValue);
    } else {
      return getJValueAs<unsigned, T>(value, outValue) || getJValueAs<uint64_t, T>(value, outValue);
    }
  }
  XR_CHECK(false, "This type is not some number: you need to implement a specialized version");
  return false;
}

template <>
inline bool getFromJValue(const JValue& value, string& outValue) {
  if (value.IsString()) {
    outValue = value.GetString();
    return true;
  }
  return false;
}

template <typename T, size_t N>
inline bool getFromJValue(const JValue& value, PointND<T, N>& outPoint) {
  using namespace vrs_rapidjson;
  if (value.IsArray() && value.Size() == N) {
    for (size_t n = 0; n < N; ++n) {
      if (!getJValueAs<float, T>(value[static_cast<SizeType>(n)], outPoint.dim[n]) &&
          !getJValueAs<double, T>(value[static_cast<SizeType>(n)], outPoint.dim[n]) &&
          !getJValueAs<int32_t, T>(value[static_cast<SizeType>(n)], outPoint.dim[n])) {
        return false;
      }
    }
  }
  return true;
}

template <typename T, size_t N>
inline bool getFromJValue(const JValue& value, MatrixND<T, N>& outMatrix) {
  using namespace vrs_rapidjson;
  if (value.IsArray() && value.Size() == N) {
    for (size_t n = 0; n < N; ++n) {
      if (!getFromJValue<T, N>(value[static_cast<SizeType>(n)], outMatrix.points[n])) {
        return false;
      }
    }
  }
  return true;
}

template <typename T, typename JSTR>
inline bool getJMap(map<string, T>& outMap, const JValue& piece, const JSTR& name) {
  using namespace vrs_rapidjson;
  outMap.clear();
  const JValue::ConstMemberIterator properties = piece.FindMember(name);
  if (properties != piece.MemberEnd() && properties->value.IsObject()) {
    for (JValue::ConstMemberIterator itr = properties->value.MemberBegin();
         itr != properties->value.MemberEnd();
         ++itr) {
      T value;
      if (getFromJValue(itr->value, value)) {
        outMap[itr->name.GetString()] = value;
      }
    }
    return true;
  }
  return false;
}

template <typename T, typename JSTR>
inline bool getJVector(vector<T>& outVector, const JValue& piece, const JSTR& name) {
  using namespace vrs_rapidjson;
  outVector.clear();
  const JValue::ConstMemberIterator properties = piece.FindMember(name);
  if (properties != piece.MemberEnd() && properties->value.IsArray()) {
    outVector.reserve(properties->value.GetArray().Size());
    for (JValue::ConstValueIterator itr = properties->value.Begin(); itr != properties->value.End();
         ++itr) {
      T value;
      if (getFromJValue(*itr, value)) {
        outVector.push_back(value);
      }
    }
    return true;
  }
  return false;
}

template <typename JSTR>
inline bool getJString(string& outString, const JValue& piece, const JSTR& name) {
  using namespace vrs_rapidjson;
  const JValue::ConstMemberIterator member = piece.FindMember(name);
  if (member != piece.MemberEnd() && member->value.IsString()) {
    outString = member->value.GetString();
    return true;
  }
  outString.clear();
  return false;
}

template <typename JSTR>
inline bool getJInt64(int64_t& outInt64, const JValue& piece, const JSTR& name) {
  using namespace vrs_rapidjson;
  const JValue::ConstMemberIterator member = piece.FindMember(name);
  if (member != piece.MemberEnd() && member->value.IsInt64()) {
    outInt64 = member->value.GetInt64();
    return true;
  }
  outInt64 = 0;
  return false;
}

template <typename JSTR>
inline bool getJInt(int& outInt, const JValue& piece, const JSTR& name) {
  using namespace vrs_rapidjson;
  const JValue::ConstMemberIterator member = piece.FindMember(name);
  if (member != piece.MemberEnd() && member->value.IsInt()) {
    outInt = member->value.GetInt();
    return true;
  }
  outInt = 0;
  return false;
}

template <typename JSTR>
inline bool getJDouble(double& outDouble, const JValue& piece, const JSTR& name) {
  using namespace vrs_rapidjson;
  const JValue::ConstMemberIterator member = piece.FindMember(name);
  if (member != piece.MemberEnd() && member->value.IsDouble()) {
    outDouble = member->value.GetDouble();
    return true;
  }
  outDouble = 0;
  return false;
}

// double -> json -> double & float -> json -> float don't preserve prefect accuracy
// These methods are used to verify "reasonable" accuracy of serialization/deserialization... :-(
template <typename T>
inline bool isSame(const T& left, const T& right) {
  return left == right;
}

template <typename T>
inline bool isSame(const unique_ptr<T>& left, const unique_ptr<T>& right) {
  return left ? right && isSame(*left.get(), *right.get()) : !right;
}

template <>
inline bool isSame<float>(const float& left, const float& right) {
  double dleft = static_cast<double>(left);
  double dright = static_cast<double>(right);
  return fabs(dleft - dright) <= max<double>(fabs(dleft), fabs(dright)) / 10000;
}

template <>
inline bool isSame<double>(const double& left, const double& right) {
  return fabs(left - right) <= max<double>(fabs(left), fabs(right)) / 10000;
}

template <typename T, size_t N>
inline bool isSame(const PointND<T, N>& left, const PointND<T, N>& right) {
  for (size_t n = 0; n < N; ++n) {
    if (!isSame(left.dim[n], right.dim[n])) {
      return false;
    }
  }
  return true;
}

template <typename T, size_t N>
inline bool isSame(const MatrixND<T, N>& left, const MatrixND<T, N>& right) {
  for (size_t n = 0; n < N; ++n) {
    if (!isSame(left[n], right[n])) {
      return false;
    }
  }
  return true;
}

template <typename T>
inline bool isSame(const vector<T>& left, const vector<T>& right) {
  if (left.size() != right.size()) {
    return false;
  }
  for (size_t index = 0; index < left.size(); ++index) {
    if (!vrs::isSame(left[index], right[index])) {
      return false;
    }
  }
  return true;
}

template <typename T>
inline bool isSame(const map<string, T>& left, const map<string, T>& right) {
  if (left.size() != right.size()) {
    return false;
  }
  for (const auto& leftPair : left) {
    const auto& rightMatch = right.find(leftPair.first);
    if (rightMatch == right.end() || !vrs::isSame(leftPair.second, rightMatch->second)) {
      return false;
    }
  }
  return true;
}

inline string jDocumentToJsonString(const JDocument& document) {
  using namespace vrs_rapidjson;
  StringBuffer buffer;
  using JWriter = vrs_rapidjson::
      Writer<StringBuffer, JUtf8Encoding, JUtf8Encoding, JCrtAllocator, kWriteNanAndInfFlag>;
  JWriter writer(buffer);
  document.Accept(writer);
  return buffer.GetString();
}

inline string jDocumentToJsonStringPretty(const JDocument& document) {
  using namespace vrs_rapidjson;
  StringBuffer buffer;
  using JPrettyWriter = vrs_rapidjson::
      PrettyWriter<StringBuffer, JUtf8Encoding, JUtf8Encoding, JCrtAllocator, kWriteNanAndInfFlag>;
  JPrettyWriter prettyWriter(buffer);
  document.Accept(prettyWriter);
  return buffer.GetString();
}

} // namespace vrs
