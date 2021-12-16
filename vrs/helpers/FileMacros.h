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

// Macro to write a known number of bytes to a file, and return the error if necessary
#define WRITE_OR_LOG_AND_RETURN(file__, data__, byteCount__)     \
  do {                                                           \
    if (byteCount__ > 0) {                                       \
      int error__ = file__.write(data__, byteCount__);           \
      if (error__ != 0) {                                        \
        XR_LOGE(                                                 \
            "File write error, {} instead of {}, Error: {}, {}", \
            file__.getLastRWSize(),                              \
            byteCount__,                                         \
            error__,                                             \
            errorCodeToMessage(error__));                        \
        return error__;                                          \
      }                                                          \
    }                                                            \
  } while (false)

// Helper macro for some file operations
#define IF_ERROR_LOG_AND_RETURN(operation__)     \
  do {                                           \
    int operationError__ = operation__;          \
    if (operationError__ != 0) {                 \
      XR_LOGE(                                   \
          "{} failed: {}, {}",                   \
          #operation__,                          \
          operationError__,                      \
          errorCodeToMessage(operationError__)); \
      return operationError__;                   \
    }                                            \
  } while (false)

#define IF_ERROR_RETURN(operation__)    \
  do {                                  \
    int operationError__ = operation__; \
    if (operationError__ != 0) {        \
      return operationError__;          \
    }                                   \
  } while (false)

#define IF_ERROR_LOG(operation__)                \
  do {                                           \
    int operationError__ = operation__;          \
    if (operationError__ != 0) {                 \
      XR_LOGE(                                   \
          "{} failed: {}, {}",                   \
          #operation__,                          \
          operationError__,                      \
          errorCodeToMessage(operationError__)); \
    }                                            \
  } while (false)
