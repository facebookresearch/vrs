// Copyright (c) Facebook, Inc. and its affiliates. Confidential and proprietary.

#pragma once

#include <atomic>
#include <functional>
#include <memory>
#include <set>
#include <thread>

#include "Compressor.h"
#include "DiskFile.h"
#include "FileFormat.h"
#include "IndexRecord.h"
#include "NewChunkHandler.h"
#include "Record.h"
#include "Recordable.h"
#include "WriteFileHandler.h"

namespace vrs {

namespace test {
struct RecordFileWriterTester;
} // namespace test

using std::function;
using std::map;
using std::multiset;
using std::pair;
using std::set;
using std::string;

class Record;
class Recordable;

/// Namespace for RecordFileWriter's private implementation classes.
namespace RecordFileWriter_ {
struct WriterThreadData;
struct PurgeThreadData;
struct CompressionThreadsData;
} // namespace RecordFileWriter_

/// Thread types that are created with the RecordFileWriter interface
enum class ThreadRole { Writer, Purge, Compression };

/// Callback type used to initialize the created threads
/// Arguments are currentThread, ThreadRole, threadIndex (Only used for Compression)
using InitCreatedThreadCallback = std::function<void(std::thread&, ThreadRole, int)>;

/// \brief The class to create VRS files.
///
/// There are different strategies to write a VRS file:
///
/// Write all the data of one or more recordables to a file synchronously in one shot:
///   - create a RecordFileWriter.
///   - add the (active) recordables you want to record using addRecordable().
///   - create all the records you want, as long as they fit in memory.
///   - call writeToFile() with a filepath.
///   - profit!
///
/// To write the data of one or more recordables, progressively, while records are being generated,
/// using a background thread:
///   - create a RecordFileWrite.
///   - add the (active) recordables you want to record using addRecordable().
///   - create the file using createFileAsync().This will create the file & write a few bytes,
///     but should be quite quick.
///   - optional: call purgeOldData() to discard records that were created before
///     what you really want to record.
///   - call writeRecordsAsync() regularly to write old enough records to disk in the background.
///     (very fast call)
///   - optional: call closeFileAsync() to write the remaining unwritten records, in the background.
///     (very fast call)
///   - call waitForFileClosed() to write all the unwritten records & wait for the file
///     to be written & closed. (blocking call).
///   - profit!
class RecordFileWriter {
 public:
  RecordFileWriter();
  RecordFileWriter(const RecordFileWriter&) = delete;
  RecordFileWriter& operator=(const RecordFileWriter&) = delete;
  virtual ~RecordFileWriter();

  /// A record file holds data from various recordables, registered using this method.
  /// The ownership of the recordable is not transferred, and the caller is responsible for deleting
  /// the recordables after the RecordFileWriter is deleted.
  /// @param recordable: Recordable object for the device to record.
  void addRecordable(Recordable* recordable);

  /// Get the recordables attached to this writer.
  /// @return A vector of Recordable pointers.
  vector<Recordable*> getRecordables() const;

  /// Set number of threads to use for background compression, or none will be used.
  /// @param size: Number of threads to compress records in parallel.
  /// The default value will make RecordFileWriter use as many threads as there are cores in the
  /// system. If you do not set any value, RecordFileWriter will use only a single thread.
  void setCompressionThreadPoolSize(size_t size = kMaxThreadPoolSizeForHW);
  static constexpr size_t kMaxThreadPoolSizeForHW = UINT32_MAX; // one-per-CPU core

  /// Sets a callback that will be called when a Thread is created by this interface.
  /// This provides the user the opportunity to set the threads priority, name, etc...
  /// The new thread will use the callback so functions like gettid() can be used.
  /// This is an optional call, but must be performed before calling createFileAsync().
  /// @param initCreatedThreadCallback Callback that returns a reference to the thread
  /// created, the ThreadRole, and the thread index (only used for Compression threads)
  void setInitCreatedThreadCallback(InitCreatedThreadCallback initCreatedThreadCallback);

  /// Take all the records of all the registered and *active* recordables,
  /// and write them all to disk. All synchronous, won't return until the file is closed.
  /// On error, a (partial) file may exist.
  /// @param filePath: Path relative or absolute where to write the VRS file.
  /// @return A status code: 0 if no error occurred, a file system error code otherwise.
  int writeToFile(const string& filePath);

  /// Delete all records older than a certain time (useful to trim a live buffer).
  /// @param maxTimestamp: timestamp cutoff for the records to purge; all purged records have been
  /// created before that maxTimestamp.
  /// @param recycleBuffer: Tell if the buffers should be recycled and the memory occupied by the
  /// records might not released immediately, or if the memory freed immediately.
  void purgeOldRecords(double maxTimestamp, bool recycleBuffers = true);

