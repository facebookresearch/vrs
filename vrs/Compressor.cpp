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

#include <vrs/Compressor.h>

#include <cstring>

#include <iterator>
#include <limits>
#include <map>
#include <mutex>

#include <lz4frame.h>
#include <zstd.h>
#include <zstd_errors.h>

#define DEFAULT_LOG_CHANNEL "VRSCompressor"
#include <logging/Log.h>

#include <vrs/ErrorCode.h>
#include <vrs/FileHandler.h>
#include <vrs/Record.h>
#include <vrs/helpers/EnumStringConverter.h>
#include <vrs/helpers/FileMacros.h>

namespace {

using namespace std;
using namespace vrs;

const map<CompressionPreset, int> sZstdPresets = {
    {CompressionPreset::ZstdFaster, -1},
    {CompressionPreset::ZstdFast, 1},
    {CompressionPreset::ZstdLight, 3},
    {CompressionPreset::ZstdMedium, 7},
    {CompressionPreset::ZstdHeavy, 12},
    {CompressionPreset::ZstdHigh, 15},
    {CompressionPreset::ZstdTight, 18},
    {CompressionPreset::ZstdMax, 20}};

static_assert(
    static_cast<uint32_t>(CompressionPreset::LastZstdPreset) -
            static_cast<uint32_t>(CompressionPreset::FirstZstdPreset) + 1 ==
        8, // Number of zstd presets defined in sZstdPreset
    "Missing sZstdPreset constant definition");

const map<CompressionPreset, const char*> sPresetNames = {
    {CompressionPreset::None, "none"},
    {CompressionPreset::Undefined, "undefined"},
    {CompressionPreset::Lz4Fast, "lz4-fast"},
    {CompressionPreset::Lz4Tight, "lz4-tight"},
    {CompressionPreset::ZstdFaster, "zstd-faster"},
    {CompressionPreset::ZstdFast, "zstd-fast"},
    {CompressionPreset::ZstdLight, "zstd-light"},
    {CompressionPreset::ZstdMedium, "zstd-medium"},
    {CompressionPreset::ZstdHeavy, "zstd-heavy"},
    {CompressionPreset::ZstdHigh, "zstd-high"},
    {CompressionPreset::ZstdTight, "zstd-tight"},
    {CompressionPreset::ZstdMax, "zstd-max"}};

string_view sCompressionPresetNames[] = {
    "none",
    "fast",
    "tight",
    "zfaster",
    "zfast",
    "zlight",
    "zmedium",
    "zheavy",
    "zhigh",
    "ztight",
    "zmax"};
ENUM_STRING_CONVERTER3(
    CompressionPreset,
    sCompressionPresetNames,
    CompressionPreset::Undefined,
    CompressionPreset::Undefined,
    true);

string_view sCompressionTypeNames[] = {
    "none",
    "lz4",
    "zstd",
};
ENUM_STRING_CONVERTER3(
    CompressionType,
    sCompressionTypeNames,
    CompressionType::None,
    CompressionType::None,
    true);

// LZ4 encodes errors as size_t(-errorCode); unsigned negation reverses it safely.
size_t lz4ErrorCodeFromResult(size_t result) {
  return size_t{0} - result;
}

template <typename Size>
bool exceedsUint32Max(Size size) {
  if constexpr (sizeof(Size) > sizeof(uint32_t)) {
    return size > std::numeric_limits<uint32_t>::max();
  }
  return false;
}

int zstdPresetToCompressionLevel(CompressionPreset preset) {
  auto iter = sZstdPresets.find(preset);
  if (iter == sZstdPresets.end()) {
    return ZSTD_CLEVEL_DEFAULT;
  }
  return iter->second;
}

} // namespace

