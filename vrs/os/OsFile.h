// Facebook Technologies, LLC Proprietary and Confidential.

#pragma once

#include <cstdint>
#include <cstdio>
#include <string>

#include <system_utils/os/Utils.h>

#include <vrs/os/Utils.h>

namespace coretech {

inline int osRemove(const std::string& filename) {
  return vrs::os::remove(filename);
}
inline std::string osGetUniquePath(const std::string& baseName, size_t randomSuffixLength = 5) {
  return vrs::os::getUniquePath(baseName, randomSuffixLength);
}

/// The following utilities are now implemented in the buck module
/// "//arvr/libraries/system_utils/system_utils/os:utils" using the namespace avr::system_utils::os
/// To help a smooth transition over multiple diffs, we inline-forward all these calls.

/// Path joining helpers
inline std::string osPathJoin(const std::string& a, const std::string& b) {
  return arvr::system_utils::os::pathJoin(a, b);
}
inline std::string osPathJoin(const std::string& a, const std::string& b, const std::string& c) {
  return arvr::system_utils::os::pathJoin(a, b, c);
}
template <class... Args>
std::string
osPathJoin(const std::string& a, const std::string& b, const std::string& c, Args... args) {
  return arvr::system_utils::os::pathJoin(osPathJoin(a, b, c), args...);
}

/// Directory making helpers
inline int osMkdir(const std::string& dir) {
  return arvr::system_utils::os::makeDir(dir);
}
inline int osMkDirectories(const std::string& dir) {
  return arvr::system_utils::os::makeDirectories(dir);
}

/// File path helpers
inline bool osIsDir(const std::string& filename) {
  return arvr::system_utils::os::isDir(filename);
}
inline bool osIsFile(const std::string& filename) {
  return arvr::system_utils::os::isFile(filename);
}
inline bool osPathExists(const std::string& filename) {
  return arvr::system_utils::os::pathExists(filename);
}
inline int64_t osGetFileSize(const std::string& filename) {
  return arvr::system_utils::os::getFileSize(filename);
}
inline std::string osGetFilename(const std::string& path) {
  return arvr::system_utils::os::getFilename(path);
}
inline std::string osGetParentFolder(const std::string& path) {
  return arvr::system_utils::os::getParentFolder(path);
}
inline const std::string& osGetHomeFolder() {
  return arvr::system_utils::os::getHomeFolder();
}
inline std::string osGetCurrentExecutablePath() {
  return arvr::system_utils::os::getCurrentExecutablePath();
}

} // namespace coretech
