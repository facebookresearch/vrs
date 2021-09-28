// Copyright (c) Facebook, Inc. and its affiliates. Confidential and proprietary.

#include "XprsManager.h"

#include <set>

#define DEFAULT_LOG_CHANNEL "XprsManager"
#include <logging/Log.h>
#include <logging/Verify.h>

#include <vrs/DataLayoutConventions.h>

#include "XprsEncoder.h"

namespace vrs::vxprs {

using namespace std;

using ImageSpec = DataLayoutConventions::ImageSpec;

bool isCompressCandidate(
    RecordFileReader& reader,
    StreamId id,
    const EncoderOptions& encoderOptions,
    BlockId& imageSpecBlock,
    BlockId& pixelBlock) {
  struct Details : public RecordFormatStreamPlayer {
    Details(const EncoderOptions& encoderOptions) : encoderOptions_{encoderOptions} {}
    set<BlockId> dataLayouts, imageSpecs, rawImageBlocks;
    double timestamp = -1;
    bool onDataLayoutRead(const CurrentRecord& record, size_t idx, DataLayout& dl) override {
      timestamp = record.timestamp;
      BlockId thisBlock{record, idx};
      dataLayouts.insert(thisBlock);
      ImageSpec& imageConfig = getExpectedLayout<ImageSpec>(dl, idx);
      ContentBlock imageBlock = imageConfig.getImageContentBlock(ImageFormat::RAW);
      if (imageBlock.getContentType() == ContentType::IMAGE) {
        string codecName;
        xprs::VideoCodec videoCodec;
        xprs::EncoderConfig encoderConfig;
        if (imageSpecToVideoCodec(
                imageBlock.image(), encoderOptions_, codecName, videoCodec, encoderConfig)) {
          imageSpecs.insert(thisBlock);
        } else {
          cout << "Found " << imageBlock.asString() << ", but pixel format "
               << imageBlock.image().getPixelFormatAsString() << " is not supported." << endl;
        }
      }
      return true;
    }
    bool onImageRead(const CurrentRecord& record, size_t idx, const ContentBlock& cb) override {
      if (record.recordType == Record::Type::CONFIGURATION) {
        return RecordFormatStreamPlayer::onImageRead(record, idx, cb);
      } else {
        BlockId thisBlock{record, idx};
        if (cb.image().getImageFormat() == ImageFormat::RAW) {
          rawImageBlocks.insert(thisBlock);
        }
      }
      return true;
    }
    const EncoderOptions& encoderOptions_;
  } collector(encoderOptions);
  reader.readFirstConfigurationRecord(id, &collector);
  if (collector.dataLayouts.empty()) {
    cout << "No config record found in " << id.getName() << ", skipping..." << endl;
    return false;
  }
  const IndexRecord::RecordInfo* dataRec =
      reader.getRecordByTime(id, Record::Type::DATA, collector.timestamp);
  if (dataRec == nullptr) {
    cout << "No data record in " << id.getName() << ", skipping..." << endl;
    return false;
  }
  reader.readRecord(*dataRec, &collector);
  if (collector.rawImageBlocks.empty()) {
    cout << "No supported images found in " << id.getName() << ", skipping..." << endl;
    return false;
  } else if (collector.rawImageBlocks.size() > 1) {
    cout << collector.rawImageBlocks.size() << " raw images found in " << id.getName()
         << ", 1 max supported, skipping..." << endl;
    return false;
  }
  pixelBlock = *collector.rawImageBlocks.begin();
  BlockId beforePixels{pixelBlock.recordType, pixelBlock.formatVersion, pixelBlock.blockIndex - 1};
  if (pixelBlock.blockIndex < 1 ||
      collector.dataLayouts.find(beforePixels) == collector.dataLayouts.end()) {
    cout << "No datalayout found before image in " << id.getName() << ", skipping..." << endl;
    return false;
  }
  if (collector.imageSpecs.find(beforePixels) != collector.imageSpecs.end()) {
    imageSpecBlock = beforePixels;
  } else {
    imageSpecBlock.clear();
    for (const auto& blockId : collector.imageSpecs) {
      if (blockId.recordType == Record::Type::CONFIGURATION) {
        imageSpecBlock = blockId;
        break;
      }
    }
    if (!imageSpecBlock.isValid()) {
      cout << "No raw pixel image spec found in " << id.getName() << ", skipping..." << endl;
      return false;
    }
  }
  RecordFormat pixelRecord;
  XR_VERIFY(
      reader.getRecordFormat(id, pixelBlock.recordType, pixelBlock.formatVersion, pixelRecord));
  cout << "Found raw images to compress at " << pixelBlock.asString() << " in "
       << pixelRecord.asString() << endl;
  RecordFormat imageSpecRecord;
  XR_VERIFY(reader.getRecordFormat(
      id, imageSpecBlock.recordType, imageSpecBlock.formatVersion, imageSpecRecord));
  cout << "Will save codec name in " << imageSpecBlock.asString() << " in "
       << imageSpecRecord.asString() << endl;
  return true;
}

utils::MakeStreamFilterFunction makeStreamFilter(const EncoderOptions& encoderOptions) {
  return [encoderOptions](
             RecordFileReader& reader,
             RecordFileWriter& writer,
             StreamId streamId,
             const utils::CopyOptions& copyOptions) -> std::unique_ptr<StreamPlayer> {
    BlockId codecDatalayout, codecImage;
    if (isCompressCandidate(reader, streamId, encoderOptions, codecDatalayout, codecImage)) {
      return make_unique<XprsEncoder>(
          reader, writer, streamId, copyOptions, encoderOptions, codecDatalayout, codecImage);
    } else {
      return make_unique<utils::Copier>(reader, writer, streamId, copyOptions);
    }
  };
}

array<uint32_t, 256> getHistogram(const uint8_t* buffer, size_t length) {
  array<uint32_t, 256> histogram = {};
  const uint8_t* end = buffer + length;
  while (buffer < end) {
    histogram[*buffer++]++;
  }
  return histogram;
}

void printHistogram(const array<uint32_t, 256>& histogram) {
  size_t nzcount = 0;
  for (size_t i = 0; i < histogram.size(); i++) {
    if (histogram[i] != 0) {
      cout << i << "=" << histogram[i] << " ";
      nzcount++;
    }
  }
  cout << endl << "Non-zero values: " << nzcount << endl;
}

} // namespace vrs::vxprs
