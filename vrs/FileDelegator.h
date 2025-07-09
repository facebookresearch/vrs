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

#include <vrs/os/CompilerAttributes.h>

#include "FileSpec.h"

namespace vrs {

using std::unique_ptr;

class FileHandler;

/// \brief Class to abstract the delegate file open operation for VRS file.
class FileDelegator {
 public:
  FileDelegator() = default;

  virtual ~FileDelegator() = default;

  /// Delegate the file open operation to the correct FileHandler.
  /// @param fileSpec: file specification.
  /// @param outNewDelegate: If provided, might be a fallback FileHandler to use.
  /// On exit, may be set to a different FileHandler than the provided one, if the provided
  /// FileHandler was not set or not the correct one to handle the provided file specification,
  /// or cleared if no appropriate FileHandler could be found.
  /// @return A status code, 0 meaning success.
  virtual int delegateOpen(const FileSpec& fileSpec, unique_ptr<FileHandler>& outNewDelegate) = 0;

  /// When converting a URI to a FileSpec, some custom parsing maybe required.
  /// @param inOutFileSpec: on input, both the fileHandlerName & URI fields are set.
  /// All the other fields of the FileSpec object are cleared, and URI holds the full original URI.
  /// @param colonIndex: index of the ':' character of the URI.
  /// @return A status code, 0 on success, which doesn't necessarily mean that the file/object
  /// exists or can be opened, merely, that parsing the URI did not fail.
  /// On success, any of the fields may have been set or changed, including fileHandlerName and URI.
  virtual int parseUri(FileSpec& inOutFileSpec, MAYBE_UNUSED size_t colonIndex) const {
    return inOutFileSpec.parseUri();
  }
};

} // namespace vrs