  /// Create a VRS file to write to in a background thread.
  /// @param filePath: Path relative or absolute of the VRS file to create.
  /// Note: the file's & recordables' tags will be written to disk before the call returns. Make
  /// sure to call this method only after you've added all your recordables and set all the tags.
  int createFileAsync(const string& filePath);

  /// Create a VRS file to write to in a background thread, with a separate head file that will
  /// contain the file's header, and the description and index records only. All the user records
  /// will be written in one or more following chunks, that will be only written going forward,
  /// which makes them streaming friendly. The file's head will always be written using a
  /// DiskFile, and will be using updates/overwrites, which is not compatible with streaming,
  /// unless you can upload the file's head at the end, and prepend it to the previously uploaded
  /// user record chunk(s). A VRS file created this way will be efficient to stream when reading
  /// records in timestamp order with no seek backward or forward.
  /// @param filePath: Path relative or absolute of the VRS file to create.
  /// The user record chunks will be named using the name provided, with "_1", "_2", etc as suffix.
  /// Note: the file's & recordables' tags will be written to disk before the call returns. Make
  /// sure to call this method only after you've added all your recordables and set all the tags.
  /// @param maxChunkSizeMB: max size of a chunk. Note that the last chunk may actually be larger.
  /// If maxChunkSizeMB is 0, at least two chunks will be created: one for the file's records, one
  /// for the file's header and index.
  /// @param chunkHandler: optional listener to be notified each time a chunk is complete, and when
  /// chunk handling should be completed (finalize uploads, maybe?).
  /// See NewChunkCallback's documentation for details.
  int createChunkedFile(
      const string& filePath,
      size_t maxChunkSizeMB = 0,
      unique_ptr<NewChunkHandler>&& chunkHandler = nullptr);

  /// Set the maximum chunk size, as a number of MB, 0 meaning no chunking (infinite limit).
  /// Must be called after calling createFileAsync(), if it wasn't set then.
  /// @param maxChunkSizeMB: Max number of MB by chunk.
  /// Actual chunks might be a bit smaller, to avoid splitting records.
  void setMaxChunkSizeMB(size_t maxChunkSizeMB);

  /// Pre-allocate space for an index similar to the one provided.
  /// Must be called *before* the file is created, but after all the recordables have been attached.
  /// Call this method just before creating the file using createFileAsync().
  /// The index should be as similar to the real thing as possible, as it will be used to guess the
  /// size of the actual index, compressed. This method is meant to be used for copy operations, so
  /// that a compressed index can be pre-allocated based on the index of the source file(s).
  /// @param preliminaryIndex: an index that resembles the index of the final file.
  /// @return 0 is the request is accepted.
  int preallocateIndex(unique_ptr<deque<IndexRecord::DiskRecordInfo>> preliminaryIndex);

  /// Send records older than the timestamp provided to be written to disk in a background thread.
  /// @param maxTimestamp: Largest timestamp of the records sent to be written to disk, i.e. all
  /// records to be written are older.
  /// @return A status code: 0 if no error occurred, a file system error code otherwise.
  int writeRecordsAsync(double maxTimestamp);

  /// To collect & write new records automatically after opening the file.
  /// @param maxTimestampProvider: Function providing the timestamp of the newest record to be sent
  /// to the background thread(s) writing records to disk. All records sent to disk are older.
  /// @param delay: Number of seconds between automated calls to send records for writing.
  /// @return A status code: 0 if no error occurred, a file system error code otherwise.
  int autoWriteRecordsAsync(function<double()> maxTimestampProvider, double delay);

  /// To purge old records automatically, when no file is being written.
  /// Note: while writing a VRS file asynchronously, record purging will automatically be disabled
  /// when the file is created, and re-enabled when the file is closed.
  /// @param maxTimestampProvider: Function providing the timestamp of the newest record to be
  /// purged.
  /// @param delay: Number of seconds between automated calls to purge records.
  /// @return A status code: 0 if no error occurred, a file system error code otherwise.
  int autoPurgeRecords(function<double()> maxTimestampProvider, double delay);

  /// Tell if a disk file is being written.
  /// @return True is a file is being written by this RecordFileWriter instance.
  bool isWriting() const {
    return file_->isOpened();
  }

  /// It can be useful/necessary to track how much buffer memory is used by the background threads,
  /// so as to report errors, stop recording, or simply wait that enough data has been processed.
  /// To avoid threading/cache invalidation costs, this feature needs to be enabled by calling
  /// this method before you start writing to disk.
  /// For race conditions, you need to call this before the background thread is active.
  /// To be safe, call it before creating the file.
  void trackBackgroundThreadQueueByteSize();
  /// Get how many record-bytes have been passed to the background thread, but not yet processed,
  /// which correlates with how much memory is being used by the queue.
  /// @return The number record bytes are waiting to be processed by the background thread.
  uint64_t getBackgroundThreadQueueByteSize();

  /// Request to close the file, when all data has been written, but don't wait for that.
  /// @return A status code: 0 if no error occurred, a file system error code otherwise.
  int closeFileAsync();

