// Facebook Technologies, LLC Proprietary and Confidential.

#include <vrs/os/Utils.h>

#include <cstdio>

#include <algorithm>

#include <logging/Checks.h>
#include <system_utils/os/Utils.h>

#include <vrs/os/Platform.h>
#include <vrs/utils/Strings.h>

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
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <cerrno>
#include <cstring>
#endif // !IS_WINDOWS_PLATFORM()

#if IS_XROS_PLATFORM()
#include <logging/Checks.h>
#include <xros/portability/StdFilesystem.h>
namespace fs = xros_std_filesystem;
#else // !IS_XROS_PLATFORM()
#include <boost/filesystem.hpp>
namespace fs = boost::filesystem;
#endif // !IS_XROS_PLATFORM()

using namespace arvr;
using std::string;

namespace vrs::os {

#if IS_WINDOWS_PLATFORM()
std::wstring osUtf8ToWstring(const string& utf8String) {
  return std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>>().from_bytes(utf8String);
}
#endif // IS_WINDOWS_PLATFORM()

FILE* fileOpen(const string& filename, const char* modes) {
#if IS_WINDOWS_PLATFORM()
  // On Windows, the C APIs create file handles that are inheritable by default. This is not a
  // desirable behavior, since subprocesses could inherit our handles to VRS file and create file
  // access contention bugs. The Windows-specific "N" mode makes the file handle non-inheritable.
  return ::_wfopen(osUtf8ToWstring(filename).c_str(), osUtf8ToWstring(modes).append(L"N").c_str());
#else // !IS_WINDOWS_PLATFORM()
  return ::fopen(filename.c_str(), modes);
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

int fileSetSize(std::FILE* file, int64_t size) {
#if IS_WINDOWS_PLATFORM()
  return ::_chsize_s(_fileno(file), size);
#elif IS_XROS_PLATFORM()
  return xros_std_filesystem::setFileSize(file, size);
#elif (defined(__ANDROID_API__) && __ANDROID_API__ >= 21)
  return ::ftruncate64(fileno(file), static_cast<off64_t>(size));
#else
  return ::ftruncate(fileno(file), size);
#endif
}

int getLastFileError() {
  return errno;
}

int remove(const string& filename) {
#if IS_WINDOWS_PLATFORM()
  return ::_wremove(osUtf8ToWstring(filename).c_str());
#else // !IS_WINDOWS_PLATFORM()
  return ::remove(filename.c_str());
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
static string random_name(int length) {
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

const string& getTempFolder() {
  static string sUniqueFolderName = [] {

#if IS_XROS_PLATFORM()
    fs::path tempDir = fs::temp_directory_path();

    fs::path uniqueDir;
    do {
      uniqueDir = tempDir / random_name(16);
    } while (fs::create_directories(uniqueDir) == false);
    return uniqueDir.string();

#else // !IS_XROS_PLATFORM()
    fs::path tempDir;
#if IS_ANDROID_PLATFORM()
    tempDir = "/data/local/tmp/";
#else // !IS_ANDROID_PLATFORM()
    tempDir = fs::temp_directory_path();
#endif // !IS_ANDROID_PLATFORM()

    string processName =
        system_utils::os::getFilename(system_utils::os::getCurrentExecutablePath());
    const size_t maxLength = 40;
    if (processName.length() > maxLength) {
      processName.resize(maxLength);
    }
    processName += '-';
    string uniqueFolderName;
    do {
      uniqueFolderName = (tempDir / (processName + random_name(10))).string();
    } while (system_utils::os::pathExists(uniqueFolderName) ||
             system_utils::os::makeDir(uniqueFolderName) != 0);
    uniqueFolderName += '/';
    return uniqueFolderName;
#endif // !IS_XROS_PLATFORM()
  }();
  return sUniqueFolderName;
}

/// If sourcePath is a link, returns the path to the linked target, or sourcePath otherwise.
/// Returns true if the source was really a link, but you can *always* use outLinkedPath.
bool getLinkedTarget(const string& sourcePath, string& outLinkedPath) {
#if !IS_WINDOWS_PLATFORM() // not supported on Windows
  struct STAT64 st;
  if (LSTAT64(sourcePath.c_str(), &st) == 0 && S_ISLNK(st.st_mode)) {
    size_t pathSize = static_cast<size_t>(st.st_size);
    outLinkedPath.resize(pathSize);
    if (readlink(sourcePath.c_str(), &outLinkedPath.front(), pathSize) == st.st_size) {
      return true;
    }
  }
#endif // !IS_WINDOWS_PLATFORM()
  outLinkedPath = sourcePath;
  return false;
}

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
    uniqueName += random_name(randomSuffixLength);
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
        vrs::utils::str::strncasecmp(prefix, sanitizedName.c_str(), l) == 0) {
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

} // namespace vrs::os
