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

// DEFAULT_LOG_CHANNEL must be defined before "logging/Log.h": the OSS build's
// XR_LOGE/XR_LOGW macros are gated on it (the internal logging header defines
// them unconditionally, which is why this ordering bug is invisible to the Buck
// build but breaks the OSS CMake build).
#define DEFAULT_LOG_CHANNEL "XPRS"

#include "cudaContextProvider.h"

#include <atomic>
#include <mutex>
#include <string>
#include <type_traits>

#include "logging/Log.h"

namespace xprs {

namespace {

// Tri-state init flag for getNvCodecContext(). We don't retry on failure
// because (a) CUDA driver / kernel module availability is fixed for the
// lifetime of the process — if cuInit() fails once it will keep failing;
// (b) every retry leaks a pair of dlopen handles (cuda_load_functions +
// cuvid_load_functions) for callers that catch the throw and call us again
// on the next VRS file open. In a long-running non-GPU process this can
// leak hundreds of handles before exit.
enum class InitState : int { Uninit = 0, Ready = 1, Failed = 2 };

std::atomic<InitState> nv_codec_init_state{InitState::Uninit};
std::mutex nv_codec_init_mutex;
NvCodecContext cached_nv_codec_context;
// Write-once: set only inside fail() under nv_codec_init_mutex, read only after
// observing kFailed via acquire-load. Do not mutate after publish.
std::string nv_codec_init_error;

// Test-only: counts how many times a thread has entered the heavy init body
// below (past the under-lock double-check). Exposed via
// detail::nvCodecInitCallCountForTesting(). Only mutated under
// nv_codec_init_mutex, so relaxed ordering suffices.
std::atomic<int> nv_codec_init_call_count{0};

// Free any partially-initialized handles after a failed init attempt.
// Safe to call with null pointers — the dlopen free helpers no-op on null.
void freeNvCodecHandles(NvCodecContext& ctx) {
  if (ctx._cucontext != nullptr && ctx._cuda_functions != nullptr) {
    ctx._cuda_functions->cuCtxDestroy(ctx._cucontext);
    ctx._cucontext = nullptr;
  }
  if (ctx._cuda_functions != nullptr) {
    cuda_free_functions(&ctx._cuda_functions);
  }
  if (ctx._cuvid_functions != nullptr) {
    cuvid_free_functions(&ctx._cuvid_functions);
  }
}

} // namespace

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
  // Fast path under acquire ordering: most calls hit this once init has
  // either succeeded or permanently failed for the process.
  const InitState state = nv_codec_init_state.load(std::memory_order_acquire);
  if (state == InitState::Ready) {
    return cached_nv_codec_context;
  }
  if (state == InitState::Failed) {
    // Safe without mutex: the acquire-load on nv_codec_init_state synchronizes-with
    // the release-store in fail(), which happens-after the write to
    // nv_codec_init_error.
    throw std::runtime_error(nv_codec_init_error);
  }

  // Slow path: serialize the first init across threads. Without this, two
  // threads racing past the fast-path check above would both try to
  // cuda_load_functions() / cuCtxCreate(), leaking handles and possibly
  // installing inconsistent state into cached_nv_codec_context.
  std::lock_guard<std::mutex> lock(nv_codec_init_mutex);

  // Re-check under the lock — another thread may have completed init while
  // we were waiting.
  const InitState recheck = nv_codec_init_state.load(std::memory_order_acquire);
  if (recheck == InitState::Ready) {
    return cached_nv_codec_context;
  }
  if (recheck == InitState::Failed) {
    throw std::runtime_error(nv_codec_init_error);
  }

  // Exactly one thread per init generation reaches here (we hold the lock and
  // the state is still Uninit). The test hook asserts this count is 1 — see
  // detail::nvCodecInitCallCountForTesting().
  nv_codec_init_call_count.fetch_add(1, std::memory_order_relaxed);

  // Build into a local context. Only copy into cached_nv_codec_context after every
  // step has succeeded — partial state must never be visible to the fast path.
  // (Previously the code wrote directly into the static, so a failure between
  // cuCtxCreate and cuCtxPopCurrent left _cucontext set to a half-built ctx.)
  NvCodecContext local_ctx;
  std::string err;

