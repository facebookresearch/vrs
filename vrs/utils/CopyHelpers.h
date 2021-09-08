// Facebook Technologies, LLC Proprietary and Confidential.

#pragma once

#include <list>

#include <vrs/Compressor.h>
#include <vrs/DataSource.h>
#include <vrs/RecordFileReader.h>
#include <vrs/RecordFileWriter.h>
#include <vrs/RecordFormatStreamPlayer.h>
#include <vrs/Recordable.h>
#include <vrs/StreamPlayer.h>
#include <vrs/gaia/GaiaUploader.h>
#include <vrs/gaia/UploadMetadata.h>

#include "FilteredVRSFileReader.h"

namespace vrs::utils {

extern const size_t kDownloadChunkSize;
extern const char* const kResetCurrentLine;

constexpr const char* kGaiaIdResult = "gaia_id";
constexpr const char* kLocalPathResult = "local_path";

// Optional parameters for copy (or merge) operations, to override defaults
struct CopyOptions {
  CopyOptions(bool showProgress = true) : showProgress{showProgress} {}

  // Compression preset of the output file. Use this method to set the user's explicit choice.
  void setCompressionPreset(CompressionPreset preset) {
    userCompressionPreset = preset;
  }
  // Compression preset of the output file to use when the user has not made an explicit choice.
  void setDefaultCompressionPreset(CompressionPreset preset) {
    defaultCompressionPreset = preset;
  }
  CompressionPreset getCompression() const {
    return userCompressionPreset == vrs::CompressionPreset::Undefined ? defaultCompressionPreset
                                                                      : userCompressionPreset;
  }
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
  // For merge operations only: tell if streams with the same RecordableTypeId should be merged.
  bool mergeStreams = false;
  // Count the number of records copied. Set during the copy/merge operation.
  mutable uint32_t outRecordCopiedCount = 0;
  // Maybe: id of resulting Gaia object
  mutable GaiaId outGaiaId = 0;

 private:
  vrs::CompressionPreset userCompressionPreset = vrs::CompressionPreset::Undefined;
  vrs::CompressionPreset defaultCompressionPreset = vrs::CompressionPreset::ZstdLight;
};

// Helper to write records, as given by the Copier class below.
class Writer : public vrs::Recordable {
 public:
  Writer(vrs::RecordableTypeId typeId, const string& flavor) : Recordable(typeId, flavor) {}

  const vrs::Record* createStateRecord();
  const vrs::Record* createConfigurationRecord();
  const vrs::Record* createRecord(const vrs::CurrentRecord& record, std::vector<int8_t>& data);
  const vrs::Record* createRecord(const vrs::CurrentRecord& record, vrs::DataSource& source);
  const vrs::Record*
  createRecord(double timestamp, Record::Type type, uint32_t formatVersion, vrs::DataSource& src);
};

// Helper to copy a RecordFileReader's given stream's records, to a RecordFileWriter.
// Does all the hooking up to the read & written files, and copies the stream's tags.
// Each record read, of any kind, is simply passed through to the written file.
class Copier : public vrs::StreamPlayer {
 public:
  Copier(
      vrs::RecordFileReader& fileReader,
      vrs::RecordFileWriter& fileWriter,
      vrs::StreamId id,
      const CopyOptions& copyOptions);

  bool processRecordHeader(const CurrentRecord& record, vrs::DataReference& outDataRef) override;
  void processRecord(const CurrentRecord& record, uint32_t bytesWrittenCount) override;

  Writer& getWriter() {
    return writer_;
  }

