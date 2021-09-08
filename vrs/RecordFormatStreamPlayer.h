// Facebook Technologies, LLC Proprietary and Confidential.

#pragma once

#include <limits>
#include <map>

#include "ContentBlockReader.h"
#include "DataLayout.h"
#include "RecordFormat.h"
#include "StreamPlayer.h"

namespace vrs {

using std::map;
using std::string;
using std::unique_ptr;
using std::vector;

class DataLayoutBlockReader;
class ImageBlockReader;
class AudioBlockReader;

namespace DataLayoutConventions {
class VideoFrameSpec;
}

/// VRS internal data structure, to hold various objects needed to decode a specific RecordFormat.
/// @internal
struct RecordFormatReader {
  double lastReadRecordTimestamp = std::numeric_limits<double>::max();
  RecordFormat recordFormat;
  vector<unique_ptr<ContentBlockReader>> contentReaders;
  vector<unique_ptr<DataLayout>> expectedDataLayouts;
  vector<unique_ptr<DataLayout>> legacyDataLayouts;
};

/// Specialized StreamPlayer designed to handle records which format is managed by RecordFormat.
/// Each ContentBlock of the RecordFormat will be decoded by a specialed ContentBlockReader,
/// and a corresponding callback will be called for each type of ContentBlock.
/// When a record is read, each of its content blocks are decoded in order, until they're all read,
/// or an error occured (a callback returned false).
/// The block's index is passed in case you need to disambiguate successive blocks of the same type,
/// or want to know when a new block is started (blockIndex == 0).
class RecordFormatStreamPlayer : public StreamPlayer {
 public:
  /// Callback for DataLayout content blocks.
  /// @param record: Metadata associated with the record being read.
  /// @param blockIndex: Index of the content block being read.
  /// @param datalayout: DataLayout read.
  /// @return Return true if remaining record content blocks should be read.
  /// Return false, if the record should not be read entirely, for instance, if you only need to
  /// read some metadata stored in the first content block, but don't need to read & decode the rest
  /// of the record.
  virtual bool onDataLayoutRead(const CurrentRecord& r, size_t /* blockIndex */, DataLayout&) {
    return true; // we can go read the next block, if any, since we've read the data
  }

  /// Callback for image content blocks. The description of the image data is held in the content
  /// block object, but the image data has not been read yet.
  /// Query the contentBlock object to know the details about the image, in particular, the size of
  /// the data to read. Then you can allocate a buffer for the data to read, and use the record's
  /// reader object to read the data in your own buffer.
  /// @param record: Metadata associated with the record being read.
  /// @param blockIndex: Index of the content block being read.
  /// @param cb: ContentBlock describing the image data to be read.
  /// @return Return true if remaining record content blocks should be read.
  virtual bool onImageRead(const CurrentRecord& record, size_t blockIndex, const ContentBlock& cb) {
    return onUnsupportedBlock(record, blockIndex, cb);
  }

  /// Callback for audio content blocks. The description of the audio data is held in the content
  /// block object, but the audio data has not been read yet.
  /// Query the contentBlock object to know the details about the audio, in particular, the size of
  /// the data to read. Then you can allocate a buffer for the data to read, and use the record's
  /// reader object to read the data in your own buffer.
  /// @param record: Metadata associated with the record being read.
  /// @param blockIndex: Index of the content block being read.
  /// @param cb: ContentBlock describing the audio data to be read.
  /// @return Return true if remaining record content blocks should be read.
  virtual bool onAudioRead(const CurrentRecord& record, size_t blockIndex, const ContentBlock& cb) {
    return onUnsupportedBlock(record, blockIndex, cb);
  }

  /// Callback for custom content blocks. The description of the custom data is held in the content
  /// block object, but the data has not been read yet. Query the contentBlock object to find out
  /// the size of the data to read. Then you can allocate a buffer for the data to read, and use the
  /// record's reader object to read the data in your own buffer.
  /// @param rec: Metadata associated with the record being read.
  /// @param blkIdx: Index of the content block being read.
  /// @param cb: ContentBlock describing the data to be read.
  /// @return Return true if remaining record content blocks should be read.
  virtual bool onCustomBlockRead(const CurrentRecord& rec, size_t blkIdx, const ContentBlock& cb) {
    return onUnsupportedBlock(rec, blkIdx, cb);
  }

  /// Callback for unsupported/unrecognized content blocks.
  /// Also used the other callbacks aren't implemented.
  /// @param rec: Metadata associated with the record being read.
  /// @param blkIdx: Index of the block being read.
  /// @param cb: ContentBlock describing the data to be read.
  /// @return Return true if remaining record content blocks should be read.
  virtual bool onUnsupportedBlock(const CurrentRecord& rec, size_t blkIdx, const ContentBlock& cb);

  /// Callback called when the object is attached to a RecordFileReader object, so that this object
  /// can initialize itself. Do not prevent this initialization, or the record won't be read
  /// correctly.
  /// For RecordFormatStreamPlayer internal use.
  /// @param recordFileReader: Record file reader this stream player was just attached to.
  /// @param streamId: StreamId of the stream of records this stream player is associated with.
  /// @internal
  void onAttachedToFileReader(RecordFileReader& recordFileReader, StreamId streamId) override;

  /// Callback called when a record is read. For RecordFormatStreamPlayer internal use.
  /// @internal
  bool processRecordHeader(const CurrentRecord& record, DataReference& outDataReference) override;
  /// Callback called when a record is read. For RecordFormatStreamPlayer internal use.
  /// @internal
  void processRecord(const CurrentRecord& record, uint32_t readSize) override;

  /// For RecordFormatStreamPlayer internal use.
  /// @internal
  RecordFormatReader* getLastRecordFormatReader(StreamId id, Record::Type recordType) const;

  /// For RecordFormatStreamPlayer internal use.
  /// @internal
  RecordFormatReader* getCurrentRecordFormatReader() const {
    return currentReader_;
  }

 protected:
  // Helper class, to be used exclusively during onXXXRead() callbacks,
  // to get the wished for DataLayout
  template <class T>
  inline T& getExpectedLayout(DataLayout& layout, size_t blockIndex) {
    return getCachedLayout<T>(currentReader_->expectedDataLayouts, layout, blockIndex);
  }
  // Helper class, to be used exclusively during onXXXRead() callbacks,
  // to get legacy fields no longer present in the official layout, for backward compatibility needs
  template <class T>
  inline T& getLegacyLayout(DataLayout& layout, size_t blockIndex) {
    return getCachedLayout<T>(currentReader_->legacyDataLayouts, layout, blockIndex);
  }
  template <class T>
  T& getCachedLayout(
      vector<unique_ptr<DataLayout>>& layoutCache,
      DataLayout& layout,
      size_t blockIndex) {
    if (layoutCache.size() <= blockIndex) {
      layoutCache.resize(blockIndex + 1);
    }
    if (!layoutCache[blockIndex]) {
      T* expectedLayout = new T;
      layoutCache[blockIndex].reset(expectedLayout);
      expectedLayout->mapLayout(layout);
    }
    return *reinterpret_cast<T*>(layoutCache[blockIndex].get());
  }

  RecordFileReader* recordFileReader_{};

  // Keep the readers all separate,
  // in case the same RecordFormatStreamPlayer is handling mulitple streams.
  map<tuple<StreamId, Record::Type, uint32_t>, RecordFormatReader> readers_;
  map<pair<StreamId, Record::Type>, RecordFormatReader*> lastReader_;
  RecordFormatReader* currentReader_{};
};

} // namespace vrs
