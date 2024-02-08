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

#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

#include <vrs/os/Platform.h>

#if IS_VRS_FB_INTERNAL()
#include <vrs/os/Utils_fb.h>
#endif

/// Mini-OS abstraction layer. Only VRS should use these.
/// Enables the encapsulation of file system implementations,
/// without explicitly depending on boost or std::filesystem, which may or may be available.

namespace vrs {
namespace os {

#if IS_WINDOWS_PLATFORM()
std::wstring osUtf8ToWstring(const std::string& utf8String);
std::string osWstringtoUtf8(const std::wstring& wstring);
#endif // IS_WINDOWS_PLATFORM()

/// FILE helpers
std::FILE* fileOpen(const std::string& path, const char* modes);
int fileClose(std::FILE* file);
size_t fileRead(void* buf, size_t elementSize, size_t elementCount, std::FILE* file);
size_t fileWrite(const void* buf, size_t elementSize, size_t elementCount, std::FILE* file);
int64_t fileTell(std::FILE* file);
int fileSeek(std::FILE* file, int64_t offset, int origin);
int fileSetSize(std::FILE* file, int64_t size);

/// Misc helpers
int remove(const std::string& path); // file or folder
int rename(const std::string& originalName, const std::string& newName); // file or folder
bool getLinkedTarget(const std::string& sourcePath, std::string& outLinkedPath);
int hidePath(const std::string& path, bool hide = true); // only does something on Windows
std::string sanitizeFileName(const std::string& filename);
std::string randomName(int length);
const std::string& getTempFolder();
std::string getUniquePath(const std::string& baseName, size_t randomSuffixLength = 5);

/// Error helpers
int getLastFileError();
std::string fileErrorToString(int errnum);

#if IS_VRS_OSS_CODE()

/// Path joining helpers
std::string pathJoin(const std::string& a, const std::string& b);
std::string pathJoin(const std::string& a, const std::string& b, const std::string& c);
template <class... Args>
std::string
pathJoin(const std::string& a, const std::string& b, const std::string& c, Args... args) {
  return vrs::os::pathJoin(vrs::os::pathJoin(a, b, c), args...);
}

/// Directory making helpers
int makeDir(const std::string& dir);
int makeDirectories(const std::string& dir);

/// File path helpers
bool isDir(const std::string& path);
bool isFile(const std::string& path);
std::vector<std::string> listDir(const std::string& dir);
bool pathExists(const std::string& path);
int64_t getFileSize(const std::string& path);
std::string getFilename(const std::string& path);
std::string getParentFolder(const std::string& path);

/// For testing and other tools
const std::string& getHomeFolder();
std::string getCurrentExecutablePath();

#endif // IS_VRS_OSS_CODE()

} // namespace os
} // namespace vrs