  auto fail = [&](std::string message) {
    err = std::move(message);
    XR_LOGW("{}", err);
    freeNvCodecHandles(local_ctx);
    nv_codec_init_error = err;
    nv_codec_init_state.store(InitState::Failed, std::memory_order_release);
  };

  // Note: all failures in this function below log at WARN, not ERROR. The only caller
  // (xprsDecApi.cpp::enumDecoders) catches the throw and falls back to SW
  // decoders. On a non-GPU machine this fires every VRS file open, so logging
  // ERROR-level would spam users who never asked for GPU decoding.
  int ret = cuda_load_functions(&local_ctx._cuda_functions, nullptr);
  if (ret < 0) {
    fail("Loading CUDA functions failed");
    throw std::runtime_error(nv_codec_init_error);
  }
  ret = cuvid_load_functions(&local_ctx._cuvid_functions, nullptr);
  if (ret < 0) {
    fail("Loading nvcuvid functions failed");
    throw std::runtime_error(nv_codec_init_error);
  }

  CUresult cu_result = local_ctx._cuda_functions->cuInit(0);
  if (cu_result != CUDA_SUCCESS) {
    fail("cuInit failed with error code: " + std::to_string(cu_result));
    throw std::runtime_error(nv_codec_init_error);
  }

  CUdevice cuda_device = 0;
  cu_result = local_ctx._cuda_functions->cuDeviceGet(&cuda_device, device_num);
  if (cu_result != CUDA_SUCCESS) {
    fail("cuDeviceGet failed with error code: " + std::to_string(cu_result));
    throw std::runtime_error(nv_codec_init_error);
  }

  cu_result = local_ctx._cuda_functions->cuDeviceGetName(
      local_ctx._device_name, sizeof(local_ctx._device_name), cuda_device);
  if (cu_result != CUDA_SUCCESS) {
    fail("cuDeviceGetName failed with error code: " + std::to_string(cu_result));
    throw std::runtime_error(nv_codec_init_error);
  }

  cu_result = local_ctx._cuda_functions->cuCtxCreate(
      &local_ctx._cucontext, CU_CTX_SCHED_BLOCKING_SYNC, cuda_device);
  if (cu_result != CUDA_SUCCESS) {
    fail("cuCtxCreate failed with error code: " + std::to_string(cu_result));
    throw std::runtime_error(nv_codec_init_error);
  }

  cu_result = local_ctx._cuda_functions->cuCtxPopCurrent(nullptr);
  if (cu_result != CUDA_SUCCESS) {
    // Context is still current on this thread — null out _cucontext so
    // freeNvCodecHandles skips cuCtxDestroy (destroying while current is UB).
    // The context leaks but this path is astronomically rare (driver bug).
    local_ctx._cucontext = nullptr;
    fail("cuCtxPopCurrent failed with error code: " + std::to_string(cu_result));
    throw std::runtime_error(nv_codec_init_error);
  }

  // SUCCESS: publish the fully-built context. The release-store on nv_codec_init_state
  // synchronizes with the acquire-load on the fast path so other threads see
  // the writes to cached_nv_codec_context that happened before the store here.
  cached_nv_codec_context = local_ctx;
  static_assert(
      std::is_trivially_destructible_v<NvCodecContext>,
      "NvCodecContext must be trivially destructible — the manual null-out "
      "below assumes no destructor runs on local_ctx going out of scope.");
  local_ctx._cuda_functions = nullptr;
  local_ctx._cuvid_functions = nullptr;
  local_ctx._cucontext = nullptr;
  nv_codec_init_state.store(InitState::Ready, std::memory_order_release);
  return cached_nv_codec_context;
}

namespace detail {

int nvCodecInitCallCountForTesting() {
  return nv_codec_init_call_count.load(std::memory_order_relaxed);
}

void resetNvCodecContextForTesting() {
  std::lock_guard<std::mutex> lock(nv_codec_init_mutex);
  freeNvCodecHandles(cached_nv_codec_context);
  nv_codec_init_error.clear();
  nv_codec_init_call_count.store(0, std::memory_order_relaxed);
  nv_codec_init_state.store(InitState::Uninit, std::memory_order_release);
}

} // namespace detail

} // namespace xprs
