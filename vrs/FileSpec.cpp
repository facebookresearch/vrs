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

#include <vrs/FileHandler.h>

#include <cctype>
#include <cstring>
#include <string_view>

#include <tuple>

#define DEFAULT_LOG_CHANNEL "FileHandler"
#include <logging/Log.h>
#include <logging/Verify.h>

#include <vrs/DiskFile.h>
#include <vrs/ErrorCode.h>
#include <vrs/FileHandlerFactory.h>
#include <vrs/helpers/EnumStringConverter.h>
#include <vrs/helpers/Rapidjson.hpp>
#include <vrs/helpers/Strings.h>
#include <vrs/os/Utils.h>
#include <vrs/utils/xxhash/xxhash.h>

using namespace std;

namespace vrs {

namespace {

constexpr const char* kChunkField = "chunks";
constexpr const char* kChunkSizesField = "chunk_sizes";
constexpr const char* kFileHandlerField = "storage";
constexpr const char* kFileNameField = "filename";
constexpr const char* kUriField = "source_uri";

// Validates URI scheme according to RFC 3986.
// Schema must start with a letter and contain only alphanumeric characters, '.', '-', '+', or '_'.
// Returns true if valid, false otherwise.
bool validateUriSchema(string_view uri, size_t colonPos) {
  for (size_t p = 0; p < colonPos; p++) {
    unsigned char c = static_cast<unsigned char>(uri[p]);
    if (p == 0) {
      if (!isalpha(c)) {
        XR_LOGE("Invalid URI scheme for '{}'", uri);
        return false;
      }
    } else {
      if (!isalnum(c) && c != '.' && c != '-' && c != '+' && c != '_') {
        XR_LOGE("Invalid character '{}' in URI scheme of '{}'", c, uri);
        return false;
      }
    }
  }
  return true;
}

// Parses query parameters from a URI query string (portion after '?').
// Parameters are separated by '&' or ';' and formatted as key=value.
void parseQueryParams(string_view uri, size_t queryStart, FileSpec::Extras& outQueryParams) {
  size_t start = queryStart + 1;
  for (size_t p = start; p < uri.size(); p++) {
    unsigned char c = static_cast<unsigned char>(uri[p]);
    if (c == '&' || c == ';' || p == uri.size() - 1) {
      string key, value;
      size_t length = (c == '&' || c == ';') ? p - start : p - start + 1;
      if (FileSpec::decodeQuery(uri.substr(start, length), key, value) == 0) {
        outQueryParams[key] = value;
      }
      start = p + 1;
    }
  }
}

} // namespace

void FileSpec::clear() {
  fileHandlerName.clear();
  fileName.clear();
  uri.clear();
  chunks.clear();
  chunkSizes.clear();
  extras.clear();
}

bool FileSpec::empty() const {
  return fileHandlerName.empty() && fileName.empty() && uri.empty() && chunks.empty() &&
      chunkSizes.empty() && extras.empty();
}

bool FileSpec::isDiskFile() const {
  return fileHandlerName.empty() || fileHandlerName == DiskFile::staticName()
#if VRS_ASYNC_DISKFILE_SUPPORTED()
      || fileHandlerName == AsyncDiskFile::staticName()
#endif
      ;
}

int FileSpec::parseUri(
    string_view uri,
    string& outScheme,
    string& outPath,
    Extras& outQueryParams) {
  // Parse URI following https://en.wikipedia.org/wiki/Uniform_Resource_Identifier.
  // URI should look like <scheme>:<path>?<query> while <scheme> corresponds to file handler name.
  // The query should be used to specify additional fields for each file handler.
  outScheme.clear();
  outPath.clear();
  outQueryParams.clear();

  auto colon = uri.find(':');
  if (colon == 0) {
    XR_LOGE("Schema required before ':': {}", uri);
    return INVALID_URI_FORMAT;
  }

  // Validate scheme if present
  if (colon != string_view::npos && !validateUriSchema(uri, colon)) {
    return INVALID_URI_FORMAT;
  }

  auto query = uri.find('?');

  // Path length must be greater than 0
  if (query <= colon + 1 || (query == string_view::npos && colon >= uri.size() - 1)) {
    XR_LOGE("'{}' is not a valid URI.", uri);
    return INVALID_URI_FORMAT;
  }

  // Parse query parameters if present
  if (query != string_view::npos) {
    parseQueryParams(uri, query, outQueryParams);
  }

  // Extract scheme
  if (colon != string_view::npos && colon > 0) {
    outScheme = string(uri.substr(0, colon));
  }

  // Extract and decode path
  string_view path = (colon != string_view::npos) ? uri.substr(colon + 1, query - colon - 1)
                                                  : uri.substr(0, query);
  int status = FileSpec::urldecode(path, outPath);
  if (status != 0) {
    XR_LOGE("Invalid character in URI '{}'", path);
    outScheme.clear();
    outPath.clear();
    outQueryParams.clear();
    return INVALID_URI_VALUE;
  }

  return SUCCESS;
}

int FileSpec::fromPathJsonUri(string_view pathJsonUri, string_view defaultFileHandlerName) {
  clear();
  if (pathJsonUri.empty()) {
    return INVALID_PARAMETER;
  } else if (pathJsonUri.front() == '{') {
    return fromJson(pathJsonUri) ? SUCCESS : FILEPATH_PARSE_ERROR;
  }
  auto colon = pathJsonUri.find(':');
  bool isUri = colon != string_view::npos && colon > 1;
  for (size_t p = 0; p < colon && isUri; p++) {
    unsigned char c = static_cast<unsigned char>(pathJsonUri[p]);
    // from https://en.wikipedia.org/wiki/Uniform_Resource_Identifier#Generic_syntax
    isUri = (p == 0) ? isalpha(c) : isalnum(c) || c == '.' || c == '-' || c == '+' || c == '_';
  }
  if (!isUri) {
    // Check if there might be URI parameter in the path, and try to parse it that way.
    auto uriParams = pathJsonUri.find('?');
    bool hasParams = uriParams != string_view::npos && uriParams > 1 &&
        isalpha(static_cast<unsigned char>(pathJsonUri[uriParams + 1])) &&
        pathJsonUri.find('=', uriParams + 2) != string_view::npos;
    fileHandlerName =
        defaultFileHandlerName.empty() ? DiskFile::staticName() : string(defaultFileHandlerName);
    if (hasParams) {
      uri = pathJsonUri;
      if (FileHandlerFactory::getInstance().parseUri(*this, 0) == SUCCESS) {
        return SUCCESS;
      }
      uri.clear();
    }
    chunks = {string(pathJsonUri)};
    return SUCCESS;
  } else {
    fileHandlerName = string(pathJsonUri.substr(0, colon));
  }
  uri = pathJsonUri;

  // give a chance to a file handler named after the uri scheme, if any, to parse the uri.
  return FileHandlerFactory::getInstance().parseUri(*this, colon);
}

string FileSpec::toPathJsonUri() const {
  if (isDiskFile()) {
    if (chunks.size() == 1 && extras.empty()) {
      return chunks.front();
    }
    return empty() ? "" : toJson();
  }
  if (!uri.empty()) {
    return uri;
  }
  return empty() ? "" : toJson();
}

bool FileSpec::fromJson(string_view jsonStr) {
  using namespace vrs_rapidjson;
  JDocument document;
  jParse(document, string(jsonStr));
  if (document.IsObject()) {
    getJString(fileName, document, kFileNameField);
    getJString(fileHandlerName, document, kFileHandlerField);
    getJString(uri, document, kUriField);
    extras.clear();
    for (auto iter = document.MemberBegin(); iter != document.MemberEnd(); ++iter) {
      const auto* key = iter->name.GetString();
      if (iter->value.IsString() && strcmp(key, kChunkField) != 0 &&
          strcmp(key, kFileHandlerField) != 0 && strcmp(key, kFileNameField) != 0 &&
          strcmp(key, kUriField) != 0) {
        extras.emplace(key, iter->value.GetString());
      }
    }
    getJVector(chunks, document, kChunkField);
    getJVector(chunkSizes, document, kChunkSizesField);
    return true;
  }
  clear();
  return false;
}

string FileSpec::toJson() const {
  using namespace vrs_rapidjson;
  JDocument document;
  JsonWrapper wrapper{document};
  if (!chunks.empty()) {
    serializeStringRefVector(chunks, wrapper, kChunkField);
  }
  if (!chunkSizes.empty()) {
    serializeVector<int64_t>(chunkSizes, wrapper, kChunkSizesField);
  }
  if (!fileHandlerName.empty()) {
    wrapper.addMember(kFileHandlerField, jStringRef(fileHandlerName));
  }
  if (!fileName.empty()) {
    wrapper.addMember(kFileNameField, jStringRef(fileName));
  }
  if (!uri.empty()) {
    wrapper.addMember(kUriField, jStringRef(uri));
  }
  for (const auto& extra : extras) {
    wrapper.addMember(extra.first, jStringRef(extra.second));
  }
  return jDocumentToJsonString(document);
}

int FileSpec::parseUri() {
  fileName.clear();
  chunks.resize(1);
  chunks[0].clear();
  chunkSizes.clear();
  return parseUri(uri, fileHandlerName, chunks[0], extras);
}

bool FileSpec::hasChunkSizes() const {
  return !chunkSizes.empty() && chunks.size() == chunkSizes.size();
}

int64_t FileSpec::getFileSize() const {
  int64_t fileSize = 0;
  if (hasChunkSizes()) {
    for (int64_t chunksSize : chunkSizes) {
      fileSize += chunksSize;
    }
  } else {
    if (isDiskFile() && !chunks.empty()) {
      for (const auto& chunk : chunks) {
        int64_t size = os::getFileSize(chunk);
        if (size < 0) {
          fileSize = -1;
          break;
        }
        fileSize += size;
      }
    } else {
      fileSize = -1;
    }
  }
  return fileSize;
}

string FileSpec::getSourceLocation() const {
  if (!uri.empty() && !isDiskFile()) {
    auto colon = uri.find(':');
    if (colon != string::npos) {
      auto end = colon;
      unsigned char c = 0;
      do {
        c = static_cast<unsigned char>(uri[++end]);
      } while (c == '/');
      do {
        c = static_cast<unsigned char>(uri[++end]);
      } while (isalnum(c) != 0 || c == '.' || c == '-' || c == '_');
      return uri.substr(0, end);
    }
    return uri;
  }
  return fileHandlerName;
}

string FileSpec::getEasyPath() const {
  if (!uri.empty()) {
    // The uri field, when present, is a trace of where the FileSpec came from, but it might no
    // longer be accurate, if the FileSpec was manipulated directly unceremoniously. This has caused
    // confusion when using getEasyPath() to show a file's path, on error in particular, when the
    // presence of a delegator not visible. So when showing the uri, should there be a delegator in
    // the FileSpec not directly visible in the uri, let's make sure to reveal the delegator.
    string uriDelagator;
    FileHandlerFactory& factory = FileHandlerFactory::getInstance();
    for (const auto& extra : extras) {
      if ((uri.find(extra.first) == string::npos || uri.find(extra.second) == string::npos) &&
          factory.getExtraDelegator(extra.first, extra.second) != nullptr) {
        uriDelagator = fmt::format("{} with delegator {}={}", uri, extra.first, extra.second);
        break;
      }
    }
    const string& easyPath = uriDelagator.empty() ? uri : uriDelagator;
    if (fileName.empty()) {
      return easyPath;
    }
    return fmt::format("uri: {}, name: {}", easyPath, fileName);
  }
  if (isDiskFile() && chunks.size() == 1 && extras.empty()) {
    return chunks.front();
  }
  if (!fileName.empty() && !fileHandlerName.empty()) {
    return "storage: " + fileHandlerName + ", name: " + fileName;
  }
  if (chunks.size() == 1 && !fileHandlerName.empty()) {
    return "storage: " + fileHandlerName + ", name: " + os::getFilename(chunks[0]);
  }
  FileSpec simpleSpec;
  simpleSpec.fileHandlerName = fileHandlerName;
  simpleSpec.fileName = fileName;
  for (const auto& chunk : chunks) {
    const size_t kMaxPath = 40;
    if (chunk.size() > kMaxPath) {
      // truncate the middle of long paths...
      const size_t kSplitSize = (kMaxPath - 4) / 2;
      simpleSpec.chunks.emplace_back(
          chunk.substr(0, kSplitSize) + "..." +
          chunk.substr(chunk.size() - (kMaxPath - kSplitSize)));
    } else {
      simpleSpec.chunks.emplace_back(chunk);
    }
  }
  return simpleSpec.toJson();
}

string FileSpec::getFileName() const {
  if (!fileName.empty()) {
    return fileName;
  }
  if (!chunks.empty()) {
    return os::getFilename(chunks.front());
  }
  return {};
}

string FileSpec::getXXHash() const {
  XXH64Digester digester;
  digester.ingest(fileHandlerName);
  digester.ingest(fileName);
  digester.ingest(uri);
  for (const auto& chunk : chunks) {
    digester.ingest(chunk);
  }
  for (const auto& extra : extras) {
    digester.ingest(extra.first);
    digester.ingest(extra.second);
  }
  digester.ingest(chunkSizes);
  return digester.digestToString();
}

bool FileSpec::operator==(const FileSpec& rhs) const {
  auto tie = [](const FileSpec& fs) {
    return std::tie(fs.fileName, fs.fileHandlerName, fs.uri, fs.chunks, fs.chunkSizes, fs.extras);
  };
  return tie(*this) == tie(rhs);
}

const string& FileSpec::getExtra(string_view name) const {
  static const string kEmptyString;
  const auto extra = extras.find(string(name));
  return (extra == extras.end()) ? kEmptyString : extra->second;
}

bool FileSpec::getExtra(string_view name, string& outValue) const {
  const auto extra = extras.find(string(name));
  if (extra == extras.end()) {
    return false;
  }
  outValue = extra->second;
  return true;
}

bool FileSpec::hasExtra(string_view name) const {
  return extras.find(string(name)) != extras.end();
}

int FileSpec::getExtraAsInt(string_view name, int defaultValue) const {
  int result = 0;
  return helpers::getInt(extras, string(name), result) ? result : defaultValue;
}

int64_t FileSpec::getExtraAsInt64(string_view name, int64_t defaultValue) const {
  int64_t result = 0;
  return helpers::getInt64(extras, string(name), result) ? result : defaultValue;
}

uint64_t FileSpec::getExtraAsUInt64(string_view name, uint64_t defaultValue) const {
  uint64_t result = 0;
  return helpers::getUInt64(extras, string(name), result) ? result : defaultValue;
}

double FileSpec::getExtraAsDouble(string_view name, double defaultValue) const {
  double result = 0;
  return helpers::getDouble(extras, string(name), result) ? result : defaultValue;
}

bool FileSpec::getExtraAsBool(string_view name, bool defaultValue) const {
  bool result = false;
  return helpers::getBool(extras, string(name), result) ? result : defaultValue;
}

void FileSpec::unsetExtra(string_view name) {
  extras.erase(string(name));
}

int FileSpec::decodeQuery(string_view query, string& outKey, string& outValue) {
  auto equal = query.find('=');
  if (equal == string_view::npos) {
    XR_LOGW("'=' doesn't exist in query: {}", query);
    return INVALID_URI_FORMAT;
  }
  if (equal == 0) {
    XR_LOGW("Key doesn't exist in query: {}", query);
    return INVALID_URI_FORMAT;
  }
  string_view key = query.substr(0, equal);
  int status = FileSpec::urldecode(key, outKey);
  if (status != 0) {
    XR_LOGW("Failed to decode key in query {} : {}", key, query);
    return status;
  }

  string_view value = query.substr(equal + 1);
  if (value.find('=') != string_view::npos) {
    XR_LOGW("More than one '=' in query: {}", query);
    return INVALID_URI_FORMAT;
  }

  if (value.empty()) {
    XR_LOGW("Value doesn't exist in query: {}", query);
    return INVALID_URI_FORMAT;
  }

  status = FileSpec::urldecode(value, outValue);
  if (status != 0) {
    XR_LOGW("Failed to decode value in query {} : {}", value, query);
    return status;
  }
  return 0;
}

// urldecode logic is copied from Curl_urldecode in
// https://github.com/curl/curl/blob/master/lib/escape.c
#define ISXDIGIT(x) (isxdigit(static_cast<unsigned char>(x)))
inline char xdigitToChar(char xdigit) {
  return (xdigit <= '9') ? xdigit - '0' : 10 + xdigit - ((xdigit <= 'Z') ? 'A' : 'a');
}

int FileSpec::urldecode(string_view in, string& out) {
  out.clear();
  out.reserve(in.size());
  for (size_t p = 0; p < in.size(); p++) {
    char c = in[p];
    if ((c == '%') && (in.size() - p) > 2 && ISXDIGIT(in[p + 1]) && ISXDIGIT(in[p + 2])) {
      c = (xdigitToChar(in[p + 1]) << 4) | xdigitToChar(in[p + 2]);
      p += 2;
    }
    if (c < 0x20) {
      XR_LOGW("Invalid character while decoding input: {} in {}", c, in);
      return INVALID_URI_VALUE;
    }
    out += c;
  }
  return 0;
}

void FileSpec::setExtra(string_view name, string_view value) {
  extras[string(name)] = string(value);
}

void FileSpec::setExtra(string_view name, const char* value) {
  extras[string(name)] = value;
}

void FileSpec::setExtra(string_view name, bool value) {
  extras[string(name)] = value ? "1" : "0";
}

void FileSpec::setExtra(string_view name, const string& value) {
  extras[string(name)] = value;
}

} // namespace vrs
