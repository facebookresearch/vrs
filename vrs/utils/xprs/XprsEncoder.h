// (c) Facebook, Inc. and its affiliates. Confidential and proprietary.

#pragma once

#include <atomic>
#include <deque>
#include <memory>
#include <thread>

#include <xprs.h>

#include <vrs/os/Semaphore.h>

#include <vrs/DataLayout.h>
#include <vrs/DataLayoutConventions.h>
#include <vrs/utils/CopyHelpers.h>

#include "XprsManager.h"

namespace vrs::vxprs {

/// Option parameters to control codec selection.
struct EncoderOptions {
  /// If not null, only codec names that contain this string will be considered.
  /// e.g. "H.26" will allow "H.264" and "H.265" to be considered, but filter-out "VP9".
  string codecNameSearchStr;
  /// A list of xprs pixel format to use, when provided.
  xprs::PixelFormatList pixelFormats;
  /// Max number of frames between keyframes. 0 and 1 mean every frame is a keyframe.
  int32_t keyframeDistance;
  /// Target quality setting, 0 being the codec's default. See <xprs.h> for more details.
  uint8_t quality;
  /// Compression time budget to achieve target quality, balancing encoding time and file size.
  string preset;

  /// Copy default options from xprs's defaults when possible
  EncoderOptions() {
    xprs::EncoderConfig xprsDefaultConfig;
    keyframeDistance = xprsDefaultConfig.keyDistance;
    quality = xprsDefaultConfig.quality;
    preset = move(xprsDefaultConfig.preset);
  }
};

/// Given an image format spec, find a candidate codec and maybe create the encoder.
/// @param spec: Image spec of the images to encode.
/// @param outCodecName: on success, set to the found codec name.
/// @param outVideoCodec: on success, set to the encoder chosen.
/// @param outEncoderConfig: on success, set the encoder's configuration.
/// @param inOutEncoder: if provided and on success, instantiated encoder.
/// @return True if a candidate encoder was found. If inOutEncoder was provided, the encoder was
/// instantiated successfully.
/// Return False if no candidate encoder was found, or when inOutEncoder is provided, if none of the
/// potential candidates could be instantiated.
/// On failure, the values of all the "out" parameters are undefined.
bool imageSpecToVideoCodec(
    const ImageContentBlockSpec& spec,
    const EncoderOptions& encoderOptions,
    string& outCodecName,
    xprs::VideoCodec& outVideoCodec,
    xprs::EncoderConfig& outEncoderConfig,
    unique_ptr<xprs::IVideoEncoder>* inOutEncoder = nullptr);

class XprsEncoder : public vrs::utils::RecordFilterCopier {
 public:
  XprsEncoder(
      RecordFileReader& fileReader,
      RecordFileWriter& fileWriter,
      StreamId id,
      const utils::CopyOptions& copyOptions,
      const EncoderOptions& encoderOptions,
      BlockId imageSpecBlock,
      BlockId pixelBlock);
  ~XprsEncoder();

  bool shouldCopyVerbatim(const CurrentRecord& record) override;

  bool onDataLayoutRead(const CurrentRecord& record, size_t blockIndex, DataLayout&) override;
  bool onImageRead(const CurrentRecord& record, size_t blockIndex, const ContentBlock& cd) override;

  void processRecord(const CurrentRecord& record, uint32_t readSize) override;
  void finishRecordProcessing(const CurrentRecord& record) override;

  void flush() override;

 private:
  bool matchEncoderConfig(const ImageContentBlockSpec& spec) const;
  void encodeThread();
  void
  setupFrame(xprs::Frame& outFrame, const ImageContentBlockSpec& imageSpec, uint8_t* pixelBuffer);

  EncoderOptions encoderOptions_;
  BlockId imageSpecBlock_;
  BlockId pixelBlock_;
  unique_ptr<xprs::IVideoEncoder> encoder_;
  xprs::EncoderConfig encoderConfig_;
  xprs::VideoCodec codec_;
  string codecName_;
  PixelFormat pixelFormat_;
  double startTime_;
  vector<uint8_t> convertedFrame_;
  DataLayoutConventions::ImageSpecType keyFrameIndexValue_ = 0;
  double keyFrameTimestampValue_ = 0;
  unique_ptr<DataLayout> imageSpecCustomDataLayout_;
  unique_ptr<DataLayout> keyFrameCustomDataLayout_;
  DataPieceString* codecNamePiece_ = nullptr;
  DataPieceValue<DataLayoutConventions::ImageSpecType>* keyFrameIndexPiece_ = nullptr;
  DataPieceValue<double>* keyFrameTimestampPiece_ = nullptr;

  deque<unique_ptr<utils::ContentChunk>> finalChunks_;
  unique_ptr<utils::ContentBlockChunk> sourceImage_;
  double recordTime_;
  Record::Type recordType_;
  uint32_t formatVersion_;

  os::Semaphore encodeThreadReady_;
  os::Semaphore encodeJobReady_;
  std::atomic<bool> copyComplete_;
  std::thread thread_;
};

} // namespace vrs::vxprs
