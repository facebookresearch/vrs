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

#pragma once

#include <deque>
#include <memory>

#include <vrs/Compressor.h>
#include <vrs/RecordFormatStreamPlayer.h>
#include <vrs/Recordable.h>
#include <vrs/os/Platform.h>

namespace vrs::utils {

using std::deque;
using std::unique_ptr;

// Default but customizable copy options tag overrider
struct TagOverrider {
  map<string, string> fileTags;
  map<StreamId, map<string, string>> streamTags;

  virtual ~TagOverrider() = default;
  virtual void overrideTags(RecordFileWriter& writer) const;
};

// Optional parameters for copy (or merge) operations, to override defaults
struct CopyOptions {
  CopyOptions(bool showProgress = true) : showProgress{showProgress} {}
  CopyOptions(const CopyOptions& rhs);

  // Compression preset of the output file. Use this method to set the user's explicit choice.
  void setCompressionPreset(CompressionPreset preset) {
    userCompressionPreset = preset;
  }
  // Compression preset of the output file to use when the user has not made an explicit choice.
  void setDefaultCompressionPreset(CompressionPreset preset) {
    defaultCompressionPreset = preset;
  }
  CompressionPreset getCompression() const {
    return userCompressionPreset == CompressionPreset::Undefined ? defaultCompressionPreset
                                                                 : userCompressionPreset;
  }
  // Get tag overrider. Use default implementation if not already specified.
  TagOverrider& getTagOverrider();
  // Size of the compression threads pool. Will be limited to HW concurency.
  unsigned compressionPoolSize = std::numeric_limits<unsigned>::max();
  // Printout text output to stdout, to monitor progress
  bool showProgress = true;
  // Grace timestamp-time window, records may be sent to write in the background thread
  double graceWindow = 0;
  // Format output as json, to be able to parse stdout
  bool jsonOutput = false;
  // To automatically chunk the output file, specify a max chunk size in MB. 0 means no chunking.
  size_t maxChunkSizeMB = 0;
  // For copy/merge operations: optional and customizable tag overrider
  unique_ptr<TagOverrider> tagOverrider;
  // For merge operations only: tell if streams with the same RecordableTypeId should be merged.
  bool mergeStreams = false;
  // Count the number of records copied. Set during the copy/merge operation.
  mutable uint32_t outRecordCopiedCount = 0;
  // Maybe: output URI if the destination's storage system decides where to write the file
  mutable string outUri;

 private:
  CompressionPreset userCompressionPreset = CompressionPreset::Undefined;
#if IS_MAC_PLATFORM() && defined(__aarch64__)
  CompressionPreset defaultCompressionPreset = CompressionPreset::ZstdMedium;
#else
  CompressionPreset defaultCompressionPreset = CompressionPreset::ZstdLight;
#endif
};

// Helper to write records, as given by the Copier class below.
class Writer : public Recordable {
 public:
  Writer(RecordableTypeId typeId, const string& flavor) : Recordable(typeId, flavor) {}

  const Record* createStateRecord() override;
  const Record* createConfigurationRecord() override;
  const Record* createRecord(const CurrentRecord& record, vector<int8_t>& data);
  const Record* createRecord(const CurrentRecord& record, DataSource& source);
  const Record*
  createRecord(double timestamp, Record::Type type, uint32_t formatVersion, DataSource& src);

  map<string, string>& getVRSTags() {
    return Recordable::getVRSTags();
  }
};

// Helper to copy a RecordFileReader's given stream's records, to a RecordFileWriter.
// Does all the hooking up to the read & written files, and copies the stream's tags.
// Each record read, of any kind, is simply passed through to the written file.
class Copier : public StreamPlayer {
 public:
  Copier(
      RecordFileReader& fileReader,
      RecordFileWriter& fileWriter,
      StreamId id,
      const CopyOptions& copyOptions);

  bool processRecordHeader(const CurrentRecord& record, DataReference& outDataRef) override;
  void processRecord(const CurrentRecord& record, uint32_t bytesWrittenCount) override;

  Writer& getWriter() {
    return writer_;
  }

 protected:
  Writer writer_;
  RecordFileWriter& fileWriter_;
  const CopyOptions& options_;
  vector<int8_t> rawRecordData_;
};

class ContentChunk {
 public:
  ContentChunk() = default;
  explicit ContentChunk(DataLayout& layout);
  explicit ContentChunk(size_t size) {
    buffer_.resize(size);
  }
  explicit ContentChunk(vector<uint8_t>&& buffer) : buffer_{buffer} {}
  virtual ~ContentChunk() = default;
  vector<uint8_t>& getBuffer() {
    return buffer_;
  }
  // maybe do some data processing before we write out the data.
  virtual size_t filterBuffer() {
    return buffer_.size();
  }
  void fillAndAdvanceBuffer(uint8_t*& buffer) const;

