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

// Macro to write a known number of bytes to a file, and return the error if necessary
#define WRITE_OR_LOG_AND_RETURN(file_, data_, byteCount_)        \
  do {                                                           \
    if (byteCount_ > 0) {                                        \
      int error_ = file_.write(data_, byteCount_);               \
      if (error_ != 0) {                                         \
        XR_LOGE(                                                 \
            "File write error, {} instead of {}, Error: {}, {}", \
            file_.getLastRWSize(),                               \
            byteCount_,                                          \
            error_,                                              \
            errorCodeToMessage(error_));                         \
        return error_;                                           \
      }                                                          \
    }                                                            \
  } while (false)

// Helper macro for some file operations
#define IF_ERROR_LOG_AND_RETURN(operation_)                                                        \
  do {                                                                                             \
    int operationError_ = operation_;                                                              \
    if (operationError_ != 0) {                                                                    \
      XR_LOGE(                                                                                     \
          "{} failed: {}, {}", #operation_, operationError_, errorCodeToMessage(operationError_)); \
      return operationError_;                                                                      \
    }                                                                                              \
  } while (false)

#define IF_ERROR_RETURN(operation_)   \
  do {                                \
    int operationError_ = operation_; \
    if (operationError_ != 0) {       \
      return operationError_;         \
    }                                 \
  } while (false)

#define IF_ERROR_LOG(operation_)                                                                   \
  do {                                                                                             \
    int operationError_ = operation_;                                                              \
    if (operationError_ != 0) {                                                                    \
      XR_LOGE(                                                                                     \
          "{} failed: {}, {}", #operation_, operationError_, errorCodeToMessage(operationError_)); \
    }                                                                                              \
  } while (false)
