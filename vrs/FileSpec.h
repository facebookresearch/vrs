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

#include <functional>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace vrs {
using std::map;
using std::string;
using std::vector;

/// \brief Generalized file descriptor class, allowing the efficient representation of complex
/// file objects, maybe multi-chunks, with additional optional properties.
///
/// File specification struct, to describe a file object in more details than just a single path,
/// possibly with multiple chunks, with a special file handler, an explicit file name (useful when
/// the chunks are urls), and possibly a source uri.
/// If no file handler name is specified, the object is assumed to a set of local files.
/// Additional properties may be specified in the extras field, which has helper methods.
struct FileSpec {
  FileSpec() {}
  FileSpec(const string& filehandler, const vector<string>& chunks)
      : fileHandlerName{filehandler}, chunks{chunks} {}
  FileSpec(const string& filehandler, const vector<string>&& chunks)
      : fileHandlerName{filehandler}, chunks{chunks} {}
  FileSpec(const vector<string>& chunks) : chunks{chunks} {}
  FileSpec(vector<string>&& chunks) : chunks{std::move(chunks)} {}

  /// clear all the fields.
  void clear();

  bool empty() const;

  bool isDiskFile() const;

  static int parseUri(
      const string& uri,
      string& outScheme,
      string& outPath,
      map<string, string>& outQueryParams);

  /// Smart setter that will parse the string given, determining if the string passed is a local
  /// file path, a uri, or a json path.
  /// @param pathJsonUri: a path, a json spec, or a URI
  /// @return A status code, 0 meaning apparent success. Note that the validation is superficial,
  /// a file might not exists, the requested filehandler may not be available, etc.
  int fromPathJsonUri(const string& pathJsonUri);

  // Parse json and extract file specs, with optional extra parameters.
  // @param jsonStr: ex. {"storage": "mystorage", "chunks":["chunk1", "chunk2"],
  // "filename":"file.vrs"}.
  // @return True if parsing the json succeeded.
  bool fromJson(const string& jsonStr);

  // Generate json string from a file spec.
  // @return jsonStr: ex. {"storage": "mystorage", "chunks":["chunk1", "chunk2"],
  // "filename":"file.vrs"}.
  string toJson() const;

  /// Tell if we have chunks and all of them has a file size.
  bool hasChunkSizes() const;
  /// Get the total size of the object, or -1 if don't know.
  int64_t getFileSize() const;
  /// Get the location of the object, which is the uri (if any), or the file handler.
  string getSourceLocation() const;
  /// Logical reverse operation from fromPathJsonUri, but kept minimal for logging
  string getEasyPath() const;

  /// Get signature of the path
  string getXXHash() const;

  /// Test equality (for testing)
  bool operator==(const FileSpec& rhs) const;

  /// Get an extra parameter, or the empty string.
  string getExtra(const string& name) const;
  /// Tell if an extra parameter is defined.
  bool hasExtra(const string& name) const;
  /// Get an extra parameter interpreted as an int, or a default value if not available,
  /// or in case of type conversion error.
  int getExtraAsInt(const string& name, int defaultValue = 0) const;
  /// Get an extra parameter interpreted as an int64, or a default value if not available,
  /// or in case of type conversion error.
  int64_t getExtraAsInt64(const string& name, int64_t defaultValue = 0) const;
  /// Get an extra parameter interpreted as an uint64, or a default value if not available,
  /// or in case of type conversion error.
  uint64_t getExtraAsUInt64(const string& name, uint64_t defaultValue = 0) const;
  /// Get an extra parameter interpreted as a double, or a default value if not available,
  /// or in case of type conversion error.
  double getExtraAsDouble(const string& name, double defaultValue = 0) const;
  /// Get an extra parameter interpreted as a bool, or a default value if not available.
  /// The value is evaluated to be true if it is either "1" or "true", o/w this returns false.
  bool getExtraAsBool(const string& name, bool defaultValue = false) const;

  static int decodeQuery(const string& query, string& outKey, string& outValue);
  static int urldecode(const string& in, string& out);

  template <typename T>
  void setExtra(const string& name, const T& value);

  /// Unset an extra parameter
  void unsetExtra(const string& name);

  string fileHandlerName;
  string fileName;
  string uri;
  vector<string> chunks;
  vector<int64_t> chunkSizes;
  map<string, string> extras;
};

template <>
inline void FileSpec::setExtra(const string& name, const string& value) {
  extras[name] = value;
}

template <>
inline void FileSpec::setExtra(const string& name, const bool& value) {
  extras[name] = value ? "1" : "0";
}

template <typename T>
inline void FileSpec::setExtra(const string& name, const T& value) {
  extras[name] = std::to_string(value);
}
} // namespace vrs
