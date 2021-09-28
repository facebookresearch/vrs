// Copyright (c) Facebook, Inc. and its affiliates. Confidential and proprietary.

#pragma once

#include <cmath>

#include <map>
#include <type_traits>
#include <vector>

#include <Serialization/Rapidjson.h>
#include <logging/Checks.h>

#include <vrs/DataPieces.h>

namespace vrs {

using std::is_floating_point;
using std::is_integral;
using std::is_same;
using std::is_signed;
using std::max;
using std::string;

/// rapidjson::Document's default MemoryPoolAllocator crashes on some platforms
/// as documented in https://github.com/cocos2d/cocos2d-x/issues/16492
using JUtf8Encoding = fb_rapidjson::UTF8<>;
using JCrtAllocator = fb_rapidjson::CrtAllocator;
using JDocument = fb_rapidjson::GenericDocument<JUtf8Encoding, JCrtAllocator>;
using JValue = fb_rapidjson::GenericValue<JUtf8Encoding, JCrtAllocator>;

/// Helper class to generate json messages using RapidJson.
/// For use by VRS only.
/// @internal
struct JsonWrapper {
  JValue& value;
  JDocument::AllocatorType& alloc;

  template <typename T>
  inline JValue jValue(const T& v) {
    return JValue(v);
  }

  template <typename T>
  inline JValue jValue(const std::vector<T>& vect) {
    JValue jv(fb_rapidjson::kArrayType);
    jv.Reserve(static_cast<fb_rapidjson::SizeType>(vect.size()), alloc);
    for (const auto& v : vect) {
      jv.PushBack(jValue(v), alloc);
    }
    return jv;
  }

  template <typename T, size_t N>
  inline JValue jValue(const PointND<T, N>& point) {
    JValue jv(fb_rapidjson::kArrayType);
    jv.Reserve(static_cast<fb_rapidjson::SizeType>(N), alloc);
    for (size_t n = 0; n < N; ++n) {
      jv.PushBack(point.dim[n], alloc);
    }
    return jv;
  }

  template <typename T, size_t N>
  JValue jValue(const MatrixND<T, N>& matrix) {
    JValue jv(fb_rapidjson::kArrayType);
    jv.Reserve(static_cast<fb_rapidjson::SizeType>(N), alloc);
    for (size_t n = 0; n < N; ++n) {
      jv.PushBack(jValue(matrix.points[n]), alloc);
    }
    return jv;
  }

  void addMember(const char* name, JValue& v) {
    value.AddMember(fb_rapidjson::StringRef(name), v, alloc);
  }

  void addMember(const char* name, const char* str) {
    value.AddMember(fb_rapidjson::StringRef(name), fb_rapidjson::StringRef(str), alloc);
  }

  void addMember(const string& name, JValue& v);

  template <typename T>
  void addMember(const char* name, const T& v);

