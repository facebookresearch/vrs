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

#include "FileHandler.h"

#include <cstring>
#include <iostream>
#include <sstream>

#define DEFAULT_LOG_CHANNEL "FileHandler"
#include <logging/Log.h>
#include <logging/Verify.h>

#include <vrs/helpers/Rapidjson.hpp>
#include <vrs/helpers/Strings.h>
#include <vrs/os/Utils.h>
#include <vrs/utils/xxhash/xxhash.h>

#include "DiskFile.h"
#include "ErrorCode.h"
#include "FileHandlerFactory.h"

using namespace std;

namespace vrs {

int FileHandler::open(const string& filePath) {
  FileSpec fileSpec;
  int status = parseFilePath(filePath, fileSpec);
  if (status != 0) {
    close();
    return status;
  }
  if (!isFileHandlerMatch(fileSpec)) {
    return FILE_HANDLER_MISMATCH;
  }
  return openSpec(fileSpec);
}

int FileHandler::delegateOpen(const string& path, unique_ptr<FileHandler>& outNewDelegate) {
  FileSpec fileSpec;
  int status = parseFilePath(path, fileSpec);
  if (status != 0) {
    close();
    outNewDelegate.reset();
    return status;
  }
  return delegateOpenSpec(fileSpec, outNewDelegate);
}

int FileHandler::delegateOpenSpec(
    const FileSpec& fileSpec,
    unique_ptr<FileHandler>& outNewDelegate) {
  // if provided with a delegate, then ask the delegate first...
  if (outNewDelegate) {
    if (outNewDelegate->openSpec(fileSpec) == SUCCESS) {
      return SUCCESS;
    }
    outNewDelegate.reset();
  }
  int status = openSpec(fileSpec);
  if (status == FILE_HANDLER_MISMATCH) {
    return FileHandlerFactory::getInstance().delegateOpen(fileSpec, outNewDelegate);
  }
  return status;
}

bool FileHandler::isReadOnly() const {
  return true;
}

bool FileHandler::isRemoteFileSystem() const {
  return true; // everything but disk file is pretty much a remote file system...
}

int FileHandler::parseFilePath(const string& filePath, FileSpec& outFileSpec) const {
  if (filePath.empty()) {
    outFileSpec.clear();
    return INVALID_PARAMETER;
  } else if (filePath.front() != '{') {
    outFileSpec.clear();
    outFileSpec.chunks = {filePath};
    // In this flow, we presume the FileHandler ("this") is the correct FileHandler
    outFileSpec.fileHandlerName = getFileHandlerName();
  } else {
    if (!outFileSpec.fromJson(filePath)) {
      outFileSpec.clear();
      return FILEPATH_PARSE_ERROR;
    }
    if (!isFileHandlerMatch(outFileSpec)) {
      XR_LOGE(
          "FileHandler mismatch. This FileHandler is '{}', but this path requires "
          "a FileHandler for '{}'.",
          getFileHandlerName(),
          outFileSpec.fileHandlerName);
      return FILE_HANDLER_MISMATCH;
    }
  }
  return SUCCESS;
}

bool FileHandler::isFileHandlerMatch(const FileSpec& fileSpec) const {
  return fileSpec.fileHandlerName.empty() || getFileHandlerName() == fileSpec.fileHandlerName;
}

int FileHandler::parseUri(FileSpec& /*inOutFileSpec*/, size_t /*colonIndex*/) const {
  return SUCCESS;
}

static const char* kChunkField = "chunks";
static const char* kChunkSizesField = "chunk_sizes";
static const char* kFileHandlerField = "storage";
static const char* kFileNameField = "filename";
static const char* kUriField = "source_uri";

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
  return fileHandlerName.empty() || fileHandlerName == DiskFile::staticName();
}

