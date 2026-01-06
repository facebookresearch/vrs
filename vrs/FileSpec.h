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

#include <map>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace vrs {
using std::map;
using std::string;
using std::string_view;
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
  using Extras = map<string, string>;

  FileSpec() = default;
  FileSpec(string filehandler, const vector<string>& chunksIn)
      : fileHandlerName{std::move(filehandler)}, chunks{chunksIn} {}
  FileSpec(string filehandler, const vector<string>&& chunksIn)
      : fileHandlerName{std::move(filehandler)}, chunks{chunksIn} {}
  explicit FileSpec(const vector<string>& chunksIn) : chunks{chunksIn} {}
  explicit FileSpec(vector<string>&& chunksIn) : chunks{std::move(chunksIn)} {}

  /// clear all the fields.
  void clear();

  bool empty() const;

  bool isDiskFile() const;

  static int parseUri(string_view uri, string& outScheme, string& outPath, Extras& outQueryParams);

  /// Smart setter that will parse the string given, determining if the string passed is a local
  /// file path, a uri, or a json path.
  /// @param pathJsonUri: a path, a json spec, or a URI
  /// @param defaultFileHandlerName: provide a default FileHandler name. DiskFile by default.
  /// @return A status code, 0 meaning apparent success. Note that the validation is superficial,
  /// a file might not exists, the requested filehandler may not be available, etc.
  int fromPathJsonUri(string_view pathJsonUri, string_view defaultFileHandlerName = {});

  /// Reverse operation as fromPathJsonUri, as possible
  string toPathJsonUri() const;

  // Parse json and extract file specs, with optional extra parameters.
  // @param jsonStr: ex. {"storage": "mystorage", "chunks":["chunk1", "chunk2"],
  // "filename":"file.vrs"}.
  // @return True if parsing the json succeeded.
  bool fromJson(string_view jsonStr);

  // Generate json string from a file spec.
  // @return jsonStr: ex. {"storage": "mystorage", "chunks":["chunk1", "chunk2"],
  // "filename":"file.vrs"}.
  string toJson() const;

  /// Parse the uri field already set, overwriting other fields on success.
  int parseUri();

  /// Tell if we have chunks and all of them has a file size.
  bool hasChunkSizes() const;
  /// Get the total size of the object, or -1 if don't know.
  int64_t getFileSize() const;
  /// Get the location of the object, which is the uri (if any), or the file handler.
  string getSourceLocation() const;
  /// Logical reverse operation from fromPathJsonUri, but kept minimal for logging
  string getEasyPath() const;
  /// Get filename, if possible
  string getFileName() const;

  /// Get signature of the path
  string getXXHash() const;

  /// Test equality (for testing)
  bool operator==(const FileSpec& rhs) const;

  /// Get an extra parameter, or the empty string.
  const string& getExtra(string_view name) const;
  /// Get an extra parameter value, and know if a match was found.
  bool getExtra(string_view name, string& outValue) const;
  /// Tell if an extra parameter is defined.
  bool hasExtra(string_view name) const;
  /// Get an extra parameter interpreted as an int, or a default value if not available,
  /// or in case of type conversion error.
  int getExtraAsInt(string_view name, int defaultValue = 0) const;
  /// Get an extra parameter interpreted as an int64, or a default value if not available,
  /// or in case of type conversion error.
  int64_t getExtraAsInt64(string_view name, int64_t defaultValue = 0) const;
  /// Get an extra parameter interpreted as an uint64, or a default value if not available,
  /// or in case of type conversion error.
  uint64_t getExtraAsUInt64(string_view name, uint64_t defaultValue = 0) const;
  /// Get an extra parameter interpreted as a double, or a default value if not available,
  /// or in case of type conversion error.
  double getExtraAsDouble(string_view name, double defaultValue = 0) const;
  /// Get an extra parameter interpreted as a bool, or a default value if not available.
  /// The value is evaluated to be true if it is either "1" or "true", o/w this returns false.
  bool getExtraAsBool(string_view name, bool defaultValue = false) const;

  static int decodeQuery(string_view query, string& outKey, string& outValue);
  static int urldecode(string_view in, string& out);

  void setExtra(string_view name, string_view value);
  void setExtra(string_view name, const char* value);
  void setExtra(string_view name, bool value);
  void setExtra(string_view name, const string& value);
  template <typename T>
  void setExtra(string_view name, T value) {
    extras[string(name)] = std::to_string(value);
  }

  /// Unset an extra parameter
  void unsetExtra(string_view name);

  string fileHandlerName;
  string fileName;
  string uri;
  vector<string> chunks;
  vector<int64_t> chunkSizes;
  Extras extras;
};

} // namespace vrs
