// (c) Facebook, Inc. and its affiliates. Confidential and proprietary.

#include "ContentBlockReader.h"

#define DEFAULT_LOG_CHANNEL "ContentBlockReader"
#include <logging/Log.h>
#include <logging/Verify.h>

#include "RecordFormatStreamPlayer.h"
#include "RecordReaders.h"

using namespace std;

namespace vrs {

namespace {
// ContentBlocks may rely on the most recent configuration record to fully define the format of
// their content. When we detect that an image or audio block's format can't be fully defined, we
// log a warning to help debug the problem. Maybe the configuraion record exists, but wasn't read
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
  XR_LOGW(
      "Can't define the {} block format for {} to read this {} block with DataLayout. "
      "This might be happening, because the {} format is defined in a configuration record using "
      "DataLayoutConventions, but {} {} record.",
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
      reader = new DataLayoutBlockReader(recordFormat, blockIndex, move(blockLayout));
      break;

    case ContentType::COUNT:
      reader = new UnsupportedBlockReader(recordFormat, blockIndex);
      break;
  }
  return unique_ptr<ContentBlockReader>(reader);
}

ContentBlockReader::~ContentBlockReader() = default;

size_t ContentBlockReader::findContentBlockSize(
    const CurrentRecord& record,
    RecordFormatStreamPlayer& player) {
  uint32_t size32;
  // Have we successfuly mapped the content block size already?
  if (contentBlockSizeSpec_ && contentBlockSizeSpec_->isMapped() &&
      contentBlockSizeSpec_->nextContentBlockSize.get(size32)) {
    return size32;
  }
  // Try to find the size in the datalayout before this block, but only try once
  if (blockIndex_ > 0 && !contentBlockSizeSpec_) {
    contentBlockSizeSpec_ = make_unique<DataLayoutConventions::NextContentBlockSizeSpec>();
    const size_t index = blockIndex_ - 1;
    RecordFormatReader* reader = player.getCurrentRecordFormatReader();
    if (reader->recordFormat.getContentBlock(index).getContentType() == ContentType::DATA_LAYOUT) {
      if (mapToBlockLayout(reader, index, *contentBlockSizeSpec_) &&
          contentBlockSizeSpec_->nextContentBlockSize.get(size32)) {
        return size32;
      }
    }
  }
  // maybe we can compute the size of the block using the amount of data left to read...
  return recordFormat_.getBlockSize(blockIndex_, record.reader->getUnreadBytes());
}

bool AudioBlockReader::readAudioContentBlock(
    const CurrentRecord& record,
    RecordFormatStreamPlayer& player,
    const ContentBlock& contentBlock) {
  const AudioContentBlockSpec& audioContent = contentBlock.audio();
  size_t remainingBlockSize =
      recordFormat_.getBlockSize(blockIndex_, record.reader->getUnreadBytes());
  size_t sampleCount = audioContent.getSampleCount();
  if (sampleCount == 0) {
    if (remainingBlockSize != ContentBlock::kSizeUnknown) {
      if (audioContent.getAudioFormat() == AudioFormat::PCM) {
        // The sample count is undefined, but we can to do the math,
        // using the remaining bytes in the record.
        uint8_t sampleBlockStride = audioContent.getSampleBlockStride();
        if (sampleBlockStride > 0 && (remainingBlockSize % sampleBlockStride) == 0) {
          // update contentBlock with the actual sample count
          return player.onAudioRead(
              record,
              blockIndex_,
              ContentBlock(
                  audioContent.getSampleFormat(),
                  audioContent.getChannelCount(),
                  audioContent.getSampleRate(),
                  static_cast<uint32_t>(remainingBlockSize / sampleBlockStride),
                  audioContent.getSampleBlockStride()));
        }
      } else {
        return player.onAudioRead(
            record, blockIndex_, ContentBlock(contentBlock, remainingBlockSize));
      }
    }
  } else {
    size_t expectedSize = sampleCount * audioContent.getSampleBlockStride();
    // if we have redundant size information, validate our values
    if (remainingBlockSize == ContentBlock::kSizeUnknown || remainingBlockSize == expectedSize) {
      return player.onAudioRead(record, blockIndex_, contentBlock);
    }
    XR_LOGW(
        "Non-matching audio block size, got {} bytes, expected {} bytes.",
        remainingBlockSize,
        expectedSize);
  }
  return player.onUnsupportedBlock(record, blockIndex_, contentBlock);
}

bool AudioBlockReader::readBlock(const CurrentRecord& record, RecordFormatStreamPlayer& player) {
  const ContentBlock& contentBlock = recordFormat_.getContentBlock(blockIndex_);
  // check if we already have enough information to extract the audio data
  if (contentBlock.audio().isSampleBlockFormatDefined()) {
    return readAudioContentBlock(record, player, contentBlock);
  }
  // if that is not the case...
  bool readNextBlock = true;
  // can we successfully map our audio spec DataLayout? If so, use it.
  if (tryCurrentAudioSpec(record, player, readNextBlock)) {
    return readNextBlock;
  }
  // current audio spec does not fit the data, search last record that might have a valid audio spec
  RecordFormatReader* reader = player.getCurrentRecordFormatReader();
  if (blockIndex_ > 0 && findAudioSpec(record, player, reader, blockIndex_)) {
    return true;
  }
  // find previous configuration block and try to get the audio spec from there
  if (record.recordType != Record::Type::CONFIGURATION) {
    reader = player.getLastRecordFormatReader(record.streamId, Record::Type::CONFIGURATION);
    if (mayUsePastConfigurationReader(record, reader, contentBlock.getContentType()) &&
        findAudioSpec(record, player, reader, reader->recordFormat.getUsedBlocksCount())) {
      return true;
    }
  }
  // we tried everything...
  return player.onUnsupportedBlock(record, blockIndex_, contentBlock);
}

bool AudioBlockReader::findAudioSpec(
    const CurrentRecord& record,
    RecordFormatStreamPlayer& player,
    RecordFormatReader* reader,
    size_t countOfBlocksToSearch) {
  // look for last datalayout before this block
  size_t index = countOfBlocksToSearch;
  while (index-- > 0) {
    if (reader->recordFormat.getContentBlock(index).getContentType() == ContentType::DATA_LAYOUT) {
      if (mapToBlockLayout(reader, index, audioSpec_)) {
        bool readNextBlock = true;
        if (tryCurrentAudioSpec(record, player, readNextBlock)) {
          return readNextBlock;
        }
      }
    }
  }
  return false;
}

bool AudioBlockReader::audioContentFromAudioSpec(
    const DataLayoutConventions::AudioSpec& audioSpec,
    ContentBlock& audioContentBlock) const {
  AudioSampleFormat sampleFormat = AudioSampleFormat::UNDEFINED;
  uint8_t numChannels = 0;
  uint32_t sampleRate = 0;
  // checke minimal set of required fields
  if ((audioSpec.sampleType.get(sampleFormat) && sampleFormat > AudioSampleFormat::UNDEFINED &&
       sampleFormat < AudioSampleFormat::COUNT) &&
      (audioSpec.channelCount.get(numChannels) && numChannels > 0) &&
      (audioSpec.sampleRate.get(sampleRate) && sampleRate > 0)) {
    // everything looks fine, check optional fields
    uint8_t blockAlign = 0;
    uint32_t sampleCount = 0;
    uint32_t sampleSizeInBytes =
        (AudioContentBlockSpec::getBitsPerSample(sampleFormat) >> 3) * numChannels;
    // If blockAlign field is set, perform a sanity check based on the format. Assume that any
    // meaningful alignment of a sample won't be longer than additional 2 bytes per channel e.g. if
    // uint16_t samples is stored in uint32_t for some reason
    if (audioSpec.sampleStride.get(blockAlign) &&
        (blockAlign < sampleSizeInBytes || blockAlign > sampleSizeInBytes + numChannels * 2)) {
      // has invalid block align
      return false;
    }
    audioSpec.sampleCount.get(sampleCount);
    audioContentBlock =
        ContentBlock(sampleFormat, numChannels, sampleRate, sampleCount, blockAlign);
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
  if (audioContentFromAudioSpec(audioSpec_, contentBlock)) {
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
  if (imageFormat == ImageFormat::RAW || imageFormat == ImageFormat::VIDEO) {
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
      videoFrameSpec_ = make_unique<DataLayoutConventions::VideoFrameSpec>();
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
  DataLayout& layout = *blockLayout_;
  vector<int8_t>& fixedData = layout.getFixedData();
  fixedData.resize(layout.getFixedDataSizeNeeded());
  vector<int8_t>& varData = layout.getVarData();
  int error = record.reader->read(fixedData);
  if (error == 0) {
    size_t varLength = layout.getVarDataSizeFromIndex();
    varData.resize(varLength);
    if (varLength > 0) {
      error = record.reader->read(varData);
    }
  } else {
    varData.resize(0);
  }
  if (XR_VERIFY(error == 0)) {
    return player.onDataLayoutRead(record, blockIndex_, layout);
  }
  return false;
}

bool EmptyBlockReader::readBlock(const CurrentRecord&, RecordFormatStreamPlayer&) {
  return true; // just continue to the next block
}

} // namespace vrs
