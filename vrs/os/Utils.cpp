// Copyright (c) Facebook, Inc. and its affiliates. Confidential and proprietary.

#include <vrs/os/Utils.h>

#include <cstdio>

#include <algorithm>

#include <logging/Checks.h>

#include <vrs/helpers/Strings.h>
#include <vrs/os/Platform.h>

#if IS_WINDOWS_PLATFORM()
#include <io.h>
#include <windows.h>
#include <codecvt>
#else // !IS_WINDOWS_PLATFORM()
// Darwin's 'stat' support 64-bit if you set a flag. On linux, use 'stat64' instead.
#if IS_APPLE_PLATFORM()
#define _DARWIN_USE_64_BIT_INODE 1
#define STAT64 stat
#define LSTAT64 lstat
#include <mach-o/dyld.h>
#elif (defined(__ANDROID_API__) && __ANDROID_API__ < 21)
// Prior to Android API 21, libc has no off64_t operations
#define STAT64 stat
#define LSTAT64 lstat
#else
#define STAT64 stat64
#define LSTAT64 lstat64
#endif
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <cerrno>
#include <cstring>
#endif // !IS_WINDOWS_PLATFORM()

#if IS_VRS_OSS_CODE()
#include <boost/filesystem.hpp>
namespace fs = boost::filesystem;
using fs_error_code = boost::system::error_code;
constexpr auto kNotFoundFileType = boost::filesystem::file_type::file_not_found;
constexpr auto kRegularFileType = boost::filesystem::file_type::regular_file;
#endif

using namespace arvr;
using std::string;

