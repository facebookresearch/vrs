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

#include <vrs/FileCache.h>

#include <vrs/ErrorCode.h>
#include <vrs/os/Platform.h>
#include <vrs/os/Utils.h>

using namespace std;

namespace vrs {

unique_ptr<FileCache> FileCache::sFileCache;

int FileCache::makeFileCache(const string& app, const string& parentFolder) {
  const string& mainFolderBase = parentFolder.empty() ? os::getHomeFolder() : parentFolder;
  string mainFolder;
  mainFolder.reserve(mainFolderBase.size() + app.size() + 3);
  mainFolder.append(mainFolderBase);
  if (!mainFolder.empty() && mainFolder.back() != '/' && mainFolder.back() != '\\') {
    mainFolder += '/';
  }
#if !IS_WINDOWS_PLATFORM()
  mainFolder.append("."); // make folder invisible
#endif
  mainFolder.append(app).append("/");
  int error = 0;
  if (!os::isDir(mainFolder) && (error = os::makeDirectories(mainFolder)) != 0) {
    return error;
  }
#if IS_WINDOWS_PLATFORM()
  os::hidePath(mainFolder);
#endif
  sFileCache.reset(new FileCache(std::move(mainFolder)));
  return 0;
}

void FileCache::disableFileCache() {
  sFileCache.reset();
}

FileCache* FileCache::getFileCache() {
  return sFileCache.get();
}

int FileCache::getFile(const string& domain, const string& filename, string& outFilePath) {
  string folder;
  folder.reserve(mainFolder_.size() + domain.size());
  folder.append(mainFolder_).append(domain);
  outFilePath.clear();
  outFilePath.reserve(folder.size() + filename.size() + 1);
  outFilePath.append(folder).append("/").append(filename);
  if (os::isFile(outFilePath)) {
    return 0;
  }
  if ((os::isDir(folder) && os::pathExists(outFilePath)) || os::makeDir(folder) != 0) {
    outFilePath.clear();
    return INVALID_DISK_DATA;
  }
  return FILE_NOT_FOUND;
}

int FileCache::getFile(const string& filename, string& outFilePath) {
  outFilePath.clear();
  outFilePath.reserve(mainFolder_.size() + filename.size());
  outFilePath.append(mainFolder_).append(filename);
  if (os::isFile(outFilePath)) {
    return 0;
  }
  if (os::pathExists(outFilePath)) {
    outFilePath.clear();
    return INVALID_DISK_DATA;
  }
  return FILE_NOT_FOUND;
}

} // namespace vrs
