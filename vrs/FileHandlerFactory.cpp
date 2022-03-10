// Copyright (c) Meta Platforms, Inc. and affiliates.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#define DEFAULT_LOG_CHANNEL "FileHandlerFactory"
#include <logging/Checks.h>
#include <logging/Log.h>

#include <vrs/DiskFile.h>
#include <vrs/ErrorCode.h>
#include <vrs/FileHandlerFactory.h>

using namespace std;

namespace vrs {

FileHandlerFactory& FileHandlerFactory::getInstance() {
  static FileHandlerFactory instance;
  return instance;
}

FileHandlerFactory::FileHandlerFactory() {
  registerFileHandler(make_unique<DiskFile>());
}

int FileHandlerFactory::delegateOpen(const string& path, unique_ptr<FileHandler>& outNewDelegate) {
  FileSpec fileSpec;
  int status = fileSpec.fromPathJsonUri(path);
  return status == 0 ? delegateOpen(fileSpec, outNewDelegate) : status;
}

int FileHandlerFactory::delegateOpen(
    const FileSpec& fileSpec,
    unique_ptr<FileHandler>& outNewDelegate) {
  if (!fileSpec.fileHandlerName.empty() &&
      (!outNewDelegate || outNewDelegate->getFileHandlerName() != fileSpec.fileHandlerName)) {
    unique_ptr<FileHandler> newHandler = getFileHandler(fileSpec.fileHandlerName);
    if (!newHandler) {
      XR_LOGW(
          "No FileHandler '{}' available to open '{}'",
          fileSpec.fileHandlerName,
          fileSpec.toJson());
      outNewDelegate.reset();
      return REQUESTED_FILE_HANDLER_UNAVAILABLE;
    }
    outNewDelegate = move(newHandler);
  }
  // default to a disk file
  if (!outNewDelegate) {
    outNewDelegate = make_unique<DiskFile>();
  }
  // Now delegate opening the file to the file handler, which might delegate further...
  unique_ptr<FileHandler> newDelegate;
  int status = outNewDelegate->delegateOpenSpec(fileSpec, newDelegate);
  if (newDelegate) {
    outNewDelegate.swap(newDelegate);
  }
  return status;
}

void FileHandlerFactory::registerFileHandler(unique_ptr<FileHandler>&& fileHandler) {
  XR_DEV_CHECK_FALSE(fileHandler->getFileHandlerName().empty());
  fileHandlerMap_[fileHandler->getFileHandlerName()] = move(fileHandler);
}

void FileHandlerFactory::unregisterFileHandler(const string& fileHandlerName) {
  fileHandlerMap_.erase(fileHandlerName);
}

unique_ptr<FileHandler> FileHandlerFactory::getFileHandler(const string& name) {
  XR_DEV_CHECK_FALSE(name.empty());
  auto handler = fileHandlerMap_.find(name);
  if (handler != fileHandlerMap_.end()) {
    return handler->second->makeNew();
  }
  return {};
}

} // namespace vrs
