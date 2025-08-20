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

#include "ContentBlockReader.h"

#define DEFAULT_LOG_CHANNEL "ContentBlockReader"
#include <logging/Log.h>
#include <logging/Verify.h>

#include <vrs/helpers/FileMacros.h>
#include <vrs/helpers/Throttler.h>
#include <vrs/os/CompilerAttributes.h>

#include "FileFormat.h"
#include "RecordFormatStreamPlayer.h"
#include "RecordReaders.h"

using namespace std;

namespace {

vrs::utils::Throttler& getThrottler() {
  static vrs::utils::Throttler sThrottler;
  return sThrottler;
}

} // namespace
namespace vrs {

namespace {
// ContentBlocks may rely on the most recent configuration record to fully define the format of
// their content. When we detect that an image or audio block's format can't be fully defined, we
// log a warning to help debug the problem. Maybe the configuration record exists, but wasn't read
// yet? Maybe the last read configuration record has a later timestamp than the data record?
bool mayUsePastConfigurationReader(
    const CurrentRecord& record,
    RecordFormatReader* reader,
    ContentType type) {
  if (reader != nullptr && reader->lastReadRecordTimestamp <= record.timestamp) {
    return true;
  }
  MAYBE_UNUSED const char* typeName = (type == ContentType::IMAGE) ? "image" : "audio";
  MAYBE_UNUSED const char* cause = (reader == nullptr)
      ? "no configuration record was read prior to reading this"
      : "the most recent configuration record read for this stream has a newer"
        " timestamp than this";
  THROTTLED_LOGW(
      record.fileReader,
      "Can't define the {} block format for {} to read this {} block with DataLayout. "
      "This might be happening, because the {} format is defined in a configuration record using "
      "datalayout conventions, but {} {} record.",
      typeName,
      record.streamId.getName(),
      typeName,
      typeName,
      cause,
      toString(record.recordType));
  return false;
}

/// Convenience function to map a DataLayoutBlockReader's desired layout to a record block layout.
/// @param reader: the RecordFormatReader to get the DataLayoutBlockReader from.
/// @param blockIndex: index of the block to use.
/// @param desiredLayout: Layout to map to the record's layout.
/// @return True there was a record layout to map to, False otherwise.
bool mapToBlockLayout(RecordFormatReader* reader, size_t blockIndex, DataLayout& desiredLayout) {
  DataLayoutBlockReader* layoutReader =
      dynamic_cast<DataLayoutBlockReader*>(reader->contentReaders[blockIndex].get());
  return layoutReader != nullptr && layoutReader->mapToBlockLayout(desiredLayout);
}

} // namespace

unique_ptr<ContentBlockReader> ContentBlockReader::build(
    const RecordFormat& recordFormat,
    size_t blockIndex,
    unique_ptr<DataLayout>&& blockLayout) {
  const ContentBlock& contentBlock = recordFormat.getContentBlock(blockIndex);
  ContentBlockReader* reader = nullptr;
  switch (contentBlock.getContentType()) {
    case ContentType::EMPTY:
      reader = new EmptyBlockReader(recordFormat, blockIndex);
      break;

    case ContentType::CUSTOM:
      reader = new CustomBlockReader(recordFormat, blockIndex);
      break;

    case ContentType::IMAGE:
      reader = new ImageBlockReader(recordFormat, blockIndex);
      break;

    case ContentType::AUDIO:
      reader = new AudioBlockReader(recordFormat, blockIndex);
      break;

    case ContentType::DATA_LAYOUT:
      reader = new DataLayoutBlockReader(recordFormat, blockIndex, std::move(blockLayout));
      break;

    case ContentType::COUNT:
      reader = new UnsupportedBlockReader(recordFormat, blockIndex);
      break;
  }
  return unique_ptr<ContentBlockReader>(reader);
}

ContentBlockReader::~ContentBlockReader() = default;

bool ContentBlockReader::findNextContentBlockSpec(
    const CurrentRecord& record,
    RecordFormatStreamPlayer& player) {
  if (contentBlockSpec_) {
    return contentBlockSpec_->isMapped();
  }
  contentBlockSpec_ = make_unique<datalayout_conventions::NextContentBlockSpec>();
  const size_t index = blockIndex_ - 1;
  RecordFormatReader* reader = player.getCurrentRecordFormatReader();
  return reader->recordFormat.getContentBlock(index).getContentType() == ContentType::DATA_LAYOUT &&
      mapToBlockLayout(reader, index, *contentBlockSpec_);
}

size_t ContentBlockReader::findContentBlockSize(
    const CurrentRecord& record,
    RecordFormatStreamPlayer& player) {
  uint32_t size = 0;
  if (blockIndex_ > 0 && findNextContentBlockSpec(record, player) &&
      contentBlockSpec_->nextContentBlockSize.get(size)) {
    return size;
  }
  // maybe we can compute the size of the block using the amount of data left to read...
  return recordFormat_.getBlockSize(blockIndex_, record.reader->getUnreadBytes());
}

uint32_t ContentBlockReader::findAudioSampleCount(
    const CurrentRecord& record,
    RecordFormatStreamPlayer& player) {
  return blockIndex_ > 0 && findNextContentBlockSpec(record, player)
      ? contentBlockSpec_->nextAudioContentBlockSampleCount.get()
      : 0;
}

#define CONTENT_SIZE_STR(CONTENT_SIZE) \
  ((CONTENT_SIZE) == ContentBlock::kSizeUnknown) ? string("???") : to_string(CONTENT_SIZE)

bool AudioBlockReader::readAudioContentBlock(
    const CurrentRecord& record,
    RecordFormatStreamPlayer& player,
    const ContentBlock& contentBlock) {
  const AudioContentBlockSpec& spec = contentBlock.audio();
  size_t contentBlockSize = findContentBlockSize(record, player);
  if (spec.getAudioFormat() != AudioFormat::PCM) {
    if (contentBlockSize != ContentBlock::kSizeUnknown) {
      return player.onAudioRead(record, blockIndex_, ContentBlock(spec, contentBlockSize));
    }
  } else if (spec.getSampleCount() == 0) {
    // PCM audio without a sample count
    if (contentBlockSize != ContentBlock::kSizeUnknown) {
      // The sample count is undefined, maybe we can to do the math using the block size
      uint8_t sampleFrameStride = spec.getSampleFrameStride();
      if (sampleFrameStride > 0 && (contentBlockSize % sampleFrameStride) == 0) {
        // update contentBlock with the actual sample count
        return player.onAudioRead(
            record,
            blockIndex_,
            ContentBlock(
                AudioFormat::PCM,
                spec.getSampleFormat(),
                spec.getChannelCount(),
                spec.getSampleFrameStride(),
                spec.getSampleRate(),
                static_cast<uint32_t>(contentBlockSize / sampleFrameStride)));
      }
    }
  } else {
    // PCM audio with a sample count
    size_t pcmSize = spec.getPcmBlockSize();
    if (pcmSize != ContentBlock::kSizeUnknown &&
        (contentBlockSize == pcmSize || contentBlockSize == ContentBlock::kSizeUnknown)) {
      return player.onAudioRead(record, blockIndex_, ContentBlock(spec, pcmSize));
    }
  }
  THROTTLED_LOGW(
      record.fileReader,
      "Can't figure out audio content block {} while we have {} bytes.",
      spec.asString(),
      CONTENT_SIZE_STR(contentBlockSize));
  return player.onUnsupportedBlock(record, blockIndex_, contentBlock);
}

bool AudioBlockReader::readBlock(const CurrentRecord& record, RecordFormatStreamPlayer& player) {
  const ContentBlock& contentBlock = recordFormat_.getContentBlock(blockIndex_);
  // check if we already have enough information from the record format to extract the audio data
  if (contentBlock.audio().isSampleBlockFormatDefined()) {
    return readAudioContentBlock(record, player, contentBlock);
  }
  // if that is not the case, find definition from config or data record's DataLayout
  bool readNextBlock = true;
  // have we already successfully mapped our audio spec DataLayout? If so, use what we found.
  if (tryCurrentAudioSpec(record, player, readNextBlock)) {
    return readNextBlock;
  }
  // check if there is a valid definition in a datalayout just before this audio content block
  RecordFormatReader* reader = player.getCurrentRecordFormatReader();
  if (blockIndex_ > 0 &&
      findAudioSpec(record, player, reader, blockIndex_, blockIndex_ - 1, readNextBlock)) {
    return readNextBlock;
  }
  // find a dalayout defintion in the last configuration record read before this record
  if (record.recordType != Record::Type::CONFIGURATION) {
    reader = player.getLastRecordFormatReader(record.streamId, Record::Type::CONFIGURATION);
    if (mayUsePastConfigurationReader(record, reader, contentBlock.getContentType()) &&
        findAudioSpec(
            record, player, reader, reader->recordFormat.getUsedBlocksCount(), 0, readNextBlock)) {
      return readNextBlock;
    }
  }
  // we tried everything...
  return player.onUnsupportedBlock(record, blockIndex_, contentBlock);
}

bool AudioBlockReader::findAudioSpec(
    const CurrentRecord& record,
    RecordFormatStreamPlayer& player,
    RecordFormatReader* reader,
    size_t indexUpperLimit,
    size_t lastIndexToCheck,
    bool& readNextBlock) {
  // check datalayout backwards before indexUpperLimit, but >= lastIndexToCheck
  size_t index = indexUpperLimit;
  while (index-- > lastIndexToCheck) {
    if (reader->recordFormat.getContentBlock(index).getContentType() == ContentType::DATA_LAYOUT) {
      if (mapToBlockLayout(reader, index, audioSpec_)) {
        return tryCurrentAudioSpec(record, player, readNextBlock);
      }
    }
  }
  return false;
}

bool AudioBlockReader::audioContentFromAudioSpec(
    const CurrentRecord& record,
    RecordFormatStreamPlayer& player,
    ContentBlock& outAudioContentBlock) {
  AudioFormat audioFormat = AudioFormat::UNDEFINED;
  AudioSampleFormat sampleFormat = AudioSampleFormat::UNDEFINED;
  uint8_t numChannels = 0;
  uint32_t sampleRate = 0;
  // if audioFormat is missing, assume it's AudioFormat::PCM (legacy behavior)
  if (!audioSpec_.audioFormat.get(audioFormat)) {
    audioFormat = AudioFormat::PCM;
  }
  // check minimal set of required fields
  if (enumIsValid(audioFormat) &&
      (audioSpec_.sampleType.get(sampleFormat) && enumIsValid(sampleFormat)) &&
      (audioSpec_.channelCount.get(numChannels) && numChannels > 0) &&
      (audioSpec_.sampleRate.get(sampleRate) && sampleRate > 0)) {
    // everything looks fine, check optional fields
    uint8_t sampleFrameStride = 0;
    uint32_t minFrameSize = AudioContentBlockSpec::getBytesPerSample(sampleFormat) * numChannels;
    // If sampleStride field is set, perform a sanity check based on the format. Assume that any
    // meaningful alignment of a sample won't be longer than additional 3 bytes per channel e.g. if
    // uint8_t samples is stored in uint32_t for some reason
    if (audioSpec_.sampleStride.get(sampleFrameStride) && sampleFrameStride > 0 &&
        (sampleFrameStride < minFrameSize || sampleFrameStride > minFrameSize + numChannels * 3)) {
      // has invalid block align
      return false;
    }
    uint32_t sampleCount = 0;
    if (!audioSpec_.sampleCount.get(sampleCount) || sampleCount == 0) {
      sampleCount = findAudioSampleCount(record, player);
    }

    uint8_t stereoPairCount = 0;
    audioSpec_.stereoPairCount.get(stereoPairCount);

    outAudioContentBlock = ContentBlock(
        audioFormat,
        sampleFormat,
        numChannels,
        sampleFrameStride,
        sampleRate,
        sampleCount,
        stereoPairCount);
    return true;
  }
  return false;
}

// Try to use the current audioSpec_ to fill in details about the audio content block
// Returns false when the spec are incomplete
// Returns true if the spec are complete & the audio callback was called setting readNextBlock.
bool AudioBlockReader::tryCurrentAudioSpec(
    const CurrentRecord& record,
    RecordFormatStreamPlayer& player,
    bool& readNextBlock) {
  // try to use current audioSpec
  ContentBlock contentBlock;
  if (audioContentFromAudioSpec(record, player, contentBlock)) {
    // try to interpret the rest of the record with the updated content block
    readNextBlock = readAudioContentBlock(record, player, contentBlock);
    return true;
  }
  // audioSpec does not contain enough details
  return false;
}

bool ImageBlockReader::readBlock(const CurrentRecord& record, RecordFormatStreamPlayer& player) {
  const ContentBlock& contentBlock = recordFormat_.getContentBlock(blockIndex_);
  ImageFormat imageFormat = contentBlock.image().getImageFormat();
  // Is the ContentBlock description already descriptive enough?
  if (imageFormat == ImageFormat::RAW &&
      contentBlock.getBlockSize() != ContentBlock::kSizeUnknown) {
    return onImageFound(record, player, contentBlock);
  }
  // Have we already successfully mapped our imageSpec_ DataLayout? If so, use it.
  // Find content-block's size for except for image/raw, for which it isn't needed
  size_t contentBlockSize = (imageFormat != ImageFormat::RAW) ? findContentBlockSize(record, player)
                                                              : ContentBlock::kSizeUnknown;
  if (imageSpec_.isMapped()) {
    ContentBlock imageContentBlock =
        imageSpec_.getImageContentBlock(contentBlock.image(), contentBlockSize);
    if (imageContentBlock.getContentType() == ContentType::IMAGE) {
      return onImageFound(record, player, imageContentBlock);
    }
  }
  // Search for a DataLayout that has enough data to interpret the image data
  if (imageFormat == ImageFormat::RAW || imageFormat == ImageFormat::CUSTOM_CODEC ||
      imageFormat == ImageFormat::VIDEO) {
    RecordFormatReader* reader = player.getCurrentRecordFormatReader();
    bool readNextBlock = true;
    // Use a lambda to repeat the search using many local variables and few differences...
    auto findImageSpec = [&](size_t countOfBlocksToSearch) {
      // look for last datalayout before this block
      size_t index = countOfBlocksToSearch;
      while (index-- > 0) {
        if (reader->recordFormat.getContentBlock(index).getContentType() ==
            ContentType::DATA_LAYOUT) {
          if (mapToBlockLayout(reader, index, imageSpec_)) {
            ContentBlock imageContentBlock =
                imageSpec_.getImageContentBlock(contentBlock.image(), contentBlockSize);
            if (imageContentBlock.getContentType() == ContentType::IMAGE) {
              readNextBlock = onImageFound(record, player, imageContentBlock);
              return true;
            }
          }
        }
      }
      return false;
    };
    if (blockIndex_ > 0 && findImageSpec(blockIndex_)) {
      return readNextBlock;
    }
    if (record.recordType != Record::Type::CONFIGURATION) {
      reader = player.getLastRecordFormatReader(record.streamId, Record::Type::CONFIGURATION);
      if (mayUsePastConfigurationReader(record, reader, contentBlock.getContentType()) &&
          findImageSpec(reader->recordFormat.getUsedBlocksCount())) {
        return readNextBlock;
      }
    }
  } else if (contentBlockSize != ContentBlock::kSizeUnknown) {
    return onImageFound(record, player, {contentBlock, contentBlockSize});
  }

  return player.onUnsupportedBlock(record, blockIndex_, contentBlock);
}

bool ImageBlockReader::onImageFound(
    const CurrentRecord& record,
    RecordFormatStreamPlayer& player,
    const ContentBlock& contentBlock) {
  if (contentBlock.image().getImageFormat() == ImageFormat::VIDEO) {
    if (!videoFrameSpec_) {
      videoFrameSpec_ = make_unique<datalayout_conventions::VideoFrameSpec>();
      // Video frames spec must specified in a datalayout just before the image block
      RecordFormatReader* reader = player.getCurrentRecordFormatReader();
      if (blockIndex_ > 0 &&
          reader->recordFormat.getContentBlock(blockIndex_ - 1).getContentType() ==
              ContentType::DATA_LAYOUT) {
        mapToBlockLayout(reader, blockIndex_ - 1, *videoFrameSpec_);
      }
    }
    if (videoFrameSpec_->isMapped() && videoFrameSpec_->hasVideoSpec()) {
      return player.onImageRead(
          record,
          blockIndex_,
          {contentBlock,
           videoFrameSpec_->keyFrameTimestamp.get(),
           videoFrameSpec_->keyFrameIndex.get()});
    }
  }
  return player.onImageRead(record, blockIndex_, contentBlock);
}

bool CustomBlockReader::readBlock(const CurrentRecord& record, RecordFormatStreamPlayer& player) {
  const ContentBlock& contentBlock = recordFormat_.getContentBlock(blockIndex_);
  // the record format specifies a size: use that
  if (contentBlock.getBlockSize() != ContentBlock::kSizeUnknown) {
    return player.onCustomBlockRead(record, blockIndex_, contentBlock);
  }
  // find the size some other way
  size_t size = findContentBlockSize(record, player);
  if (size != ContentBlock::kSizeUnknown) {
    return player.onCustomBlockRead(record, blockIndex_, {contentBlock, size});
  }
  // give up
  return player.onUnsupportedBlock(record, blockIndex_, contentBlock);
}

bool UnsupportedBlockReader::readBlock(
    const CurrentRecord& record,
    RecordFormatStreamPlayer& player) {
  const ContentBlock& contentBlock = recordFormat_.getContentBlock(blockIndex_);
  return player.onUnsupportedBlock(record, blockIndex_, contentBlock);
}

bool DataLayoutBlockReader::readBlock(
    const CurrentRecord& record,
    RecordFormatStreamPlayer& player) {
  if (!blockLayout_) {
    return false;
  }
  // DataLayouts have two parts:
  // 1- a fixed size part, which includes the fixed size pieces' data,
  // plus the index for the variable size part (in any), which has a known & fixed size.
  // 2- the data for the variable size pieces
  // The size of the variable size buffer can be read from the var size index, so we read
  // the fixed size buffer first, extract the size of the var size data from the var size index,
  // so we can then read the var size buffer...
  const size_t kMaxFixedDataSize = 1024 * 1024 * 1024; // 1GB, arbitrary limit
  const size_t kMaxRecordSize = 4 * 1024 * 1024 * 1024UL - sizeof(FileFormat::RecordHeader);
  DataLayout& layout = *blockLayout_;
  vector<int8_t>& fixedData = layout.getFixedData();
  size_t fixedDataSize = layout.getFixedDataSizeNeeded();
  if (!XR_VERIFY(fixedDataSize <= kMaxFixedDataSize)) {
    return false;
  }
  fixedData.resize(fixedDataSize);
  vector<int8_t>& varData = layout.getVarData();
  int readBlockStatus = record.reader->read(fixedData);
  if (readBlockStatus == 0) {
    size_t varDataSize = layout.getVarDataSizeFromIndex();
    if (!XR_VERIFY(fixedDataSize + varDataSize <= kMaxRecordSize)) {
      return false;
    }
    varData.resize(varDataSize);
    if (varDataSize > 0) {
      readBlockStatus = record.reader->read(varData);
    }
  } else {
    varData.resize(0);
  }
  if (VERIFY_SUCCESS(readBlockStatus)) {
    return player.onDataLayoutRead(record, blockIndex_, layout);
  }
  return false;
}

bool EmptyBlockReader::readBlock(const CurrentRecord&, RecordFormatStreamPlayer&) {
  return true; // just continue to the next block
}

} // namespace vrs
