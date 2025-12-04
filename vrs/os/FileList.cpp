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

#include "FileList.h"

#include <cerrno>

#include <algorithm>

#include <vrs/helpers/Strings.h>
#include <vrs/os/Platform.h>

using namespace std;

#if IS_VRS_OSS_TARGET_PLATFORM()
#include <filesystem>
namespace fs = std::filesystem;
#endif

namespace vrs {
namespace os {

static bool beforefileName(const std::string& left, const std::string& right) {
  return helpers::beforeFileName(left.c_str(), right.c_str());
}

int getFilesAndFolders(const string& path, vector<string>& inOutFiles, vector<string>* outFolders) {
  if (outFolders) {
    outFolders->clear();
  }
#if IS_VRS_OSS_TARGET_PLATFORM()
  std::error_code ec;
  fs::path fspath(path);
  fs::file_status status = fs::status(fspath);
  if (!exists(status)) {
    return ENOENT; // file not found
  } else if (status.type() == fs::file_type::directory) {
    for (fs::directory_iterator it(fspath, ec), eit; it != eit; it.increment(ec)) {
      if (ec) {
        return ec.value() != 0 ? ec.value() : -1;
      }
      if (!fs::is_symlink(it->symlink_status())) {
        status = it->status();
        if (status.type() == fs::file_type::regular) {
#if IS_MAC_PLATFORM()
          if (it->path().filename() == ".DS_Store") {
            continue;
          }
#endif
          inOutFiles.emplace_back(it->path().generic_string());
        } else if (outFolders && status.type() == fs::file_type::directory) {
          outFolders->emplace_back(it->path().generic_string());
        }
      }
    }
  } else if (status.type() == fs::file_type::regular) {
    inOutFiles.emplace_back(path);
  }
  sort(inOutFiles.begin(), inOutFiles.end(), beforefileName);
  if (outFolders) {
    sort(outFolders->begin(), outFolders->end());
  }
#endif
  return 0;
}

int getFileList(const string& path, vector<string>& inOutFiles, int maxRecursiveDepth) {
  vector<string> subfiles, subfolders;
  int status = getFilesAndFolders(path, subfiles, &subfolders);
  if (status != 0) {
    return status;
  }
  inOutFiles.reserve(inOutFiles.size() + subfiles.size());
  for (string& subfile : subfiles) {
    inOutFiles.emplace_back(std::move(subfile)); // avoid string copy
  }
  if (--maxRecursiveDepth >= 0) {
    for (const string& subfolder : subfolders) {
      string subRelativePrefix;
      status = getFileList(subfolder, inOutFiles, maxRecursiveDepth);
      if (status != 0) {
        return status;
      }
    }
  }
  return 0;
}

} // namespace os
} // namespace vrs
