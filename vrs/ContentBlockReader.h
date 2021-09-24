// (c) Facebook, Inc. and its affiliates. Confidential and proprietary.

#pragma once

#include <memory>

#include "DataLayout.h"
#include "DataLayoutConventions.h"
#include "StreamPlayer.h"

namespace vrs {

using std::move;
using std::unique_ptr;

class ContentBlock;
class RecordFormatStreamPlayer;
struct RecordFormatReader;

/// Abstract class to handle the interpretation of a record format's content block.
/// Specialized versions of this class will handle specific types of content blocks.
/// @see RecordFormat
/// @see ContentBlock
/// @internal
class ContentBlockReader {
 public:
  /// Factory style constructor, which will determine what ContentBlockReader object needs to be
  /// created to handle the referenced block.
  /// @param recordFormat: RecordFormat of the record.
  /// @param blockIndex: Index of the block to build a ContentBlockReader for.
  /// @param blockLayout: optional DataLayout, if the block is a DataLayout block.
  /// @return An appropriate ContentBlockReader for the given block.
  static unique_ptr<ContentBlockReader>
  build(const RecordFormat& recordFormat, size_t blockIndex, unique_ptr<DataLayout>&& blockLayout);

  /// Virtual destructor, since this is an abstract class.
  virtual ~ContentBlockReader();

  /// Method called by RecordFormatStreamPlayer when a content block needs to be read.
  /// To be implemented by each specialized ContentBlockReader class, which can then interpret
  /// the data of the format it expects.
  virtual bool readBlock(const CurrentRecord&, RecordFormatStreamPlayer&) = 0;

 protected:
  ContentBlockReader(const RecordFormat& recordFormat, size_t blockIndex)
      : recordFormat_(recordFormat), blockIndex_(blockIndex) {}
  size_t findContentBlockSize(const CurrentRecord& record, RecordFormatStreamPlayer& player);

  const RecordFormat& recordFormat_;
  const size_t blockIndex_;
  unique_ptr<DataLayoutConventions::NextContentBlockSizeSpec> contentBlockSizeSpec_;
};

/// Specialized version of ContentBlockReader to handle a content block containing an image.
/// @internal
class ImageBlockReader : public ContentBlockReader {
 public:
  ImageBlockReader(const RecordFormat& recordFormat, size_t blockIndex)
      : ContentBlockReader(recordFormat, blockIndex) {}

  bool readBlock(const CurrentRecord&, RecordFormatStreamPlayer&) override;

 protected:
  bool
  onImageFound(const CurrentRecord& rec, RecordFormatStreamPlayer& player, const ContentBlock& cb);

  DataLayoutConventions::ImageSpec imageSpec_;
  unique_ptr<DataLayoutConventions::VideoFrameSpec> videoFrameSpec_;
};

/// Specialized version of ContentBlockReader to handle a content block containing audio data.
/// @internal
class AudioBlockReader : public ContentBlockReader {
 public:
  AudioBlockReader(const RecordFormat& recordFormat, size_t blockIndex)
      : ContentBlockReader(recordFormat, blockIndex) {}

  bool readBlock(const CurrentRecord&, RecordFormatStreamPlayer&) override;

 protected:
  bool readAudioContentBlock(const CurrentRecord&, RecordFormatStreamPlayer&, const ContentBlock&);
  bool audioContentFromAudioSpec(const DataLayoutConventions::AudioSpec&, ContentBlock&) const;
  bool findAudioSpec(
      const CurrentRecord&,
      RecordFormatStreamPlayer&,
      RecordFormatReader*,
      size_t countOfBlocksToSearch);
  bool tryCurrentAudioSpec(const CurrentRecord&, RecordFormatStreamPlayer&, bool& readNextBlock);

  DataLayoutConventions::AudioSpec audioSpec_;
};

/// Specialized version of ContentBlockReader to handle a content block containing custom data.
/// Custom data is data which format/content is not known to VRS.
/// @internal
class CustomBlockReader : public ContentBlockReader {
 public:
  CustomBlockReader(const RecordFormat& recordFormat, size_t blockIndex)
      : ContentBlockReader(recordFormat, blockIndex) {}

  bool readBlock(const CurrentRecord&, RecordFormatStreamPlayer&) override;
};

/// Specialized version of ContentBlockReader to handle data that could not be handled by another
/// better suited ContentBlockReader. It's the fallback handler.
/// @internal
class UnsupportedBlockReader : public ContentBlockReader {
 public:
  UnsupportedBlockReader(const RecordFormat& recordFormat, size_t blockIndex)
      : ContentBlockReader(recordFormat, blockIndex) {}

  bool readBlock(const CurrentRecord&, RecordFormatStreamPlayer&) override;
};

/// Specialized version of ContentBlockReader to handle an empty content block.
/// This can happen, when a variable size block is empty, or when a content block ends up being
/// a placeholder.
/// @internal
class EmptyBlockReader : public ContentBlockReader {
 public:
  EmptyBlockReader(const RecordFormat& recordFormat, size_t blockIndex)
      : ContentBlockReader(recordFormat, blockIndex) {}

  bool readBlock(const CurrentRecord&, RecordFormatStreamPlayer&) override;
};

/// Specialized version of ContentBlockReader to handle DataLayout blocks.
/// @internal
class DataLayoutBlockReader : public ContentBlockReader {
 public:
  DataLayoutBlockReader(
      const RecordFormat& recordFormat,
      size_t blockIndex,
      unique_ptr<DataLayout>&& blockLayout)
      : ContentBlockReader(recordFormat, blockIndex), blockLayout_{move(blockLayout)} {}

  bool readBlock(const CurrentRecord&, RecordFormatStreamPlayer&) override;

  /// Convenience methods to map a desired layout to the block's layout, if we have one.
  /// @param desiredLayout Layout to map to the block's layout.
  /// @return True when there was a block layout to map to, False otherwise.
  bool mapToBlockLayout(DataLayout& desiredLayout) {
    if (blockLayout_) {
      desiredLayout.mapLayout(*blockLayout_);
      return true;
    }
    return false;
  }

 private:
  unique_ptr<DataLayout> blockLayout_;
};

} // namespace vrs
