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

#include <map>
#include <memory>
#include <mutex>
#include <string>

#include <vrs/ForwardDefinitions.h>

namespace vrs {

using std::map;
using std::mutex;
using std::string;
using std::unique_ptr;

/// \brief A factory system for FileHandlers, allowing the runtime registration & usage of custom
/// FileHandler implementations
class FileHandlerFactory {
 public:
  static FileHandlerFactory& getInstance();

  /// Delegators operating on FileSpec.fileHandlerName
  void registerFileDelegator(const string& name, unique_ptr<FileDelegator>&& delegator);
  void unregisterFileDelegator(const string& name);

  /// Delegators operating on particular key-value pairs of FileSpec.extras so this type of URI can
  /// be customized: a_scheme:/my/path?my_unique_name=my_unique_value
  void registerExtraDelegator(
      const string& extraName,
      const string& extraValue,
      unique_ptr<FileDelegator>&& delegator);
  void unregisterExtraDelegator(const string& extraName, const string& extraValue);
  FileDelegator* getExtraDelegator(const FileSpec& fileSpec);
  FileDelegator* getExtraDelegator(const string& extraName, const string& extraValue);

  /// Delegators operating on FileSpec.fileHandlerName
  void registerFileHandler(unique_ptr<FileHandler>&& fileHandler);
  void unregisterFileHandler(const string& fileHandlerName);
  unique_ptr<FileHandler> getFileHandler(const string& name);
  FileDelegator* getFileDelegator(const string& name);

  /// Use different strategies to determine which FileHandler should be used to open the file path.
  /// @param path: a path or other form of identification for a file.
  /// @param outNewDelegate: If provided, might be a fallback FileHandler to use.
  /// On success, will be set to the right FileHandler to handle the provided path.
  /// On failure, maybe unset.
  /// @return A status code, 0 meaning success.
  ///
  /// `path` identification methods:
  /// 1) File paths; examples `/posix/path/to/file`, or `C:\\Path\\To\\File`
  /// 2) URI paths, probably mapped to custom FileHandler implementations registered externally.
  /// 3) JSON "paths" containing:
  ///    storage - name of a registered file handler to use
  ///    chunks - storage specific references (path, url, ...) to successive chunks of data
  ///    ```{
  ///      "storage": "required-name-of-file-handler",
  ///      "chunks": [ "chunk_path_1", "chunk2_path_2", ... ],
  ///    }```
  ///    JSON paths are converted into FileSpec objects by FileSpec::fromJson()
  /// 4) If all the above methods fail, fall back to opening path as file path.
  ///
  /// Method is virtual to enable mock FileHandlerFactories to be created for unit testing.
  virtual int delegateOpen(const string& path, unique_ptr<FileHandler>& outNewDelegate);
  virtual int delegateOpen(const FileSpec& fileSpec, unique_ptr<FileHandler>& outNewDelegate);

  /// Parsing URIs can be customized by FileHandler and FileDelegators.
  /// Note that extra delegators only get the parseUri() callback after parsing is complete,
  /// but they can still completely change the FileSpec.
  virtual int parseUri(FileSpec& inOutFileSpec, size_t colonIndex);

 protected:
  FileHandlerFactory();
  virtual ~FileHandlerFactory() = default;

 private:
  mutex mutex_;
  map<string, unique_ptr<FileDelegator>> fileDelegatorMap_;
  map<string, unique_ptr<FileHandler>> fileHandlerMap_;
  map<string, map<string, unique_ptr<FileDelegator>>> extraDelegatorMap_;
};

} // namespace vrs
