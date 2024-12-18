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

#define DEFAULT_LOG_CHANNEL "FileHandlerFactory"
#include <logging/Checks.h>
#include <logging/Log.h>

#include <vrs/DiskFile.h>
#include <vrs/ErrorCode.h>
#include <vrs/FileHandlerFactory.h>
#include <vrs/helpers/FileMacros.h>

using namespace std;

namespace vrs {

FileHandlerFactory& FileHandlerFactory::getInstance() {
  static FileHandlerFactory instance;
  return instance;
}

FileHandlerFactory::FileHandlerFactory() {
  registerFileHandler(make_unique<DiskFile>());
#if VRS_ASYNC_DISKFILE_SUPPORTED()
  registerFileHandler(make_unique<AsyncDiskFile>());
#endif
}

int FileHandlerFactory::delegateOpen(const string& path, unique_ptr<FileHandler>& outNewDelegate) {
  FileSpec fileSpec;
  int status = fileSpec.fromPathJsonUri(path);
  return status == 0 ? delegateOpen(fileSpec, outNewDelegate) : status;
}

int FileHandlerFactory::delegateOpen(
    const FileSpec& fileSpec,
    unique_ptr<FileHandler>& outNewDelegate) {
  FileDelegator* delegator = getExtraDelegator(fileSpec);
  if (delegator != nullptr) {
    return delegator->delegateOpen(fileSpec, outNewDelegate);
  }
  if (!fileSpec.fileHandlerName.empty() &&
      (!outNewDelegate || outNewDelegate->getFileHandlerName() != fileSpec.fileHandlerName)) {
    delegator = getFileDelegator(fileSpec.fileHandlerName);
    if (delegator != nullptr) {
      return delegator->delegateOpen(fileSpec, outNewDelegate);
    }

    unique_ptr<FileHandler> newHandler = getFileHandler(fileSpec.fileHandlerName);
    if (!newHandler) {
      XR_LOGW(
          "No FileHandler '{}' available to open '{}'",
          fileSpec.fileHandlerName,
          fileSpec.toJson());
      outNewDelegate.reset();
      return REQUESTED_FILE_HANDLER_UNAVAILABLE;
    }
    outNewDelegate = std::move(newHandler);
  }
  // default to a disk file
  if (!outNewDelegate) {
    outNewDelegate = make_unique<DiskFile>();
  }
  // Now delegate opening the file to the file handler, which might delegate further...
  unique_ptr<FileHandler> newDelegate;
  int status = outNewDelegate->delegateOpen(fileSpec, newDelegate);
  if (newDelegate) {
    outNewDelegate.swap(newDelegate);
  }
  return status;
}

int FileHandlerFactory::parseUri(FileSpec& inOutFileSpec, size_t colonIndex) {
  FileDelegator* delegator = getFileDelegator(inOutFileSpec.fileHandlerName);
  if (delegator != nullptr) {
    IF_ERROR_RETURN(delegator->parseUri(inOutFileSpec, colonIndex));
  } else {
    unique_ptr<FileHandler> fileHandler = getFileHandler(inOutFileSpec.fileHandlerName);
    if (fileHandler) {
      IF_ERROR_RETURN(fileHandler->parseUri(inOutFileSpec, colonIndex));
    } else {
      IF_ERROR_RETURN(inOutFileSpec.parseUri());
    }
  }

  if (!inOutFileSpec.extras.empty() && (delegator = getExtraDelegator(inOutFileSpec)) != nullptr) {
    IF_ERROR_RETURN(delegator->parseUri(inOutFileSpec, colonIndex));
  }
  return SUCCESS;
}

void FileHandlerFactory::registerFileHandler(unique_ptr<FileHandler>&& fileHandler) {
  unique_lock<mutex> lock(mutex_);
  const auto fileHandlerName = fileHandler->getFileHandlerName();
  XR_DEV_CHECK_FALSE(fileHandlerName.empty());
  fileHandlerMap_[fileHandlerName] = std::move(fileHandler);
}

void FileHandlerFactory::unregisterFileHandler(const string& fileHandlerName) {
  unique_lock<mutex> lock(mutex_);
  fileHandlerMap_.erase(fileHandlerName);
}

unique_ptr<FileHandler> FileHandlerFactory::getFileHandler(const string& name) {
  unique_lock<mutex> lock(mutex_);
  XR_DEV_CHECK_FALSE(name.empty());
  auto handler = fileHandlerMap_.find(name);
  if (handler != fileHandlerMap_.end()) {
    return handler->second->makeNew();
  }
  return {};
}

void FileHandlerFactory::registerFileDelegator(
    const string& name,
    unique_ptr<FileDelegator>&& delegator) {
  unique_lock<mutex> lock(mutex_);
  fileDelegatorMap_[name] = std::move(delegator);
}

void FileHandlerFactory::unregisterFileDelegator(const string& name) {
  unique_lock<mutex> lock(mutex_);
  fileDelegatorMap_.erase(name);
}

void FileHandlerFactory::registerExtraDelegator(
    const string& extraName,
    const string& extraValue,
    unique_ptr<FileDelegator>&& delegator) {
  XR_DEV_CHECK_FALSE(extraName.empty());
  XR_DEV_CHECK_FALSE(extraValue.empty());
  unique_lock<mutex> lock(mutex_);
  extraDelegatorMap_[extraName][extraValue] = std::move(delegator);
}

void FileHandlerFactory::unregisterExtraDelegator(
    const string& extraName,
    const string& extraValue) {
  XR_DEV_CHECK_FALSE(extraName.empty());
  XR_DEV_CHECK_FALSE(extraValue.empty());
  unique_lock<mutex> lock(mutex_);
  auto& extra = extraDelegatorMap_[extraName];
  extra.erase(extraValue);
  if (extra.empty()) {
    extraDelegatorMap_.erase(extraName);
  }
}

FileDelegator* FileHandlerFactory::getExtraDelegator(const FileSpec& fileSpec) {
  unique_lock<mutex> lock(mutex_);
  for (const auto& iter : extraDelegatorMap_) {
    const string& extraName = iter.first;
    const string& extraValue = fileSpec.getExtra(extraName);
    if (!extraValue.empty()) {
      const auto& delegateIter = iter.second.find(extraValue);
      if (delegateIter != iter.second.end()) {
        return delegateIter->second.get();
      } else {
        XR_LOGE("No {} delegator named {} was registered.", extraName, extraValue);
        class FailedDelegator : public FileDelegator {
         public:
          int delegateOpen(const FileSpec&, unique_ptr<FileHandler>&) override {
            return REQUESTED_DELEGATOR_UNAVAILABLE;
          }
          int parseUri(FileSpec&, size_t) const override {
            return REQUESTED_DELEGATOR_UNAVAILABLE;
          }
        };
        static FailedDelegator sFailedDelegator;
        return &sFailedDelegator;
      }
    }
  }
  return nullptr;
}

FileDelegator* FileHandlerFactory::getFileDelegator(const string& name) {
  unique_lock<mutex> lock(mutex_);
  auto delegator = fileDelegatorMap_.find(name);
  return (delegator == fileDelegatorMap_.end()) ? nullptr : delegator->second.get();
}

unique_ptr<WriteFileHandler> WriteFileHandler::make(const string& fileHandlerName) {
  unique_ptr<WriteFileHandler> file{dynamic_cast<WriteFileHandler*>(
      FileHandlerFactory::getInstance().getFileHandler(fileHandlerName).release())};
  return file;
}

} // namespace vrs
