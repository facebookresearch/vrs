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
#include <map>
#include <mutex>

#include <lz4frame.h>
#include <zstd.h>

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

#define IF_ZCOMP_ERROR_LOG_AND_RETURN(operation__)                                       \
  do {                                                                                   \
    zresult = operation__;                                                               \
    if (ZSTD_isError(zresult)) {                                                         \
      XR_LOGE("{} failed: {}, {}", #operation__, zresult, ZSTD_getErrorName(zresult));   \
      ZSTD_CCtx_reset(zstdContext_, ZSTD_reset_session_and_parameters);                  \
      return domainErrorCode(                                                            \
          ErrorDomain::ZstdCompressionErrorDomain, zresult, ZSTD_getErrorName(zresult)); \
    }                                                                                    \
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
  }
  uint32_t lz4Compress(
      vector<uninitialized_byte>& buffer,
      const void* data,
      size_t dataSize,
      CompressionPreset preset,
      size_t headerSpace) {
    const auto* prefs = getLz4Preferences(preset);
    size_t maxCompressedSize = LZ4F_compressFrameBound(dataSize, prefs);
    // increase our internal buffer size if necessary
    if (buffer.size() < headerSpace + maxCompressedSize) {
      buffer.resize(0); // avoid copy of current data when resizing
      buffer.resize(headerSpace + maxCompressedSize);
    }
    size_t result =
        LZ4F_compressFrame(buffer.data() + headerSpace, maxCompressedSize, data, dataSize, prefs);
    if (LZ4F_isError(result)) {
      XR_LOGE("Compression error {}", LZ4F_getErrorName(result));
      compressionType_ = CompressionType::None;
      return 0;
    }
    // if the compressed data isn't smaller, don't use it!
    if (result < dataSize) {
      compressionType_ = CompressionType::Lz4;
      return static_cast<uint32_t>(result);
    }
    compressionType_ = CompressionType::None;
    return 0;
  }
  uint32_t zstdCompress(
      vector<uninitialized_byte>& buffer,
      const void* data,
      size_t dataSize,
      CompressionPreset preset,
      size_t headerSpace) {
    size_t maxCompressedSize = ZSTD_compressBound(dataSize);
    // increase our internal buffer size if necessary
    if (buffer.size() < headerSpace + maxCompressedSize) {
      buffer.resize(0); // avoid copy of current data when resizing
      buffer.resize(headerSpace + maxCompressedSize);
    }
    if (zstdContext_ == nullptr) {
      zstdContext_ = ZSTD_createCCtx();
    }
    size_t zresult = 0;
    IF_ZCOMP_ERROR_LOG_AND_RETURN(ZSTD_CCtx_reset(zstdContext_, ZSTD_reset_session_only));
    IF_ZCOMP_ERROR_LOG_AND_RETURN(ZSTD_CCtx_setParameter(
        zstdContext_, ZSTD_c_compressionLevel, zstdPresetToCompressionLevel(preset)));
    IF_ZCOMP_ERROR_LOG_AND_RETURN(ZSTD_CCtx_setPledgedSrcSize(zstdContext_, dataSize));
    size_t result = ZSTD_compress2(
        zstdContext_, buffer.data() + headerSpace, maxCompressedSize, data, dataSize);
    if (ZSTD_isError(result)) {
      XR_LOGE("Compression error {}", ZSTD_getErrorName(result));
      compressionType_ = CompressionType::None;
      return 0;
    }
    // if the compressed data isn't smaller, don't use it!
    if (result < dataSize) {
      compressionType_ = CompressionType::Zstd;
      return static_cast<uint32_t>(result);
    }
    compressionType_ = CompressionType::None;
    return 0;
  }
  int startFrame(size_t dataSize, CompressionPreset zstdPreset) {
    if (zstdContext_ == nullptr) {
      zstdContext_ = ZSTD_createCCtx();
    }
    size_t zresult = 0;
    IF_ZCOMP_ERROR_LOG_AND_RETURN(ZSTD_CCtx_setParameter(
        zstdContext_, ZSTD_c_compressionLevel, zstdPresetToCompressionLevel(zstdPreset)));
    IF_ZCOMP_ERROR_LOG_AND_RETURN(ZSTD_CCtx_setPledgedSrcSize(zstdContext_, dataSize));
    return 0;
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
      IF_ZCOMP_ERROR_LOG_AND_RETURN(ZSTD_compressStream2(
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
    return 0;
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

uint32_t Compressor::compress(
    const void* data,
    size_t dataSize,
    CompressionPreset preset,
    size_t headerSpace) {
  if (shouldTryToCompress(preset, dataSize)) {
    if (preset >= CompressionPreset::FirstLz4Preset && preset <= CompressionPreset::LastLz4Preset) {
      return impl_->lz4Compress(buffer_, data, dataSize, preset, headerSpace);
    }
    if (preset >= CompressionPreset::FirstZstdPreset &&
        preset <= CompressionPreset::LastZstdPreset) {
      return impl_->zstdCompress(buffer_, data, dataSize, preset, headerSpace);
    }
  }
  return 0; // means failure
}

int Compressor::startFrame(size_t frameSize, CompressionPreset zstdPreset, uint32_t& outSize) {
  outSize = 0;
  size_t minOutSize = ZSTD_CStreamOutSize();
  if (buffer_.size() < minOutSize) {
    buffer_.resize(0); // avoid copy of current data when resizing
    buffer_.resize(minOutSize);
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