int FileSpec::parseUri(
    const string& uri,
    string& outScheme,
    string& outPath,
    map<string, string>& outQueryParams) {
  // Parse URI following https://en.wikipedia.org/wiki/Uniform_Resource_Identifier.
  // URI should look like <schema>:<path>?<query> while <schema> corresponds to file handler name.
  // The query should be used to specify additional fields for each file handler.
  outScheme.clear();
  outPath.clear();
  outQueryParams.clear();

  auto colon = uri.find(':');
  if (colon == 0) {
    XR_LOGE("Schema doesn't exist in uri before ':': {}", uri);
    return INVALID_URI_FORMAT;
  }
  // validate url schema
  for (size_t p = 0; p < colon && colon != uri.npos; p++) {
    unsigned char c = static_cast<unsigned char>(uri[p]);
    // from https://en.wikipedia.org/wiki/Uniform_Resource_Identifier#Generic_syntax
    if (p == 0 && !isalpha(c)) {
      XR_LOGE("Schema doesn't start with alphabet {}: {}", c, uri);
      return INVALID_URI_FORMAT;
    }
    if (p != 0 && !(isalnum(c) || c == '.' || c == '-' || c == '+' || c == '_')) {
      XR_LOGE("Schema contains an invalid character {}: {}", c, uri);
      return INVALID_URI_FORMAT;
    }
  }

  auto query = uri.find('?');

  // length of path should be longer than 0
  if (query <= colon + 1 || (query == uri.npos && colon >= uri.size() - 1)) {
    XR_LOGE("Path doesn't exist in uri: {}", uri);
    return INVALID_URI_FORMAT;
  }

  if (query != uri.npos) {
    size_t start = query + 1;
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

  if (colon != uri.npos && colon > 0) {
    outScheme = uri.substr(0, colon);
  }

  string path =
      (colon != uri.npos) ? uri.substr(colon + 1, query - colon - 1) : uri.substr(0, query);
  int status = FileSpec::urldecode(path, outPath);
  if (status != 0) {
    XR_LOGE("Path contains invalid character {}", path);
    outScheme.clear();
    outPath.clear();
    outQueryParams.clear();
    return INVALID_URI_VALUE;
  }

  return SUCCESS;
}

int FileSpec::fromPathJsonUri(const string& pathJsonUri) {
  clear();
  if (pathJsonUri.front() == '{') {
    return fromJson(pathJsonUri) ? SUCCESS : FILEPATH_PARSE_ERROR;
  }
  auto colon = pathJsonUri.find(':');
  bool isUri = colon != pathJsonUri.npos && colon > 1;
  for (size_t p = 0; p < colon && isUri; p++) {
    unsigned char c = static_cast<unsigned char>(pathJsonUri[p]);
    // from https://en.wikipedia.org/wiki/Uniform_Resource_Identifier#Generic_syntax
    isUri = (p == 0) ? isalpha(c) : isalnum(c) || c == '.' || c == '-' || c == '+' || c == '_';
  }
  if (!isUri) {
    chunks = {pathJsonUri};
    fileHandlerName = DiskFile::staticName();
    return SUCCESS;
  }
  fileHandlerName = pathJsonUri.substr(0, colon);
  uri = pathJsonUri;
  // give a chance to a file handler named after the uri scheme, if any, to parse the uri.
  auto fileHandler = FileHandlerFactory::getInstance().getFileHandler(fileHandlerName);
  if (fileHandler) {
    return fileHandler->parseUri(*this, colon);
  }
  chunks.resize(1);
  return parseUri(uri, fileHandlerName, chunks[0], extras);
}

bool FileSpec::fromJson(const string& jsonStr) {
  using namespace fb_rapidjson;
  JDocument document;
  document.Parse(jsonStr.c_str());
  if (document.IsObject()) {
    getString(fileName, document, kFileNameField);
    getString(fileHandlerName, document, kFileHandlerField);
    getString(uri, document, kUriField);
    extras.clear();
    for (auto iter = document.MemberBegin(); iter != document.MemberEnd(); ++iter) {
      const auto* key = iter->name.GetString();
      if (iter->value.IsString() && strcmp(key, kChunkField) != 0 &&
          strcmp(key, kFileHandlerField) != 0 && strcmp(key, kFileNameField) != 0 &&
          strcmp(key, kUriField) != 0) {
        extras.emplace(key, iter->value.GetString());
      }
    }
    getVector(chunks, document, fb_rapidjson::StringRef(kChunkField));
    getVector(chunkSizes, document, fb_rapidjson::StringRef(kChunkSizesField));
    return true;
  }
  clear();
  return false;
}

string FileSpec::toJson() const {
  using namespace fb_rapidjson;
  JDocument document;
  document.SetObject();
  JsonWrapper wrapper{document, document.GetAllocator()};
  if (!chunks.empty()) {
    serializeVector<string>(chunks, wrapper, StringRef(kChunkField));
  }
  if (!chunkSizes.empty()) {
    serializeVector<int64_t>(chunkSizes, wrapper, StringRef(kChunkSizesField));
  }
  if (!fileHandlerName.empty()) {
    wrapper.addMember(kFileHandlerField, fileHandlerName);
  }
  if (!fileName.empty()) {
    wrapper.addMember(kFileNameField, fileName);
  }
  if (!uri.empty()) {
    wrapper.addMember(kUriField, uri);
  }
  for (const auto& extra : extras) {
    wrapper.addMember(extra.first, extra.second);
  }
  return jDocumentToJsonString(document);
}

bool FileSpec::hasChunkSizes() const {
  return !chunkSizes.empty() && chunks.size() == chunkSizes.size();
}

int64_t FileSpec::getFileSize() const {
  if (!hasChunkSizes()) {
    return -1;
  }
  int64_t fileSize = 0;
  for (int64_t chunksSize : chunkSizes) {
    fileSize += chunksSize;
  }
  return fileSize;
}

string FileSpec::getSourceLocation() const {
  if (!uri.empty()) {
    auto colon = uri.find(':');
    if (colon != string::npos) {
      auto end = colon;
      unsigned char c;
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
    if (fileName.empty()) {
      return uri;
    }
    return "uri: " + uri + ", name: " + fileName;
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

string FileSpec::getXXHash() const {
  XXH64Digester digester;
  digester.update(fileHandlerName);
  digester.update(fileName);
  digester.update(uri);
  for (const auto& chunk : chunks) {
    digester.update(chunk);
  }
  for (auto extra : extras) {
    digester.update(extra.first);
    digester.update(extra.second);
  }
  digester.update(chunkSizes);
  return digester.digestToString();
}

bool FileSpec::operator==(const FileSpec& rhs) const {
  return fileName == rhs.fileName && fileHandlerName == rhs.fileHandlerName && uri == rhs.uri &&
      chunks == rhs.chunks && chunkSizes == rhs.chunkSizes && extras == rhs.extras;
}

string FileSpec::getExtra(const string& name) const {
  const auto extra = extras.find(name);
  return (extra == extras.end()) ? string() : extra->second;
}

bool FileSpec::hasExtra(const string& name) const {
  return extras.find(name) != extras.end();
}

int FileSpec::getExtraAsInt(const string& name, int defaultValue) const {
  int result;
  return helpers::getInt(extras, name, result) ? result : defaultValue;
}

int64_t FileSpec::getExtraAsInt64(const string& name, int64_t defaultValue) const {
  int64_t result;
  return helpers::getInt64(extras, name, result) ? result : defaultValue;
}

uint64_t FileSpec::getExtraAsUInt64(const string& name, uint64_t defaultValue) const {
  uint64_t result;
  return helpers::getUInt64(extras, name, result) ? result : defaultValue;
}

double FileSpec::getExtraAsDouble(const string& name, double defaultValue) const {
  double result;
  return helpers::getDouble(extras, name, result) ? result : defaultValue;
}

bool FileSpec::getExtraAsBool(const string& name, bool defaultValue) const {
  bool result;
  return helpers::getBool(extras, name, result) ? result : defaultValue;
}

int FileSpec::decodeQuery(const string& query, string& outKey, string& outValue) {
  auto equal = query.find('=');
  if (equal == query.npos) {
    XR_LOGW("'=' doesn't exist in query: {}", query);
    return INVALID_URI_FORMAT;
  }
  if (equal == 0) {
    XR_LOGW("Key doesn't exist in query: {}", query);
    return INVALID_URI_FORMAT;
  }
  string key = query.substr(0, equal);
  int status = FileSpec::urldecode(key, outKey);
  if (status != 0) {
    XR_LOGW("Failed to decode key in query {} : {}", key, query);
    return status;
  }

  string value = query.substr(equal + 1);
  if (value.find('=') != value.npos) {
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

int FileSpec::urldecode(const string& in, string& out) {
  out.clear();
  out.reserve(in.size());
  for (size_t p = 0; p < in.size(); p++) {
    char c = in[p];
    if (c == '+') {
      c = ' ';
    } else if ((c == '%') && (in.size() - p) > 2 && ISXDIGIT(in[p + 1]) && ISXDIGIT(in[p + 2])) {
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

} // namespace vrs