  template <typename T>
  void addMember(const string& name, const T& v);
};

template <>
inline JValue JsonWrapper::jValue<Bool>(const Bool& v) {
  return JValue(v.operator bool());
}

template <>
inline JValue JsonWrapper::jValue<string>(const string& str) {
  JValue jstring;
  jstring.SetString(str.c_str(), static_cast<fb_rapidjson::SizeType>(str.length()), alloc);
  return jstring;
}

inline void JsonWrapper::addMember(const string& name, JValue& v) {
  value.AddMember(jValue(name), v, alloc);
}

template <typename T>
inline void JsonWrapper::addMember(const char* name, const T& v) {
  value.AddMember(fb_rapidjson::StringRef(name), jValue(v), alloc);
}

template <typename T>
inline void JsonWrapper::addMember(const string& name, const T& v) {
  value.AddMember(jValue(name), jValue(v), alloc);
}

template <typename T>
inline void serializeMap(
    const map<string, T>& amap,
    JsonWrapper& rj,
    fb_rapidjson::GenericStringRef<char> name) {
  using namespace fb_rapidjson;
  if (amap.size() > 0) {
    JValue mapValues(kObjectType);
    for (const auto& element : amap) {
      mapValues.AddMember(rj.jValue(element.first), rj.jValue(element.second), rj.alloc);
    }
    rj.addMember(name, mapValues);
  }
}

template <>
inline void serializeMap<string>(
    const map<string, string>& amap,
    JsonWrapper& rj,
    fb_rapidjson::GenericStringRef<char> name) {
  using namespace fb_rapidjson;
  if (amap.size() > 0) {
    JValue mapValues(kObjectType);
    for (const auto& element : amap) {
      mapValues.AddMember(rj.jValue(element.first), rj.jValue(element.second), rj.alloc);
    }
    rj.addMember(name, mapValues);
  }
}

template <typename T>
inline void
serializeVector(const vector<T>& vect, JsonWrapper& rj, fb_rapidjson::GenericStringRef<char> name) {
  using namespace fb_rapidjson;
  if (vect.size() > 0) {
    JValue arrayValues(kArrayType);
    arrayValues.Reserve(static_cast<SizeType>(vect.size()), rj.alloc);
    for (const auto& element : vect) {
      arrayValues.PushBack(rj.jValue(element), rj.alloc);
    }
    rj.addMember(name, arrayValues);
  }
}

template <typename T>
inline void serializeStringMap(
    const map<string, T>& stringMap,
    JsonWrapper& rj,
    fb_rapidjson::GenericStringRef<char> name) {
  using namespace fb_rapidjson;
  if (stringMap.size() > 0) {
    JValue mapValues(kObjectType);
    for (const auto& element : stringMap) {
      mapValues.AddMember(rj.jValue(element.first), rj.jValue(element.second), rj.alloc);
    }
    rj.addMember(name, mapValues);
  }
}

template <typename JSON_TYPE, typename OUT_TYPE>
inline bool getFromRapidjsonValueAs(const JValue& value, OUT_TYPE& outValue) {
  if (value.Is<JSON_TYPE>()) {
    JSON_TYPE jsonValue = value.Get<JSON_TYPE>();
    outValue = static_cast<OUT_TYPE>(jsonValue);
    return true;
  }
  return false;
}

template <typename T>
inline bool getFromRapidjsonValue(const JValue& value, T& outValue) {
  if (is_floating_point<T>::value) {
    if (is_same<T, double>::value) {
      return getFromRapidjsonValueAs<double, T>(value, outValue) ||
          getFromRapidjsonValueAs<float, T>(value, outValue) ||
          getFromRapidjsonValueAs<int64_t, T>(value, outValue);
    } else {
      return getFromRapidjsonValueAs<float, T>(value, outValue) ||
          getFromRapidjsonValueAs<double, T>(value, outValue) ||
          getFromRapidjsonValueAs<int64_t, T>(value, outValue);
    }
  }
  if (is_same<T, Bool>::value) {
    return getFromRapidjsonValueAs<bool, T>(value, outValue) ||
        getFromRapidjsonValueAs<int, T>(value, outValue);
  } else if (is_integral<T>::value) {
    if (is_signed<T>::value) {
      return getFromRapidjsonValueAs<int, T>(value, outValue) ||
          getFromRapidjsonValueAs<int64_t, T>(value, outValue);
    } else {
      return getFromRapidjsonValueAs<unsigned, T>(value, outValue) ||
          getFromRapidjsonValueAs<uint64_t, T>(value, outValue);
    }
  }
  XR_CHECK(false, "This type is not some number: you need to implement a specialized version");
  return false;
}

template <>
inline bool getFromRapidjsonValue(const JValue& value, string& outValue) {
  if (value.IsString()) {
    outValue = value.GetString();
    return true;
  }
  return false;
}

template <typename T, size_t N>
inline bool getFromRapidjsonValue(const JValue& value, PointND<T, N>& outPoint) {
  using namespace fb_rapidjson;
  if (value.IsArray() && value.Size() == N) {
    for (size_t n = 0; n < N; ++n) {
      if (!getFromRapidjsonValueAs<float, T>(value[static_cast<SizeType>(n)], outPoint.dim[n]) &&
          !getFromRapidjsonValueAs<double, T>(value[static_cast<SizeType>(n)], outPoint.dim[n]) &&
          !getFromRapidjsonValueAs<int32_t, T>(value[static_cast<SizeType>(n)], outPoint.dim[n])) {
        return false;
      }
    }
  }
  return true;
}

template <typename T, size_t N>
inline bool getFromRapidjsonValue(const JValue& value, MatrixND<T, N>& outMatrix) {
  using namespace fb_rapidjson;
  if (value.IsArray() && value.Size() == N) {
    for (size_t n = 0; n < N; ++n) {
      if (!getFromRapidjsonValue<T, N>(value[static_cast<SizeType>(n)], outMatrix.points[n])) {
        return false;
      }
    }
  }
  return true;
}

template <typename T>
inline void
getMap(map<string, T>& outMap, const JValue& piece, fb_rapidjson::GenericStringRef<char> name) {
  using namespace fb_rapidjson;
  outMap.clear();
  const JValue::ConstMemberIterator properties = piece.FindMember(name);
  if (properties != piece.MemberEnd() && properties->value.IsObject()) {
    for (JValue::ConstMemberIterator itr = properties->value.MemberBegin();
         itr != properties->value.MemberEnd();
         ++itr) {
      T value;
      if (getFromRapidjsonValue(itr->value, value)) {
        outMap[itr->name.GetString()] = value;
      }
    }
  }
}

template <typename T>
inline void
getVector(vector<T>& outVector, const JValue& piece, fb_rapidjson::GenericStringRef<char> name) {
  using namespace fb_rapidjson;
  outVector.clear();
  const JValue::ConstMemberIterator properties = piece.FindMember(name);
  if (properties != piece.MemberEnd() && properties->value.IsArray()) {
    outVector.reserve(properties->value.GetArray().Size());
    for (JValue::ConstValueIterator itr = properties->value.Begin(); itr != properties->value.End();
         ++itr) {
      T value;
      if (getFromRapidjsonValue(*itr, value)) {
        outVector.push_back(value);
      }
    }
  }
}

inline bool getString(string& outString, const JValue& piece, const char* name) {
  using namespace fb_rapidjson;
  const JValue::ConstMemberIterator member = piece.FindMember(name);
  if (member != piece.MemberEnd() && member->value.IsString()) {
    outString = member->value.GetString();
    return true;
  }
  outString.clear();
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
  using namespace fb_rapidjson;
  StringBuffer buffer;
  using JWriter = fb_rapidjson::
      Writer<StringBuffer, JUtf8Encoding, JUtf8Encoding, JCrtAllocator, kWriteNanAndInfFlag>;
  JWriter writer(buffer);
  document.Accept(writer);
  return buffer.GetString();
}

inline string jDocumentToJsonStringPretty(const JDocument& document) {
  using namespace fb_rapidjson;
  StringBuffer buffer;
  using JPrettyWriter = fb_rapidjson::
      PrettyWriter<StringBuffer, JUtf8Encoding, JUtf8Encoding, JCrtAllocator, kWriteNanAndInfFlag>;
  JPrettyWriter prettyWriter(buffer);
  document.Accept(prettyWriter);
  return buffer.GetString();
}

} // namespace vrs
