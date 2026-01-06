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

#include <cstdio>

#include <string>

#include <vrs/os/Platform.h>

#if IS_VRS_FB_INTERNAL()
#include <vrs/os/Utils_fb.h>
#else
#include <vector>
#endif

/// Mini-OS abstraction layer. Only VRS should use these.
/// Enables the encapsulation of file system implementations,
/// without explicitly depending on boost or std::filesystem, which may or may be available.

namespace vrs {
namespace os {

using std::string;

/// FILE helpers
std::FILE* fileOpen(const string& path, const char* modes);
int fileClose(std::FILE* file);
size_t fileRead(void* buf, size_t elementSize, size_t elementCount, std::FILE* file);
size_t fileWrite(const void* buf, size_t elementSize, size_t elementCount, std::FILE* file);
int64_t fileTell(std::FILE* file);
int fileSeek(std::FILE* file, int64_t offset, int origin);
int fileSetSize(std::FILE* file, int64_t size);

inline bool fileWriteString(FILE* f, const string& str) {
  return os::fileWrite(str.c_str(), 1, str.size(), f) == str.size();
}

/// Misc helpers
int remove(const string& path); // file or folder
int rename(const string& originalName, const string& newName); // file or folder
bool getLinkedTarget(const string& sourcePath, string& outLinkedPath);
int hidePath(const string& path, bool hide = true); // only does something on Windows
void shortenPath(string& path); // only does something on Windows, maybe...
string sanitizeFileName(const string& filename);
string randomName(int length);
const string& getOsTempFolder(); // get the OS's temp (shared) folder
const string& getTempFolder(); // get unique subfolder in the OS's temp folder
string getUniquePath(const string& baseName, size_t randomSuffixLength = 5);
string makeUniqueFolder(const string& baseName = {}, size_t randomSuffixLength = 10);

/// Error helpers
int getLastFileError();
string fileErrorToString(int errnum);

#if IS_VRS_OSS_CODE()

/// Path joining helpers
string pathJoin(const string& a, const string& b);
string pathJoin(const string& a, const string& b, const string& c);
template <class... Args>
string pathJoin(const string& a, const string& b, const string& c, Args... args) {
  return vrs::os::pathJoin(vrs::os::pathJoin(a, b, c), args...);
}

/// Directory making helpers
int makeDir(const string& dir);
int makeDirectories(const string& dir);

/// File path helpers
bool isDir(const string& path);
bool isFile(const string& path);
std::vector<string> listDir(const string& dir);
bool pathExists(const string& path);
int64_t getFileSize(const string& path);
string getFilename(const string& path);
string getParentFolder(const string& path);

/// For testing and other tools
const string& getHomeFolder();
string getCurrentExecutablePath();

#endif // IS_VRS_OSS_CODE()

} // namespace os
} // namespace vrs
