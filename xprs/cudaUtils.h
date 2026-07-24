// (c) Meta Platforms, Inc. and affiliates. Confidential and proprietary.

#pragma once

#include "xprsUtils.h"

namespace xprs {

#define THROW_IF_ERROR true
#define DO_NOT_THROW false

#define CUDA_API_CALL(nvidia_api, cuda_functions, throw_if_error)                                  \
  do {                                                                                             \
    CUresult cu_result = nvidia_api;                                                               \
    if (cu_result != CUDA_SUCCESS) {                                                               \
      const char* error_name = NULL;                                                               \
      (cuda_functions)->cuGetErrorName(cu_result, &error_name);                                    \
      std::string message =                                                                        \
          std::string("ERROR. ") + #nvidia_api + " failed with error: " + std::string(error_name); \
      XR_LOGE("{}", message.c_str());                                                              \
      if (throw_if_error) {                                                                        \
        throw std::runtime_error(message);                                                         \
      }                                                                                            \
    }                                                                                              \
  } while (0)

} // end of namespace xprs
