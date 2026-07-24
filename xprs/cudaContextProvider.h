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