 protected:
  Writer writer_;
  vrs::RecordFileWriter& fileWriter_;
  const CopyOptions& options_;
  std::vector<int8_t> rawRecordData_;
};

class ContentChunk {
 public:
  ContentChunk() {}
  ContentChunk(DataLayout& layout) {
    buffer_.resize(layout.getFixedData().size() + layout.getVarData().size());
    uint8_t* buffer = buffer_.data();
    DataSourceChunk fixedSizeChunk(layout.getFixedData());
    fixedSizeChunk.fillAndAdvanceBuffer(buffer);
    DataSourceChunk varSizeChunk(layout.getVarData());
    varSizeChunk.fillAndAdvanceBuffer(buffer);
  }
  ContentChunk(size_t size) {
    buffer_.resize(size);
  }
  ContentChunk(vector<uint8_t>&& buffer) : buffer_{buffer} {}
  virtual ~ContentChunk() = default;
  vector<uint8_t>& getBuffer() {
    return buffer_;
  }
  // maybe do some data processing before we write out the data.
  virtual size_t filterBuffer() {
    return buffer_.size();
  }
  void fillAndAdvanceBuffer(uint8_t*& buffer) const {
    DataSourceChunk dataSourceChunk(buffer_);
    dataSourceChunk.fillAndAdvanceBuffer(buffer);
  }

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
  void copyTo(uint8_t* buffer) const override {
    for (auto& chunk : chunks_) {
      chunk->fillAndAdvanceBuffer(buffer);
    }
  }

 private:
  static size_t getFilteredChunksSize(const deque<unique_ptr<ContentChunk>>& chunks) {
    size_t total = 0;
    for (auto& chunk : chunks) {
      total += chunk->filterBuffer();
    }
    return total;
  }

  deque<unique_ptr<ContentChunk>>& chunks_;
};

// Helper to filter records of a stream while copying them. It's an advanced version of vrs::Copier,
// that provides hooks to decide if a particular record should be copied verbatim, or modified.
// RecordFilterCopier can handle any record that RecordFormatStreamPlayer can parse.
class RecordFilterCopier : public vrs::RecordFormatStreamPlayer {
 public:
  RecordFilterCopier(
      vrs::RecordFileReader& fileReader,
      vrs::RecordFileWriter& fileWriter,
      vrs::StreamId id,
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

 protected:
  bool processRecordHeader(const CurrentRecord& record, DataReference& outDataReference) override;
  void processRecord(const CurrentRecord& record, uint32_t readSize) override;
  bool onDataLayoutRead(const CurrentRecord& record, size_t blockIndex, DataLayout&) override;
  bool onImageRead(const CurrentRecord& record, size_t blockIndex, const ContentBlock& cb) override;
  bool onAudioRead(const CurrentRecord& record, size_t blockIndex, const ContentBlock& cd) override;
  bool onUnsupportedBlock(const CurrentRecord& record, size_t idx, const ContentBlock& cb) override;
  /// after processing a datalayout, make sure it's written out in the record
  void pushDataLayout(DataLayout& dataLayout);
  Writer& getWriter() {
    return writer_;
  }

  Writer writer_;
  vrs::RecordFileWriter& fileWriter_;
  const CopyOptions& options_;
  bool copyVerbatim_;
  bool skipRecord_;
  deque<unique_ptr<ContentChunk>> chunks_;
  std::vector<int8_t> verbatimRecordData_;
};

/// Class to control memory usage while writing out to a VRS file
/// using a RecordFileWriter object.
class ThrottledWriter {
 public:
  ThrottledWriter(const CopyOptions& options);

  /// Init writer with latest copy options values (if they were changed since constructor)
  void initWriter();

  /// Get a reference to the RecordFileWriter object which progress is being monitored.
  /// @return The RecordFileWriter used to write the output file.
  vrs::RecordFileWriter& getWriter();

  /// Set the range of timestamps expected, to track progress on the time range.
  /// @param minTimestamp: earliest timestamp of the operation
  /// @param maxTimestamp: latest timestamp of the operation
  void initTimeRange(double minTimestamp, double maxTimestamp);

