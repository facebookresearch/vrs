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

#include <memory>
#include <string>

namespace vrs {

using std::string;

/// \brief Utility class to manage various forms of file caching. Disabled by default.
///
/// There is a main file cache, which needs to be created for file caching to be enabled.
class FileCache {
 public:
  /// Make the file cache. You need to create it to enable caching features.
  /// Note that there is only one of these file cache in any running app, but there could be
  /// multiple ones in the file system.
  /// @param app: name for the app, to have its own space, maybe shared with other apps of the same
  /// team. VRS apps like VRStool & VRSplayer will use kDefaultVrsAppsCache.
  /// @param parentFolder: an optional cache folder location.
  /// If empty, the cache will be created in the home folder.
  /// @return A status code, 0 meaning success.
  static int makeFileCache(const string& app, const string& parentFolder = {});

  /// To disable the file cache. Safe to call even if not enabled.
  static void disableFileCache();

  /// Get file cache, if one has been created.
  /// @return The existing file cache, or nullptr if none was created.
  static FileCache* getFileCache();

  /// Look-up a file in the cache.
  /// @param filename: a filename for the object.
  /// @param outFilePath: a path to the object in the cache.
  /// @return 0 if the file exists, and outFilePath has been set.
  /// Returns FILE_NOT_FOUND if the file doesn't exist, and outFilePath has been set, for you to
  /// add the object in the cache at that location.
  /// Returns another error code and outFilePath isn't set, if some error occurred, and the object
  /// doesn't exist and can't be created, maybe because some files or folders are in the way.
  int getFile(const string& filename, string& outFilePath);

  /// Look-up a file in the cache, specifying a domain.
  /// Useful, if you want to cache a bunch of derived objects from a particular object, placing
  /// all those cached objects in a folder named after the main object.
  /// @param domain: the primary domain. Might be an identifier for a specific VRS file...
  /// @param filename: a filename for the object.
  /// @param outFilePath: a path to the object in the cache.
  /// @return 0 if the file exists, and outFilePath has been set.
  /// Returns FILE_NOT_FOUND if the file doesn't exist, and outFilePath has been set, for you to
  /// add the object in the cache at that location.
  /// Returns another error code and outFilePath isn't set, if some error occurred, and the object
  /// doesn't exist and can't be created, maybe because some files or folders are in the way.
  int getFile(const string& domain, const string& filename, string& outFilePath);

 protected:
  explicit FileCache(string mainFolder) : mainFolder_{std::move(mainFolder)} {}

 private:
  string mainFolder_;

  static std::unique_ptr<FileCache> sFileCache;
};

} // namespace vrs