  /// Start writing all the pending records, and wait for the file to be written & closed.
  /// @return A status code: 0 if no error occurred, a file system error code otherwise.
  int waitForFileClosed();

  /// Set a tag value.
  /// Note: tags are written when the file is created! Changes made later will not be saved!
  /// Tags are written early, so that if the app crashes, or we run out of disk space, we don't
  /// loose them! An index can be rebuilt if it's missing in a truncated file, but
  /// tags need to be safe, or they're useless!
  /// @param tagName: The name of the tag.
  /// @param tagValue: The value of the tag. Can be any string value, including a json message.
  void setTag(const string& tagName, const string& tagValue);
  /// Add file tags in bulk.
  /// @param newTags: A map of string name/value pairs.
  void addTags(const map<string, string>& newTags);
  /// Get all the file tags at once.
  /// @return A map of all the tags associated with the file itself.
  const map<string, string>& getTags() const {
    return fileTags_;
  }

  /// To use a different type of WriteFileHandler to generate the file.
  /// WriteFileHandler uses DiskFile by default, but you can use a different implementation if you
  /// need to. You might want to stream the data to the cloud, or override DiskFile to tweak file
  /// creation and initialize the file objects differently.
  /// @param writeFileHandler: a specialized WriteFileHandler object.
  /// @return An error status, 0 meaning success.
  int setWriteFileHandler(unique_ptr<WriteFileHandler> writeFileHandler);

  /// Background threads implementation: do not call!
  /// @internal
  void backgroundWriterThreadActivity();
  /// Background threads implementation: do not call!
  /// @internal
  void backgroundPurgeThreadActivity();

  /// Helper class to sort records by time.
  /// @internal
  struct SortRecord {
    SortRecord(Record* record, StreamId streamId) : record(record), streamId(streamId) {}

    /// we are sorting records primarily by timestamp, but this order is a total order
    bool operator<(const SortRecord& rhs) const {
      return this->record->getTimestamp() < rhs.record->getTimestamp() ||
          (this->record->getTimestamp() <= rhs.record->getTimestamp() &&
           (this->streamId < rhs.streamId ||
            // Records have a unique creation order within a particular device
            (this->streamId == rhs.streamId &&
             this->record->getCreationOrder() < rhs.record->getCreationOrder())));
    }

    Record* record;
    StreamId streamId;
  };
  /// Batch of records collected at one point in time, for each recordable
  using RecordBatch = vector<pair<StreamId, list<Record*>>>;
  /// Series of record batches collected
  using RecordBatches = vector<unique_ptr<RecordBatch>>;
  /// List of records, sorted by time
  using SortedRecords = deque<SortRecord>;

 private:
  /// The implementation of internal methods & members should never be relied upon, and may change.
  uint64_t collectOldRecords(RecordBatch& batch, double maxTimestamp); ///< internal
  static uint64_t addRecordBatchesToSortedRecords(
      const RecordBatches& batch,
      SortedRecords& inOutSortedRecords); ///< internal
  int createFileAsync(const string& filePath, bool splitHead); ///< internal
  int createFile(const string& filePath, bool splitHead); ///< internal
  int writeRecords(SortedRecords& records, int lastError); ///< internal
  int writeRecordsSingleThread(SortedRecords& records, int lastError); ///< internal
  int writeRecordsMultiThread(
      RecordFileWriter_::CompressionThreadsData& compressionThreadsData,
      SortedRecords& records,
      int lastError); ///< internal
  int completeAndCloseFile(); ///< internal

  void backgroundWriteCollectedRecord(); ///< internal
  bool addRecordsReadyToWrite(SortedRecords& inOutRecordsToWrite); ///< internal

  /// set of recordables registered with this file
  set<Recordable*> recordables_;
  mutable std::mutex recordablesMutex_;

  // data members valid while a file is being worked on
  unique_ptr<WriteFileHandler> file_;
  uint64_t maxChunkSize_;
  unique_ptr<NewChunkHandler> newChunkHandler_;
  FileFormat::FileHeader fileHeader_;
  uint32_t lastRecordSize_;
  bool skipFinalizeIndexRecords_ = false; // for unit testing only!
  unique_ptr<deque<IndexRecord::DiskRecordInfo>> preliminaryIndex_;
  IndexRecord::Writer indexRecordWriter_;
  map<string, string> fileTags_;
  Compressor compressor_;
  size_t compressionThreadPoolSize_{};

  /// when a background thread is active
  RecordFileWriter_::WriterThreadData* writerThreadData_;
  std::unique_ptr<std::atomic<uint64_t>> queueByteSize_; // background thread's queue byte size

  /// when a purge thread is active
  RecordFileWriter_::PurgeThreadData* purgeThreadData_;

  InitCreatedThreadCallback initCreatedThreadCallback_;

  friend struct ::vrs::test::RecordFileWriterTester; // for tests ONLY
};

} // namespace vrs
