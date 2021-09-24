// (c) Facebook, Inc. and its affiliates. Confidential and proprietary.

#pragma once

#include <cstdint>
#include <cstdio>
#include <string>

#include <system_utils/os/Utils.h>

/// Please do not use this outside of VRS.
/// Only VRS should use these methods, designed to abstract OS operations for open sourcing.
/// This version relies on arvr::system_utils::os
/// The open source version of vrs will use a separate implementation of vrs/os/Utils.cpp|.h

namespace vrs::os {

/// FILE helpers
std::FILE* fileOpen(const std::string& filename, const char* modes);
int fileClose(std::FILE* file);
size_t fileRead(void* buf, size_t elementSize, size_t elementCount, std::FILE* file);
size_t fileWrite(const void* buf, size_t elementSize, size_t elementCount, std::FILE* file);
int64_t fileTell(std::FILE* file);
int fileSeek(std::FILE* file, int64_t offset, int origin);
int fileSetSize(std::FILE* file, int64_t size);

/// Misc helpers
int remove(const std::string& filename); // file or folder
int rename(const std::string& originalName, const std::string& newName); // file or folder
bool getLinkedTarget(const std::string& sourcePath, std::string& outLinkedPath);
int hidePath(const std::string& path, bool hide = true); // only does something on Windows
std::string sanitizeFileName(const std::string& filename);
const std::string& getTempFolder();
std::string getUniquePath(const std::string& baseName, size_t randomSuffixLength = 5);

/// Error helpers
int getLastFileError();
std::string fileErrorToString(int errnum);

/// The following utilities are now implemented in the buck module
/// "//arvr/libraries/system_utils/system_utils/os:utils" using the namespace avr::system_utils::os
/// To help a smooth transition over multiple diffs, we inline-forward all these calls.
/// In the end, only vrs will continue to use these aliases, designed to enable an open source
/// version that won't rely on arvr::system_utils::os
/// The open source version of vrs will use a separate implementation of OsFile.cpp/.h

/// Path joining helpers
inline std::string pathJoin(const std::string& a, const std::string& b) {
  return arvr::system_utils::os::pathJoin(a, b);
}
inline std::string pathJoin(const std::string& a, const std::string& b, const std::string& c) {
  return arvr::system_utils::os::pathJoin(a, b, c);
}
template <class... Args>
std::string
pathJoin(const std::string& a, const std::string& b, const std::string& c, Args... args) {
  return arvr::system_utils::os::pathJoin(arvr::system_utils::os::pathJoin(a, b, c), args...);
}

/// Directory making helpers
inline int makeDir(const std::string& dir) {
  return arvr::system_utils::os::makeDir(dir);
}
inline int makeDirectories(const std::string& dir) {
  return arvr::system_utils::os::makeDirectories(dir);
}

/// File path helpers
inline bool isDir(const std::string& filename) {
  return arvr::system_utils::os::isDir(filename);
}
inline bool isFile(const std::string& filename) {
  return arvr::system_utils::os::isFile(filename);
}
inline bool pathExists(const std::string& filename) {
  return arvr::system_utils::os::pathExists(filename);
}
inline int64_t getFileSize(const std::string& filename) {
  return arvr::system_utils::os::getFileSize(filename);
}
inline std::string getFilename(const std::string& path) {
  return arvr::system_utils::os::getFilename(path);
}
inline std::string getParentFolder(const std::string& path) {
  return arvr::system_utils::os::getParentFolder(path);
}
inline const std::string& getHomeFolder() {
  return arvr::system_utils::os::getHomeFolder();
}
inline std::string getCurrentExecutablePath() {
  return arvr::system_utils::os::getCurrentExecutablePath();
}

} // namespace vrs::os
