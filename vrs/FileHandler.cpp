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

#include <vrs/helpers/EnumStringConverter.h>
#include <vrs/helpers/Rapidjson.hpp>
#include <vrs/helpers/Strings.h>
#include <vrs/os/Utils.h>
#include <vrs/utils/xxhash/xxhash.h>

#include "DiskFile.h"
#include "ErrorCode.h"
#include "FileHandlerFactory.h"

using namespace std;

namespace {
using vrs::CachingStrategy;

const char* sCachingStrategyNames[] = {
    "Undefined",
    "Passive",
    "Streaming",
    "StreamingBidirectional",
    "StreamingBackward",
    "ReleaseAfterRead"};
struct CachingStrategyConverter : public EnumStringConverter<
                                      CachingStrategy,
                                      sCachingStrategyNames,
                                      COUNT_OF(sCachingStrategyNames),
                                      CachingStrategy::Undefined,
                                      CachingStrategy::Undefined> {
  static_assert(
      cNamesCount == vrs::enumCount<CachingStrategy>(),
      "Missing CachingStrategy name definitions");
};

} // namespace

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

string toString(CachingStrategy cachingStrategy) {
  return CachingStrategyConverter::toString(cachingStrategy);
}

template <>
CachingStrategy toEnum<>(const string& name) {
  return CachingStrategyConverter::toEnumNoCase(name.c_str());
}

} // namespace vrs
