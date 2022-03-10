// Copyright (c) Meta Platforms, Inc. and affiliates.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#pragma once

#include <map>
#include <memory>
#include <mutex>
#include <string>

#include "ForwardDefinitions.h"

namespace vrs {

/// \brief A factory system for FileHandlers, allowing the runtime registration & usage of custom
/// FileHandler implementations
class FileHandlerFactory {
 public:
  static FileHandlerFactory& getInstance();

  void registerFileHandler(std::unique_ptr<FileHandler>&& fileHandler);
  void unregisterFileHandler(const std::string& fileHandlerName);
  std::unique_ptr<FileHandler> getFileHandler(const std::string& name);

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
  virtual int delegateOpen(const std::string& path, std::unique_ptr<FileHandler>& outNewDelegate);
  virtual int delegateOpen(const FileSpec& fileSpec, std::unique_ptr<FileHandler>& outNewDelegate);

 protected:
  FileHandlerFactory();
  virtual ~FileHandlerFactory() = default;

 private:
  std::mutex mutex_;
  std::map<std::string, std::unique_ptr<FileHandler>> fileHandlerMap_ = {};
};

} // namespace vrs
