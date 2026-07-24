// (c) Meta Platforms, Inc. and affiliates. Confidential and proprietary.

#pragma once

#include <ffnvcodec/dynlink_cuda.h>
#include <ffnvcodec/dynlink_loader.h> // CudaFunctions

#include "xprsUtils.h"

namespace xprs {

class NvCodecContext {
 public:
  CUcontext _cucontext = nullptr;
  char _device_name[128] = {0};
  CudaFunctions* _cuda_functions = nullptr;
  CuvidFunctions* _cuvid_functions = nullptr;
};

struct CUDAContextScope {
  explicit CUDAContextScope(const NvCodecContext& nv_codec_context);
  ~CUDAContextScope();

 private:
  NvCodecContext _nv_codec_context;
};

class NvCodecContextProvider {
 public:
  static NvCodecContext getNvCodecContext(int device_num = 0);
};

} // namespace xprs