 private:
  vector<uint8_t> buffer_;
};

class ContentBlockChunk : public ContentChunk {
 public:
  ContentBlockChunk(const ContentBlock& contentBlock, const CurrentRecord& record);
  ContentBlockChunk(const ContentBlock& contentBlock, vector<uint8_t>&& buffer);

  const ContentBlock& getContentBlock() const {
    return contentBlock_;
  }

 protected:
  const ContentBlock contentBlock_;
};

class FilteredChunksSource : public DataSource {
 public:
  FilteredChunksSource(deque<unique_ptr<ContentChunk>>& chunks)
      : DataSource(getFilteredChunksSize(chunks)), chunks_{chunks} {}
  void copyTo(uint8_t* buffer) const override;

 private:
  static size_t getFilteredChunksSize(const deque<unique_ptr<ContentChunk>>& chunks);

  deque<unique_ptr<ContentChunk>>& chunks_;
};

// Helper to filter records of a stream while copying them. It's an advanced version of Copier,
// that provides hooks to decide if a particular record should be copied verbatim, or modified.
// RecordFilterCopier can handle any record that RecordFormatStreamPlayer can parse.
class RecordFilterCopier : public RecordFormatStreamPlayer {
 public:
  RecordFilterCopier(
      RecordFileReader& fileReader,
      RecordFileWriter& fileWriter,
      StreamId id,
      const CopyOptions& copyOptions)
      : RecordFilterCopier(fileReader, fileWriter, id, id.getTypeId(), copyOptions) {}

  RecordFilterCopier(
      RecordFileReader& fileReader,
      RecordFileWriter& fileWriter,
      StreamId id,
      RecordableTypeId copyRecordableTypeId,
      const CopyOptions& copyOptions);
  // Tell if this particular record should be copied verbatim, or edited.
  virtual bool shouldCopyVerbatim(const CurrentRecord& record) = 0;

  // Modify the output record's timestamp, record format version, or record type (rarely needed).
  virtual void doHeaderEdits(CurrentRecord& record) {}

  // Edit DataLayout blocks, if needed.
  // Use DataLayout's findDataPieceXXX methods to find the fields you want to edit,
  // so you can set or stage a different value.
  virtual void doDataLayoutEdits(const CurrentRecord& record, size_t blockIndex, DataLayout& dl) {}

  // Filter image blocks. If the filter is more than a simple pixel buffer modification,
  // in particular if a pixel format conversion and/or a resolution change are made,
  // make sure to make the corresponding changes in the datalayout that describes the image format.
  virtual void filterImage(
      const CurrentRecord& record,
      size_t blockIndex,
      const ContentBlock& imageBlock,
      vector<uint8_t>& pixels) {}

  // Filter audio blocks. If the filter is more than a simple audio samples buffer modification,
  // make sure to make the corresponding changes in the datalayout that describes the audio format.
  virtual void filterAudio(
      const CurrentRecord& record,
      size_t blockIndex,
      const ContentBlock& audioBlock,
      vector<uint8_t>& audioSamples) {}

  // Call if while processing a record, you decide that this record should not be copied.
  void skipRecord() {
    skipRecord_ = true;
  }

  // Called after all the content chunks have been received. By default, write-out new record.
  virtual void finishRecordProcessing(const CurrentRecord& record);

  // For advanced operations, like altering RecordFormat definitions
  Writer& getWriter() {
    return writer_;
  }

 protected:
  bool processRecordHeader(const CurrentRecord& record, DataReference& outDataReference) override;
  void processRecord(const CurrentRecord& record, uint32_t readSize) override;
  bool onDataLayoutRead(const CurrentRecord& record, size_t blockIndex, DataLayout&) override;
  bool onImageRead(const CurrentRecord& record, size_t blockIndex, const ContentBlock& cb) override;
  bool onAudioRead(const CurrentRecord& record, size_t blockIndex, const ContentBlock& cd) override;
  bool onUnsupportedBlock(const CurrentRecord& record, size_t idx, const ContentBlock& cb) override;
  /// after processing a datalayout, make sure it's written out in the record
  void pushDataLayout(DataLayout& dataLayout);

  Writer writer_;
  RecordFileWriter& fileWriter_;
  const CopyOptions& options_;
  bool copyVerbatim_{false};
  bool skipRecord_{false};
  deque<unique_ptr<ContentChunk>> chunks_;
  vector<int8_t> verbatimRecordData_;
};

} // namespace vrs::utils
