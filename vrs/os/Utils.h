// Copyright (c) Facebook, Inc. and its affiliates. Confidential and proprietary.

#pragma once

#include <cstdint>
#include <cstdio>
#include <string>

#include <vrs/os/Platform.h>

#if IS_VRS_FB_INTERNAL()
#include <vrs/os/Utils_fb.h>
#endif

/// Mini-OS abstraction layer. Only VRS should use these.
/// Enables the encapsulation of file system implementations,
/// without explicitly depending on boost or std::filesystem, which may or may be available.

namespace vrs {
namespace os {

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