  /// Called when a record is read, which can allow you to slow down decoding by adding a mere sleep
  /// in the callback itself. This is the main use case of this callback, as data is queued for
  /// processing & writing to Disk in a different thread, and we could run out of memory if we don't
  /// allow the background thread to run further, while we slow down the decoding thread.
  /// Can also be used to build a UI (GUI or terminal) to monitor progress when copying large files.
  /// @param timestamp: Timestamp of the record decoded.
  /// @param writeGraceWindow: Grace window in which records are allowed to be out-of-order.
  void onRecordDecoded(double timestamp, double writeGraceWindow = 0.0);

  /// Called when we're ready to close the file. On exit, it is expected that the writer is closed.
  int closeFile();

  void waitForBackgroundThreadQueueSize(size_t maxSize);

  void printPercentAndQueueSize(uint64_t queueByteSize, bool waiting);

  void addWaitCondition(function<bool()> waitCondition) {
    waitCondition_ = waitCondition;
  }

  bool showProgress() const {
    return copyOptions_.showProgress;
  }

 private:
  vrs::RecordFileWriter writer_;
  function<bool()> waitCondition_;
  const CopyOptions& copyOptions_;
  double nextUpdateTime_ = 0;
  int32_t percent_ = 0;
  double minTimestamp_ = 0;
  double duration_ = 0;
};

/// Helper class to handle creating & close local files or uploads
class ThrottledFileHelper {
 public:
  ThrottledFileHelper(ThrottledWriter& throttledWriter)
      : throttledWriter_{throttledWriter}, uploader_{}, uploadId_(0), gaiaId_(0) {}

  /// Create a VRS file, and maybe set it up for upload-streaming to Gaia,
  /// if uploadMetadata is provided.
  /// Either way, the file is created for async writes.
  int createFile(const string& pathToCopy, unique_ptr<UploadMetadata>& uploadMetadata);
  /// Close the file, wait that all the data is written out, and uploaded to Gaia (if needed).
  int closeFile();
  /// Retrieve the resulting GaiaId.
  GaiaId getGaiaId() const {
    return gaiaId_;
  }

 private:
  ThrottledWriter& throttledWriter_;
  unique_ptr<GaiaUploader> uploader_;
  UploadId uploadId_ = 0;
  GaiaId gaiaId_ = 0;
};

struct SourceRecord {
  vrs::RecordFileReader* reader;
  const vrs::IndexRecord::RecordInfo* record;

  bool operator<(const SourceRecord& rhs) const {
    return *record < *rhs.record;
  }
};

string jsonResult(
    int status,
    const string& failureMessage,
    const map<string, string>& successFields = {},
    GaiaId gaiaId = 0);
int printJsonResult(
    int status,
    const string& failureMessage,
    const map<string, string>& successFields = {},
    GaiaId gaiaId = 0);

void printProgress(const char* status, size_t currentSize, size_t totalSize, bool showProgress);

int verbatimInMemoryDownload(GaiaIdFileVersion idv, std::ostream& output, bool showProgress);

/// Download a Gaia file at a given location.
/// idv: Gaia object to download.
/// downloadLocation: path to an existing folder where to download the file using its Gaia name,
/// or path to a new file in an existing folder.
/// showProgress: show download progress as percentage in cout, for cmd line operation usage.
/// jsonOuput: if set, will print a one line json message to track the result of the operation.
int verbatimDownload(
    GaiaIdFileVersion idv,
    const string& downloadLocation,
    bool showProgress,
    bool jsonOutput = false);
int verbatimUpload(
    const string& path,
    unique_ptr<UploadMetadata>& metadata,
    GaiaId& outGaiaId,
    bool showProgress,
    bool jsonOutput = false);
int verbatimUpdate(
    GaiaId gaiaId,
    FilteredVRSFileReader& source,
    bool showProgress,
    bool jsonOutput = false);

int cacheDownload(GaiaIdFileVersion idv, bool showProgress, bool jsonOutput = false);
void uncacheDownload(GaiaIdFileVersion idv);

}; // namespace vrs::utils
