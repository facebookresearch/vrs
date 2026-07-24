// (c) Meta Platforms, Inc. and affiliates. Confidential and proprietary.

#include "cudaContextProvider.h"

#include "logging/Log.h"
#define DEFAULT_LOG_CHANNEL "XPRS"

namespace xprs {

CUDAContextScope::CUDAContextScope(const NvCodecContext& nv_codec_context)
    : _nv_codec_context(nv_codec_context) {
  CUresult cu_result =
      _nv_codec_context._cuda_functions->cuCtxPushCurrent(_nv_codec_context._cucontext);
  if (cu_result != CUDA_SUCCESS) {
    const char* error_name = nullptr;
    _nv_codec_context._cuda_functions->cuGetErrorName(cu_result, &error_name);
    std::string message = "FATAL. cuCtxPushCurrent failed with error: " + std::string(error_name);
    XR_LOGE("{}", message.c_str());
    throw std::runtime_error(message);
  }
}

CUDAContextScope ::~CUDAContextScope() {
  CUresult cu_result = _nv_codec_context._cuda_functions->cuCtxPopCurrent(nullptr);
  if (cu_result != CUDA_SUCCESS) {
    const char* error_name = nullptr;
    _nv_codec_context._cuda_functions->cuGetErrorName(cu_result, &error_name);
    XR_LOGE("cuCtxPopCurrent failed with error: {}", error_name);
  }
}

NvCodecContext NvCodecContextProvider::getNvCodecContext(const int device_num) {
  static NvCodecContext nv_codec_context;
  if (nv_codec_context._cucontext) {
    return nv_codec_context;
  }

  int ret = cuda_load_functions(&nv_codec_context._cuda_functions, nullptr);
  if (ret < 0) {
    const std::string message = "Loading CUDA functions failed";
    XR_LOGE("{}", message.c_str());
    throw std::runtime_error(message);
  }
  ret = cuvid_load_functions(&nv_codec_context._cuvid_functions, nullptr);
  if (ret < 0) {
    const std::string message = "Loading nvcuvid functions failed";
    XR_LOGE("{}", message.c_str());
    throw std::runtime_error(message);
  }

  CUresult cu_result = nv_codec_context._cuda_functions->cuInit(0);
  if (cu_result != CUDA_SUCCESS) {
    const std::string message = "cuInit failed with error code: " + std::to_string(cu_result);
    XR_LOGE("{}", message.c_str());
    throw std::runtime_error(message);
  }

  CUdevice cuda_device = 0;
  cu_result = nv_codec_context._cuda_functions->cuDeviceGet(&cuda_device, device_num);
  if (cu_result != CUDA_SUCCESS) {
    const std::string message = "cuDeviceGet failed with error code: " + std::to_string(cu_result);
    XR_LOGE("{}", message.c_str());
    throw std::runtime_error(message);
  }

  cu_result = nv_codec_context._cuda_functions->cuDeviceGetName(
      nv_codec_context._device_name, sizeof(nv_codec_context._device_name), cuda_device);
  if (cu_result != CUDA_SUCCESS) {
    const std::string message =
        "cuDeviceGetName failed with error code: " + std::to_string(cu_result);
    XR_LOGE("{}", message.c_str());
    throw std::runtime_error(message);
  }

  cu_result = nv_codec_context._cuda_functions->cuCtxCreate(
      &nv_codec_context._cucontext, CU_CTX_SCHED_BLOCKING_SYNC, cuda_device);
  if (cu_result != CUDA_SUCCESS) {
    const std::string message = "cuCtxCreate failed with error code: " + std::to_string(cu_result);
    XR_LOGE("{}", message.c_str());
    throw std::runtime_error(message);
  }

  // Restore the previous context
  cu_result = nv_codec_context._cuda_functions->cuCtxPopCurrent(nullptr);
  if (cu_result != CUDA_SUCCESS) {
    const std::string message =
        "cuCtxPopCurrent failed with error code: " + std::to_string(cu_result);
    XR_LOGE("{}", message.c_str());
    throw std::runtime_error(message);
  }

  return nv_codec_context;
}

} // namespace xprs
