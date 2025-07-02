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
using namespace vrs;
using vrs::CachingStrategy;

string_view sCachingStrategyNames[] = {
    "Undefined",
    "Passive",
    "Streaming",
    "StreamingBidirectional",
    "StreamingBackward",
    "ReleaseAfterRead"};
ENUM_STRING_CONVERTER(CachingStrategy, sCachingStrategyNames, CachingStrategy::Undefined);

} // namespace

namespace vrs {

const string& FileHandler::getWriteFileHandlerName() const {
  static const string kEmptryString;
  return kEmptryString;
}

unique_ptr<FileHandler> FileHandler::makeOpen(const string& filePath) {
  unique_ptr<FileHandler> delegate;
  int status = FileHandlerFactory::getInstance().delegateOpen(filePath, delegate);
  if (status != 0) {
    XR_LOGE("Could not open '{}': {}", filePath, errorCodeToMessage(status));
    return nullptr;
  }
  return delegate;
}

unique_ptr<FileHandler> FileHandler::makeOpen(const FileSpec& fileSpec) {
  unique_ptr<FileHandler> delegate;
  int status = FileHandlerFactory::getInstance().delegateOpen(fileSpec, delegate);
  if (status != 0) {
    XR_LOGE("Could not open '{}': {}", fileSpec.toPathJsonUri(), errorCodeToMessage(status));
    return nullptr;
  }
  return delegate;
}

int FileHandler::open(const string& filePath) {
  FileSpec fileSpec;
  int status = fileSpec.fromPathJsonUri(filePath, getFileHandlerName());
  if (status != 0) {
    close();
    return status;
  }
  if (!isFileHandlerMatch(fileSpec)) {
    XR_LOGE(
        "FileHandler mismatch. This FileHandler is '{}', but this path requires "
        "a FileHandler for '{}'.",
        getFileHandlerName(),
        fileSpec.fileHandlerName);
    return FILE_HANDLER_MISMATCH;
  }
  return openSpec(fileSpec);
}

int FileHandler::delegateOpen(const FileSpec& fileSpec, unique_ptr<FileHandler>& outNewDelegate) {
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

bool FileHandler::isFileHandlerMatch(const FileSpec& fileSpec) const {
  return fileSpec.fileHandlerName.empty() || getFileHandlerName() == fileSpec.fileHandlerName;
}

DEFINE_ENUM_CONVERTERS(CachingStrategy);

} // namespace vrs