namespace vrs {

const size_t Compressor::kMinByteCountForCompression = 250;

DEFINE_ENUM_CONVERTERS(CompressionPreset);
DEFINE_ENUM_CONVERTERS(CompressionType)

#define IF_LZ4_COMP_ERROR_LOG_AND_RETURN(operation__)                                    \
  do {                                                                                   \
    lzresult = operation__;                                                              \
    if (LZ4F_isError(lzresult)) {                                                        \
      XR_LOGE("{} failed: {}, {}", #operation__, lzresult, LZ4F_getErrorName(lzresult)); \
      compressionType_ = CompressionType::None;                                          \
      return domainErrorCode(                                                            \
          ErrorDomain::Lz4CompressionErrorDomain,                                        \
          lz4ErrorCodeFromResult(lzresult),                                              \
          LZ4F_getErrorName(lzresult));                                                  \
    }                                                                                    \
  } while (false)

#define IF_ZSTD_COMP_ERROR_LOG_AND_RETURN(operation__)                                          \
  do {                                                                                          \
    zresult = operation__;                                                                      \
    if (ZSTD_isError(zresult)) {                                                                \
      const ZSTD_ErrorCode errorCode = ZSTD_getErrorCode(zresult);                              \
      const char* errorMessage = ZSTD_getErrorString(errorCode);                                \
      XR_LOGE("{} failed: {}, {}", #operation__, zresult, errorMessage);                        \
      ZSTD_CCtx_reset(zstdContext_, ZSTD_reset_session_and_parameters);                         \
      compressionType_ = CompressionType::None;                                                 \
      return domainErrorCode(ErrorDomain::ZstdCompressionErrorDomain, errorCode, errorMessage); \
    }                                                                                           \
  } while (false)

class Compressor::CompressorImpl {
 public:
  CompressorImpl() {
    compressionType_ = CompressionType::None;
  }
  ~CompressorImpl() {
    if (zstdContext_ != nullptr) {
      ZSTD_freeCCtx(zstdContext_);
    }
    if (lz4Context_ != nullptr) {
      LZ4F_freeCompressionContext(lz4Context_);
    }
  }
  int lz4Compress(
      helpers::IOVector<uint8_t>& buffer,
      const void* data,
      size_t dataSize,
      CompressionPreset preset,
      uint32_t& outCompressedSize,
      size_t headerSpace) {
    outCompressedSize = 0;
    const auto* prefs = getLz4Preferences(preset);
    size_t maxCompressedSize = LZ4F_compressFrameBound(dataSize, prefs);
    // increase our internal buffer size if necessary
    if (buffer.size() < headerSpace + maxCompressedSize) {
      buffer.resizeDiscardingWithoutInitialization(headerSpace + maxCompressedSize);
    }
    size_t lzresult = 0;
    IF_LZ4_COMP_ERROR_LOG_AND_RETURN(
        LZ4F_compressFrame(buffer.data() + headerSpace, maxCompressedSize, data, dataSize, prefs));
    // if the compressed data isn't smaller, don't use it!
    if (lzresult < dataSize) {
      compressionType_ = CompressionType::Lz4;
      outCompressedSize = static_cast<uint32_t>(lzresult);
    } else {
      compressionType_ = CompressionType::None;
    }
    return SUCCESS;
  }
  int zstdCompress(
      helpers::IOVector<uint8_t>& buffer,
      const void* data,
      size_t dataSize,
      CompressionPreset preset,
      uint32_t& outCompressedSize,
      size_t headerSpace) {
    outCompressedSize = 0;
    size_t maxCompressedSize = ZSTD_compressBound(dataSize);
    // increase our internal buffer size if necessary
    if (buffer.size() < headerSpace + maxCompressedSize) {
      buffer.resizeDiscardingWithoutInitialization(headerSpace + maxCompressedSize);
    }
    if (zstdContext_ == nullptr) {
      zstdContext_ = ZSTD_createCCtx();
    }
    size_t zresult = 0;
    IF_ZSTD_COMP_ERROR_LOG_AND_RETURN(ZSTD_CCtx_reset(zstdContext_, ZSTD_reset_session_only));
    IF_ZSTD_COMP_ERROR_LOG_AND_RETURN(ZSTD_CCtx_setParameter(
        zstdContext_, ZSTD_c_compressionLevel, zstdPresetToCompressionLevel(preset)));
    IF_ZSTD_COMP_ERROR_LOG_AND_RETURN(ZSTD_CCtx_setPledgedSrcSize(zstdContext_, dataSize));
    size_t result = ZSTD_compress2(
        zstdContext_, buffer.data() + headerSpace, maxCompressedSize, data, dataSize);
    if (ZSTD_isError(result)) {
      const ZSTD_ErrorCode errorCode = ZSTD_getErrorCode(result);
      const char* errorMessage = ZSTD_getErrorString(errorCode);
      XR_LOGE("Compression error {}", errorMessage);
      compressionType_ = CompressionType::None;
      return domainErrorCode(ErrorDomain::ZstdCompressionErrorDomain, errorCode, errorMessage);
    }
    // if the compressed data isn't smaller, don't use it!
    if (result < dataSize) {
      compressionType_ = CompressionType::Zstd;
      outCompressedSize = static_cast<uint32_t>(result);
    } else {
      compressionType_ = CompressionType::None;
    }
    return SUCCESS;
  }
  int compressBuffers(
      helpers::IOVector<uint8_t>& buffer,
      const DataSlice* dataSlices,
      size_t dataSliceCount,
      CompressionPreset preset,
      uint32_t& outCompressedSize,
      size_t headerSpace,
      size_t totalSize) {
    if (preset >= CompressionPreset::FirstLz4Preset && preset <= CompressionPreset::LastLz4Preset) {
      return lz4CompressBuffers(
          buffer, dataSlices, dataSliceCount, preset, outCompressedSize, headerSpace, totalSize);
    }
    return zstdCompressBuffers(
        buffer, dataSlices, dataSliceCount, preset, outCompressedSize, headerSpace, totalSize);
  }
  int lz4CompressBuffers(
      helpers::IOVector<uint8_t>& buffer,
      const DataSlice* dataSlices,
      size_t dataSliceCount,
      CompressionPreset preset,
      uint32_t& outCompressedSize,
      size_t headerSpace,
      size_t totalSize) {
    outCompressedSize = 0;
    const LZ4F_preferences_t* prefs = getLz4Preferences(preset);
    // Each LZ4F_compressBound() bounds a single compressUpdate; sum them, plus the frame header and
    // the end-of-frame flush. (Unlike zstd, LZ4F has no whole-payload bound for multi-call
    // framing.)
    size_t maxCompressedSize = LZ4F_HEADER_SIZE_MAX + LZ4F_compressBound(0, prefs);
    for (size_t i = 0; i < dataSliceCount; ++i) {
      maxCompressedSize += LZ4F_compressBound(dataSlices[i].size, prefs);
    }
    if (buffer.size() < headerSpace + maxCompressedSize) {
      buffer.resizeDiscardingWithoutInitialization(headerSpace + maxCompressedSize);
    }
    size_t lzresult = 0;
    if (lz4Context_ == nullptr) {
      IF_LZ4_COMP_ERROR_LOG_AND_RETURN(LZ4F_createCompressionContext(&lz4Context_, LZ4F_VERSION));
    }
    auto getError = [this](size_t result) -> int {
      if (!LZ4F_isError(result)) {
        return SUCCESS;
      }
      const char* errorMessage = LZ4F_getErrorName(result);
      XR_LOGE("Compression error {}", errorMessage);
      compressionType_ = CompressionType::None;
      const int error = domainErrorCode(
          ErrorDomain::Lz4CompressionErrorDomain, lz4ErrorCodeFromResult(result), errorMessage);
      // The context is left mid-frame on error; drop it so the next call starts clean.
      LZ4F_freeCompressionContext(lz4Context_);
      lz4Context_ = nullptr;
      return error;
    };
    uint8_t* out = buffer.data() + headerSpace;
    size_t pos = 0;
    size_t written = LZ4F_compressBegin(lz4Context_, out + pos, maxCompressedSize - pos, prefs);
    int error = getError(written);
    if (error != SUCCESS) {
      return error;
    }
    pos += written;
    for (size_t i = 0; i < dataSliceCount; ++i) {
      written = LZ4F_compressUpdate(
          lz4Context_,
          out + pos,
          maxCompressedSize - pos,
          dataSlices[i].data,
          dataSlices[i].size,
          nullptr);
      error = getError(written);
      if (error != SUCCESS) {
        return error;
      }
      pos += written;
    }
    written = LZ4F_compressEnd(lz4Context_, out + pos, maxCompressedSize - pos, nullptr);
    error = getError(written);
    if (error != SUCCESS) {
      return error;
    }
    pos += written;
    // if the compressed data isn't smaller, don't use it!
    if (pos < totalSize) {
      compressionType_ = CompressionType::Lz4;
      outCompressedSize = static_cast<uint32_t>(pos);
    } else {
      compressionType_ = CompressionType::None;
    }
    return SUCCESS;
  }
  int zstdCompressBuffers(
      helpers::IOVector<uint8_t>& buffer,
      const DataSlice* dataSlices,
      size_t dataSliceCount,
      CompressionPreset preset,
      uint32_t& outCompressedSize,
      size_t headerSpace,
      size_t totalSize) {
    outCompressedSize = 0;
    size_t maxCompressedSize = ZSTD_compressBound(totalSize);
    if (buffer.size() < headerSpace + maxCompressedSize) {
      buffer.resizeDiscardingWithoutInitialization(headerSpace + maxCompressedSize);
    }
    if (zstdContext_ == nullptr) {
      zstdContext_ = ZSTD_createCCtx();
    }
    size_t zresult = 0;
    IF_ZSTD_COMP_ERROR_LOG_AND_RETURN(ZSTD_CCtx_reset(zstdContext_, ZSTD_reset_session_only));
    IF_ZSTD_COMP_ERROR_LOG_AND_RETURN(ZSTD_CCtx_setParameter(
        zstdContext_, ZSTD_c_compressionLevel, zstdPresetToCompressionLevel(preset)));
    IF_ZSTD_COMP_ERROR_LOG_AND_RETURN(ZSTD_CCtx_setPledgedSrcSize(zstdContext_, totalSize));
    ZSTD_outBuffer output{buffer.data() + headerSpace, maxCompressedSize, 0};
    for (size_t i = 0; i < dataSliceCount; ++i) {
      ZSTD_inBuffer input{dataSlices[i].data, dataSlices[i].size, 0};
      const ZSTD_EndDirective mode = (i + 1 == dataSliceCount) ? ZSTD_e_end : ZSTD_e_continue;
      do {
        IF_ZSTD_COMP_ERROR_LOG_AND_RETURN(
            ZSTD_compressStream2(zstdContext_, &output, &input, mode));
      } while (mode == ZSTD_e_end ? zresult > 0 : input.pos < input.size);
    }
    // if the compressed data isn't smaller, don't use it!
    if (output.pos < totalSize) {
      compressionType_ = CompressionType::Zstd;
      outCompressedSize = static_cast<uint32_t>(output.pos);
    } else {
      compressionType_ = CompressionType::None;
    }
    return SUCCESS;
  }
  int startFrame(size_t dataSize, CompressionPreset zstdPreset) {
    if (zstdContext_ == nullptr) {
      zstdContext_ = ZSTD_createCCtx();
    }
    size_t zresult = 0;
    IF_ZSTD_COMP_ERROR_LOG_AND_RETURN(ZSTD_CCtx_setParameter(
        zstdContext_, ZSTD_c_compressionLevel, zstdPresetToCompressionLevel(zstdPreset)));
    IF_ZSTD_COMP_ERROR_LOG_AND_RETURN(ZSTD_CCtx_setPledgedSrcSize(zstdContext_, dataSize));
    return SUCCESS;
  }
  int addFrameData(
      WriteFileHandler& file,
      ZSTD_inBuffer_s* input,
      ZSTD_outBuffer_s* output,
      uint32_t& inOutCompressedSize,
      size_t maxCompressedSize,
      bool endFrame) {
    size_t zresult = 0;
    do {
      IF_ZSTD_COMP_ERROR_LOG_AND_RETURN(ZSTD_compressStream2(
          zstdContext_, output, input, endFrame ? ZSTD_e_end : ZSTD_e_continue));
      if (output->pos > 0) {
        if (maxCompressedSize > 0 && inOutCompressedSize + output->pos > maxCompressedSize) {
          ZSTD_CCtx_reset(zstdContext_, ZSTD_reset_session_only);
          return TOO_MUCH_DATA;
        }
        WRITE_OR_LOG_AND_RETURN(file, output->dst, output->pos);
        inOutCompressedSize += static_cast<uint32_t>(output->pos);
        output->pos = 0;
      }
    } while (endFrame ? (input->pos < input->size || zresult > 0) : (input->pos < input->size));
    return SUCCESS;
  }

  void resetCompressionType() {
    compressionType_ = CompressionType::None;
  }

  CompressionType getCompressionType() const {
    return compressionType_;
  }

 private:
  static const LZ4F_preferences_t* getLz4Preferences(CompressionPreset lz4Preset) {
    static LZ4F_preferences_t sLz4FastPreset;
    static LZ4F_preferences_t sLz4TightPreset;
    static once_flag sPresetInitFlag;
    call_once(sPresetInitFlag, [] {
      memset(&sLz4FastPreset, 0, sizeof(LZ4F_preferences_t));
      memset(&sLz4TightPreset, 0, sizeof(LZ4F_preferences_t));
      // Max compression for lz4, or close to.
      // Higher numbers take much much longer, but give only minimal additional gains.
      // 4 seems like the sweet spot. You should probably use zstd instead.
      sLz4TightPreset.compressionLevel = 4;
    });
    // default to fast preset
    return lz4Preset == CompressionPreset::Lz4Tight ? &sLz4TightPreset : &sLz4FastPreset;
  }

  ZSTD_CCtx* zstdContext_ = nullptr;
  LZ4F_compressionContext_t lz4Context_ = nullptr;
  CompressionType compressionType_;
};

string toPrettyName(CompressionPreset preset) {
  auto nameIter = sPresetNames.find(preset);
  string name;
  if (nameIter != sPresetNames.end()) {
    name = nameIter->second;
  } else {
    name = fmt::format("Preset index #{}", static_cast<int>(preset));
  }
  if (preset >= CompressionPreset::FirstZstdPreset && preset <= CompressionPreset::LastZstdPreset) {
    fmt::format_to(std::back_inserter(name), "({})", zstdPresetToCompressionLevel(preset));
  }
  return name;
}

Compressor::Compressor() : impl_(new CompressorImpl()) {}

Compressor::~Compressor() = default;

int Compressor::compress(
    const void* data,
    size_t dataSize,
    CompressionPreset preset,
    uint32_t& outCompressedSize,
    size_t headerSpace) {
  outCompressedSize = 0;
  impl_->resetCompressionType();
  if (exceedsUint32Max(dataSize)) {
    return TOO_MUCH_DATA;
  }
  if (shouldTryToCompress(preset, dataSize)) {
    if (preset >= CompressionPreset::FirstLz4Preset && preset <= CompressionPreset::LastLz4Preset) {
      return impl_->lz4Compress(buffer_, data, dataSize, preset, outCompressedSize, headerSpace);
    }
    if (preset >= CompressionPreset::FirstZstdPreset &&
        preset <= CompressionPreset::LastZstdPreset) {
      return impl_->zstdCompress(buffer_, data, dataSize, preset, outCompressedSize, headerSpace);
    }
  }
  return SUCCESS;
}

int Compressor::compress(
    const DataSlice* dataSlices,
    size_t dataSliceCount,
    CompressionPreset preset,
    uint32_t& outCompressedSize,
    size_t headerSpace) {
  outCompressedSize = 0;
  impl_->resetCompressionType();
  // A single slice must stay byte-identical to the one-shot path: one-shot zstd/lz4 sets the
  // frame's single-segment flag that the multi-call streaming path does not, so delegate it.
  if (dataSliceCount == 1) {
    return compress(dataSlices[0].data, dataSlices[0].size, preset, outCompressedSize, headerSpace);
  }
  size_t totalSize = 0;
  for (size_t i = 0; i < dataSliceCount; ++i) {
    if (dataSlices[i].size > std::numeric_limits<uint32_t>::max() - totalSize) {
      return TOO_MUCH_DATA;
    }
    totalSize += dataSlices[i].size;
  }
  if (shouldTryToCompress(preset, totalSize)) {
    return impl_->compressBuffers(
        buffer_, dataSlices, dataSliceCount, preset, outCompressedSize, headerSpace, totalSize);
  }
  return SUCCESS;
}

int Compressor::startFrame(size_t frameSize, CompressionPreset zstdPreset, uint32_t& outSize) {
  outSize = 0;
  size_t minOutSize = ZSTD_CStreamOutSize();
  if (buffer_.size() < minOutSize) {
    buffer_.resizeDiscardingWithoutInitialization(minOutSize);
  }
  return impl_->startFrame(frameSize, zstdPreset);
}

int Compressor::addFrameData(
    WriteFileHandler& file,
    const void* data,
    size_t dataSize,
    uint32_t& inOutCompressedSize,
    size_t maxCompressedSize) {
  ZSTD_inBuffer_s input{data, dataSize, 0};
  ZSTD_outBuffer_s output{buffer_.data(), buffer_.size(), 0};
  return impl_->addFrameData(
      file, &input, &output, inOutCompressedSize, maxCompressedSize, /* endFrame */ false);
}

int Compressor::endFrame(
    WriteFileHandler& file,
    uint32_t& inOutCompressedSize,
    size_t maxCompressedSize) {
  ZSTD_inBuffer_s input{nullptr, 0, 0};
  ZSTD_outBuffer_s output{buffer_.data(), buffer_.size(), 0};
  return impl_->addFrameData(
      file, &input, &output, inOutCompressedSize, maxCompressedSize, /* endFrame */ true);
}

CompressionType Compressor::getCompressionType() const {
  return impl_->getCompressionType();
}

bool Compressor::shouldTryToCompress(CompressionPreset preset, size_t size) {
  if (preset == CompressionPreset::None) {
    return false;
  }
  return size >= Compressor::kMinByteCountForCompression;
}

} // namespace vrs
