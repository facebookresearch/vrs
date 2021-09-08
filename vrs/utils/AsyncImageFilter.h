//  Facebook Technologies, LLC Proprietary and Confidential.

#pragma once

#include <deque>
#include <functional>

#include <vrs/Compressor.h>
#include <vrs/IndexRecord.h>
#include <vrs/StreamPlayer.h>
#include <vrs/gaia/UploadMetadata.h>

#include "CopyHelpers.h"
#include "FilteredVRSFileReader.h"

namespace vrs::utils {

struct PendingRecord {
  PendingRecord() = default;
  PendingRecord& operator=(PendingRecord&& pendingRecord) {
    recordChunks = move(pendingRecord.recordChunks);
    imageChunk = pendingRecord.imageChunk;
    writer = pendingRecord.writer;
    formatVersion = pendingRecord.formatVersion;
    return *this;
  }
  void set(
      deque<unique_ptr<ContentChunk>>&& _recordChunks,
      ContentBlockChunk* _imageChunk,
      Writer* _writer,
      uint32_t _formatVersion) {
    recordChunks = move(_recordChunks);
    imageChunk = _imageChunk;
    writer = _writer;
    formatVersion = _formatVersion;
  }
  void clear() {
    recordChunks.clear();
    imageChunk = nullptr;
    writer = nullptr;
    formatVersion = 0;
  }
  bool needsImageProcessing() const {
    return imageChunk != nullptr;
  }
  void setBuffer(vector<uint8_t>&& processedImage) {
    imageChunk->getBuffer() = move(processedImage);
    imageChunk = nullptr;
  }

  deque<unique_ptr<ContentChunk>> recordChunks;
  ContentBlockChunk* imageChunk;
  Writer* writer;
  uint32_t formatVersion;
};

class AsyncImageFilter {
 public:
  /// Create an async image filter
  AsyncImageFilter(FilteredVRSFileReader& filteredReader);

  /// Create the output file, possibly in a cloud location, and initialize the image filter.
  /// param outputFilePath: path of the output file.
  /// param uploadMetadata: (optional) upload parameters (for Gaia only)
  /// return A status code.
  int createOutputFile(
      const string& outputFilePath,
      std::unique_ptr<UploadMetadata>&& uploadMetadata = nullptr);

  /// Get an image to process, until there are no more.
  /// @param outRecordIndex: on success, set to the index of the record containing the image.
  /// @param outImageSpec: on success, set to the image spec of the image to process.
  /// @param outFamre: on success, set to the image buffer to process.
  /// return True if an image to process was found & the data returned.
  /// False when no more images to process can be found.
  bool getNextImage(
      size_t& outRecordIndex,
      ImageContentBlockSpec& outImageSpec,
      std::vector<uint8_t>& outFrame);
  /// Return an image returned by getNextImage() after the pixels have been processed as desired.
  /// @param recordIndex: index of the image returned.
  /// @param procssedImage: processed image data.
  /// Note that the image format must be the exact same as the original image.
  /// If the image format is raw, then the size of the buffer should be the same.
  /// If the image was encoded somehow (jpg? png? other?), then the size of the buffer may be
  /// changed, but the image spec (dimension & pixel format) probably should have been preserved.
  /// return True if the corresponding image was found.
  /// False, if the image is not known, or was already returned. Neither should happen...
  bool writeProcessedImage(size_t recordIndex, std::vector<uint8_t>&& processedImage);

  /// Helper method to find out about a particular record
  const IndexRecord::RecordInfo* getRecordInfo(size_t recordIndex) const;
  /// Helper method to know how many records have been retrieved, but not yet returned
  size_t getPendingCount() const {
    return pendingRecords_.size();
  }
  /// Access some rarely needed options. Make changes before creating the output file.
  /// @return CopyOption struct.
  CopyOptions& getCopyOptions() {
    return copyOptions_;
  }

  /// Processing is complete when getNextImage() return false and getPendingCount() return 0.
  /// The file can then be closed, which will send all remaining buffers to be written out,
  /// and the output file uploaded & closed.
  /// return True if everything worked out, and the output file was closed successfuly.
  int closeFile();

 protected:
  FilteredVRSFileReader& getFilteredReader() const {
    return filteredReader_;
  }
  vrs::RecordFileWriter& getWriter() {
    return throttledWriter_.getWriter();
  }
  void processChunkedRecord(
      Writer& writer,
      const CurrentRecord& hdr,
      deque<unique_ptr<ContentChunk>>& chunks,
      ContentBlockChunk* imageChunk);
  friend class AsyncRecordFilterCopier;

  CopyOptions copyOptions_{false};

 private:
  FilteredVRSFileReader& filteredReader_;
  ThrottledWriter throttledWriter_;
  unique_ptr<ThrottledFileHelper> fileHelper_;
  vector<unique_ptr<StreamPlayer>> copiers_;
  deque<const IndexRecord::RecordInfo*> records_;
  size_t nextRecordIndex_;
  PendingRecord pendingRecord_;
  map<size_t, PendingRecord> pendingRecords_;
};

} // namespace vrs::utils
