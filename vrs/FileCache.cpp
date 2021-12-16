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

#include "FileCache.h"

#include <vrs/os/Platform.h>
#include <vrs/os/Utils.h>

#include "ErrorCode.h"

namespace vrs {

std::unique_ptr<FileCache> FileCache::sFileCache;

int FileCache::makeFileCache(const string& app, const std::string& parentFolder) {
  string appName;
#if !IS_WINDOWS_PLATFORM()
  appName = '.'; // make folder invisible
#endif
  appName += app;

  string mainFolder = parentFolder.empty() ? os::getHomeFolder() : parentFolder;
  if (!mainFolder.empty() && mainFolder.back() != '/' && mainFolder.back() != '\\') {
    mainFolder += '/';
  }
  int error = 0;
  if (!os::isDir(mainFolder) && (error = os::makeDir(mainFolder)) != 0) {
    return error;
  }
  mainFolder += appName + '/';
  if (!os::isDir(mainFolder) && (error = os::makeDir(mainFolder)) != 0) {
    return error;
  }
#if IS_WINDOWS_PLATFORM()
  os::hidePath(mainFolder);
#endif
  sFileCache.reset(new FileCache(mainFolder));
  return 0;
}

void FileCache::disableFileCache() {
  sFileCache.reset();
}

FileCache* FileCache::getFileCache() {
  return sFileCache.get();
}

int FileCache::getFile(
    const std::string& domain,
    const std::string& filename,
    std::string& outFilePath) {
  string folder = mainFolder_ + domain;
  outFilePath = folder + '/' + filename;
  if (os::isFile(outFilePath)) {
    return 0;
  }
  if ((os::isDir(folder) && os::pathExists(outFilePath)) || os::makeDir(folder) != 0) {
    outFilePath.clear();
    return INVALID_DISK_DATA;
  }
  return FILE_NOT_FOUND;
}

int FileCache::getFile(const std::string& filename, std::string& outFilePath) {
  outFilePath = mainFolder_ + filename;
  if (os::isFile(outFilePath)) {
    return 0;
  }
  if (os::pathExists(outFilePath)) {
    return INVALID_DISK_DATA;
  }
  return FILE_NOT_FOUND;
}

} // namespace vrs
