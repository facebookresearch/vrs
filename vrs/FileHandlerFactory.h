// (c) Facebook, Inc. and its affiliates. Confidential and proprietary.

#pragma once

#include <map>
#include <memory>
#include <string>

#include "ForwardDefinitions.h"

namespace vrs {

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
  /// 1) File path; examples `/posix/path/to/file`, or `C:\\Path\\To\\File`
  /// 2) Gaia URI; examples `gaia:12346789`
  /// 3) JSON object containing:
  ///    storage - name of a registered file handler to use
  ///    chunks - storage specific references (path, url, ...) to successive chunks of data
  ///    ```{
  ///      "storage": "required-name-of-file-handler",
  ///      "chunks": [ "chunk_path_1", "chunk2_path_2", ... ],
  ///    }```
  /// 4) If all the above methods fail, fall back to opening path as file path.
  ///
  /// Method is virtual to enable mock FileHandlerFactories to be created for unit testing.
  virtual int delegateOpen(const std::string& path, std::unique_ptr<FileHandler>& outNewDelegate);
  virtual int delegateOpen(const FileSpec& fileSpec, std::unique_ptr<FileHandler>& outNewDelegate);

 protected:
  FileHandlerFactory();
  virtual ~FileHandlerFactory() = default;

 private:
  std::map<std::string, std::unique_ptr<FileHandler>> fileHandlerMap_ = {};
};

} // namespace vrs