namespace vrs::os {

#if IS_WINDOWS_PLATFORM()
std::wstring osUtf8ToWstring(const string& utf8String) {
  return std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>>().from_bytes(utf8String);
}
#endif // IS_WINDOWS_PLATFORM()

FILE* fileOpen(const string& path, const char* modes) {
#if IS_WINDOWS_PLATFORM()
  // On Windows, the C APIs create file handles that are inheritable by default. This is not a
  // desirable behavior, since subprocesses could inherit our handles to VRS file and create file
  // access contention bugs. The Windows-specific "N" mode makes the file handle non-inheritable.
  return ::_wfopen(osUtf8ToWstring(path).c_str(), osUtf8ToWstring(modes).append(L"N").c_str());
#else // !IS_WINDOWS_PLATFORM()
  return ::fopen(path.c_str(), modes);
#endif // !IS_WINDOWS_PLATFORM()
}

int fileClose(FILE* file) {
  return ::fclose(file);
}

size_t fileRead(void* buf, size_t elementSize, size_t elementCount, FILE* file) {
  return ::fread(buf, elementSize, elementCount, file);
}

size_t fileWrite(const void* buf, size_t elementSize, size_t elementCount, FILE* file) {
  return ::fwrite(buf, elementSize, elementCount, file);
}

int64_t fileTell(FILE* file) {
#if IS_WINDOWS_PLATFORM()
  return ::_ftelli64(file);
#elif (defined(__ANDROID_API__) && __ANDROID_API__ >= 24)
  return static_cast<int64_t>(::ftello64(file));
#else
  return ::ftello(file);
#endif
}

int fileSeek(FILE* file, int64_t offset, int origin) {
#if IS_WINDOWS_PLATFORM()
  return ::_fseeki64(file, offset, origin);
#elif (defined(__ANDROID_API__) && __ANDROID_API__ >= 24)
  if (ferror(file)) {
    ::rewind(file);
  }
  return ::fseeko64(file, static_cast<off64_t>(offset), origin);
#else
  if (ferror(file)) {
    ::rewind(file);
  }
  return ::fseeko(file, offset, origin);
#endif
}

#if IS_VRS_OSS_CODE()
int fileSetSize(std::FILE* file, int64_t size) {
#if IS_WINDOWS_PLATFORM()
  return ::_chsize_s(_fileno(file), size);
#elif (defined(__ANDROID_API__) && __ANDROID_API__ >= 21)
  return ::ftruncate64(fileno(file), static_cast<off64_t>(size));
#else
  return ::ftruncate(fileno(file), size);
#endif
}
#endif

int getLastFileError() {
  return errno;
}

int remove(const string& path) {
#if IS_WINDOWS_PLATFORM()
  return ::_wremove(osUtf8ToWstring(path).c_str());
#else // !IS_WINDOWS_PLATFORM()
  return ::remove(path.c_str());
#endif // !IS_WINDOWS_PLATFORM()
}

int rename(const string& originalName, const string& newName) {
#if IS_WINDOWS_PLATFORM()
  return ::_wrename(osUtf8ToWstring(originalName).c_str(), osUtf8ToWstring(newName).c_str());
#else // !IS_WINDOWS_PLATFORM()
  return ::rename(originalName.c_str(), newName.c_str());
#endif // !IS_WINDOWS_PLATFORM()
}

string fileErrorToString(int errnum) {
  return strerror(errnum);
}

// we can't use boost::unique_path, because std::file system doesn't have it.
// we can't use std::tmpnam, because it's deprecated, and throws a warning, breaking the build...
// So yes, we're reinventing the wheel.
string randomName(int length) {
  auto randchar = []() -> char {
    const char charset[] =
        "0123456789_"
        "abcdefghijklmnopqrstuvwxyz";
    const size_t max_index = (sizeof(charset) - 1);
    return charset[static_cast<size_t>(rand()) % max_index];
  };
  string str(length, 0);
  generate_n(str.begin(), length, randchar);
  return str;
}

#if IS_VRS_OSS_CODE()
const string& getTempFolder() {
  static string sUniqueFolderName = [] {
    fs::path tempDir;
#if IS_ANDROID_PLATFORM()
    tempDir = "/data/local/tmp/";
#else // !IS_ANDROID_PLATFORM()
    tempDir = fs::temp_directory_path();
#endif // !IS_ANDROID_PLATFORM()

    string processName = getFilename(getCurrentExecutablePath());
    const size_t maxLength = 40;
    if (processName.length() > maxLength) {
      processName.resize(maxLength);
    }
    processName += '-';
    string uniqueFolderName;
    do {
      uniqueFolderName = (tempDir / (processName + randomName(10))).string();
    } while (pathExists(uniqueFolderName) || makeDir(uniqueFolderName) != 0);
    uniqueFolderName += '/';
    return uniqueFolderName;
  }();
  return sUniqueFolderName;
}

/// If sourcePath is a link, returns the path to the linked target, or sourcePath otherwise.
/// Returns true if the source was really a link, but you can *always* use outLinkedPath.
bool getLinkedTarget(const string& sourcePath, string& outLinkedPath) {
  fs::path source(sourcePath);
  if (fs::symlink_status(source).type() == fs::symlink_file) {
    // Note: apply canonical() instead of readlink()
    // so that relative paths in symlinks are resolved properly
    outLinkedPath = fs::canonical(source).string();
    return true;
  }
  outLinkedPath = sourcePath;
  return false;
}
#endif

int hidePath(MAYBE_UNUSED const string& path, MAYBE_UNUSED bool hide) {
#if IS_WINDOWS_PLATFORM()
  std::wstring wpath = osUtf8ToWstring(path);
  DWORD attributes = GetFileAttributesW(wpath.c_str());
  if (attributes == INVALID_FILE_ATTRIBUTES) {
    return static_cast<int>(GetLastError());
  }
  if (hide) {
    attributes |= FILE_ATTRIBUTE_HIDDEN;
  } else {
    attributes &= ~FILE_ATTRIBUTE_HIDDEN;
  }
  if (!SetFileAttributesW(wpath.c_str(), attributes)) {
    return static_cast<int>(GetLastError());
  }
#endif
  return 0;
}

string getUniquePath(const string& baseName, size_t randomSuffixLength) {
  string uniqueName;
  uniqueName.reserve(baseName.size() + 1 + randomSuffixLength);
  uniqueName = baseName + '~';
  do {
    uniqueName.resize(baseName.size() + 1);
    uniqueName += randomName(randomSuffixLength);
  } while (os::pathExists(uniqueName));
  return uniqueName;
}

string sanitizeFileName(const string& filename) {
  // We're assuming utf8 characters and only sanitize the lower part of ASCII (< 128)
  // Above 128, we have a part of a utf8 char, which we won't handle today, sorry.
  // Values below 128 are guaranteed to not be part of a larger utf8 entity.
  // technically, \x7F is allowed, but it's not printable, so we'll sanitize it too...
#if IS_WINDOWS_PLATFORM()
  const char illegalChars[] = "/\\:\"*?<>|\x7F";
#else
  const char illegalChars[] = "/\x7F";
#endif
  string sanitizedName;
  sanitizedName.reserve(filename.size());
  for (unsigned char c : filename) {
    if (c < 32 || strchr(illegalChars, c) != nullptr) {
      sanitizedName += '%';
      // old type hex conversion, because I don't really like the C++ library
      const char* kHex = "0123456789ABCDEF";
      sanitizedName += kHex[(c >> 4) & 0xf];
      sanitizedName += kHex[c & 0xf];
    } else {
      sanitizedName.push_back(c);
    }
  }
#if IS_WINDOWS_PLATFORM()
  std::vector<const char*> illegalNames = {
      "CON",  "PRN",  "AUX",  "NUL",  "COM1", "COM2", "COM3", "COM4", "COM5", "COM6", "COM7",
      "COM8", "COM9", "LPT1", "LPT2", "LPT3", "LPT4", "LPT5", "LPT6", "LPT7", "LPT8", "LPT9"};
  for (const char* prefix : illegalNames) {
    size_t l = strlen(prefix);
    // test if the name starts with these bad names
    if (sanitizedName.size() >= l &&
        vrs::helpers::strncasecmp(prefix, sanitizedName.c_str(), l) == 0) {
      // it's only a problem if the name is exactly that?...
      if (l == sanitizedName.size()) {
        return '~' + sanitizedName;
      }
      // or that's the exact name with a file type extension...
      // "Nul.txt", and "nul." are not ok, nul.toto.txt is ok...
      if (sanitizedName.size() > l && sanitizedName[l] == '.' &&
          strrchr(sanitizedName.c_str(), '.') == sanitizedName.c_str() + l) {
        return '~' + sanitizedName;
      }
    }
  }
#endif
  // other bad news names
  if (sanitizedName == "." || sanitizedName == "..") {
    return '~' + sanitizedName;
  }
  return sanitizedName;
}

#if IS_VRS_OSS_CODE()

string pathJoin(const string& a, const string& b) {
  return (fs::path(a) / b).generic_string();
}

string pathJoin(const string& a, const string& b, const string& c) {
  return (fs::path(a) / b / c).generic_string();
}

int makeDir(const string& dir) {
  fs_error_code code;
  return fs::create_directory(fs::path(dir), code) ? 0 : code.value();
}

int makeDirectories(const string& dir) {
  fs_error_code code;
  return fs::create_directories(dir, code) ? 0 : code.value();
}

bool isDir(const string& path) {
  return fs::is_directory(fs::path(path));
}

bool isFile(const string& path) {
  fs_error_code ec;
  auto type = fs::status(fs::path(path), ec).type(); // This will traverse symlinks
  if (ec) { // Underlying OS API error - we cannot access it.
    return false;
  }
  return type == kRegularFileType; // Not supporting block-, character-, socket files
}

bool pathExists(const string& path) {
  fs_error_code ec;
  auto type = fs::status(fs::path(path), ec).type(); // This will traverse symlinks
  if (ec) {
    return false; // Underlying OS API error - we cannot access it.
  }
  return type != kNotFoundFileType;
}

int64_t getFileSize(const string& path) {
  fs_error_code ec;
  auto size = fs::file_size(fs::path(path), ec);
  if (ec) {
    return -1;
  }
  return size;
}

// make 'path/to/folder' and 'path/to/folder/' mean the same thing
static fs::path getCleanedPath(const string& path) {
  fs::path fspath;
  if (path.empty() || (path.back() != '/' && path.back() != '\\')) {
    fspath = path;
  } else {
    string p{path};
    do {
      p.pop_back();
    } while (!p.empty() && (p.back() == '/' || p.back() == '\\'));
    fspath = p;
  }
  return fspath;
}

string getFilename(const string& path) {
  return getCleanedPath(path).filename().generic_string();
}

string getParentFolder(const string& path) {
  return getCleanedPath(path).parent_path().generic_string();
}

const string& getHomeFolder() {
  static string sHomeFolder = [] {
#if IS_ANDROID_PLATFORM()
    return "/data/local/tmp/"; // there is no home folder where to write, use the temp folder...
#elif IS_WINDOWS_PLATFORM()
    const char* home = getenv("USERPROFILE");
    string homeFolder = (home != nullptr) ? home : fs::temp_directory_path().string();
    if (homeFolder.empty() || (homeFolder.back() != '/' && homeFolder.back() != '\\')) {
      homeFolder += '/';
    }
    return homeFolder;
#else
    const char* home = getenv("HOME");
    string homeFolder = (home != nullptr) ? home : fs::temp_directory_path().string();
    if (homeFolder.empty() || homeFolder.back() != '/') {
      homeFolder += '/';
    }
    return homeFolder;
#endif
  }();
  return sHomeFolder;
}

#if !IS_WINDOWS_PLATFORM() && !IS_APPLE_PLATFORM()
static size_t getLinuxSelfExePath(char* buf, size_t buflen) {
  ssize_t readlinkLen = ::readlink("/proc/self/exe", buf, buflen);
  size_t len = (readlinkLen == -1 || readlinkLen == sizeof(buf)) ? 0 : readlinkLen;
  buf[len] = '\0';
  return len;
}
#endif

string getCurrentExecutablePath() {
#if IS_WINDOWS_PLATFORM()
  const size_t kMaxPath = 1024;
  char exePath[kMaxPath];
  if (::GetModuleFileNameA(NULL, exePath, kMaxPath) <= 0) {
    exePath[0] = '\0';
  }
  return exePath;
#elif IS_APPLE_PLATFORM()
  char exePath[PATH_MAX];
  uint32_t len = PATH_MAX;
  if (_NSGetExecutablePath(exePath, &len) != 0) {
    exePath[0] = '\0'; // buffer too small (!)
  }
  return exePath;
#else // IS_LINUX_PLATFORM() || IS_ANDROID_PLATFORM()
  char exePath[PATH_MAX];
  size_t len = getLinuxSelfExePath(exePath, PATH_MAX);
  exePath[len] = '\0';
  return exePath;
#endif // IS_LINUX_PLATFORM() || IS_ANDROID_PLATFORM()
}
#endif

} // namespace vrs::os
