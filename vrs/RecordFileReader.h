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

#include <array>
#include <functional>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <thread>
#include <vector>

#include "FileFormat.h"
#include "FileHandler.h"
#include "IndexRecord.h"
#include "ProgressLogger.h"
#include "Record.h"
#include "RecordFormat.h"
#include "RecordReaders.h"
#include "Recordable.h"
#include "StreamId.h"

namespace vrs {

using std::map;
using std::set;
using std::string;
using std::vector;

class DataLayout;
class StreamPlayer;

/// This is a special boolean extra field in FileSpec to make RecordFileReader fail fast
/// on open if the file's index is incomplete or missing, and prevent VRS from rebuilding the index.
/// This is useful when accessing large files in cloud, when we'd rather fail than rebuild the
/// index.
constexpr const char* kFailFastOnIncompleteIndex = "fail_fast_on_incomplete_index";

/// \brief The class to read VRS files.
///
/// Recipe:
/// - open a VRS file using openFile().
/// - get info about the VRS file using getTags().
/// (optional: the writer may provide recording context info).
/// - find out which streams it contains using getStreams().
/// - attach record players to the streams you care about, using setStreamPlayer().
/// - playback records one-by-one using readRecord(), or all at once using readAllRecords().
/// - close the file.
/// State, configuration & data records handling is delegated to their stream id players.
/// Player can 'process' the data in the callback made by the file reader's thread,
/// or delegate the work by posting a PlaybackRecord to a PlaybackThread.
class RecordFileReader {
 public:
  RecordFileReader();
  RecordFileReader(const RecordFileReader&) = delete;
  RecordFileReader& operator=(const RecordFileReader&) = delete;

  virtual ~RecordFileReader();

  /// Checks if a file is most probably a VRS file by checking its header for VRS file's format
  /// magic numbers.
  /// Note: will reset the object if needed.
  /// @param filePath: Absolute or relative path of the file to check.
  /// @return True if the file is probably a VRS file, false otherwise (or if no file is found).
  bool isVrsFile(const string& filePath);
  /// Checks if a file is most probably a VRS file by checking its header for VRS file's format
  /// magic numbers.
  /// Note: will reset the object if needed.
  /// @param fileSpec: File spec of the local or remote file to check.
  /// @return True if the file is probably a VRS file, false otherwise (or if no file is found).
  bool isVrsFile(const FileSpec& fileSpec);

  /// Open a record file.
  /// Use one RecordFileReader object per file you want to read.
  /// @param filePath: Identifier of resource to open. Might be a file path, a URI,
  /// or a "JSON path". See FileHandlerFactory::delegateOpen for details.
  /// @param autoWriteFixedIndex: If the index was rebuilt, patch the original file in place.
  /// @return 0 on success and you can read the file, or some non-zero error code, in which case,
  /// further read calls will fail.
  int openFile(const string& filePath, bool autoWriteFixedIndex = false);
  /// Open a record file.
  /// Use one RecordFileReader object per file you want to read.
  /// @param fileSpec: File spec of the local or remote file to check.
  /// @param autoWriteFixedIndex: If the index was rebuilt, patch the original file in place.
  /// @return 0 on success and you can read the file, or some non-zero error code, in which case,
  /// further read calls will fail.
  int openFile(const FileSpec& fileSpec, bool autoWriteFixedIndex = false);

  /// Tell if an actual file is being read
  /// @return True if the file is opened.
  bool isOpened() const;

  /// Hook a stream player to a specific stream after opening a file and before reading records.
  /// The file player does *not* take ownership of the StreamPlayer.
  /// Using the same StreamPlayer instance for multiple streams is supported.
  /// So the caller is responsible for deleting the StreamPlayer objects after the file is read.
  /// Disconnect the StreamPlayer by passing a nullptr for the stream id.
  /// @param streamId: StreamId to hook the stream player to.
  /// @param streamPlayer: StreamPlayer to attach.
  void setStreamPlayer(StreamId streamId, StreamPlayer* streamPlayer);

  /// Remove all registered stream players.
  /// @return O on success.
  int clearStreamPlayers();

  /// When streaming a VRS file from the cloud, it may be very beneficial to tell before hand which
  /// records will be read, in order, so that the data can be prefetched optimally.
  /// Note the only some FileHandlers implement this, others will just ignore the request, which is
  /// always safe to make.
  /// @param records: a sequence of records in the exact order they will be read. It's ok to
  /// skip one or more records, but:
  /// - don't try to read "past" records, or you'll confuse the caching strategy, possibly leading
  /// to much worse performance.
  /// - if you read a single record out of the sequence, the prefetch list will be cleared.
  /// You may call this method as often as you like, and any previous read sequence will be cleared,
  /// but whatever is already in the cache will remain.
  /// @param clearSequence: Flag on whether to cancel any pre-existing custom read sequence upon
  /// caching starts.
  /// @return True if the file handler backend supports this request, false if it was ignored.
  bool prefetchRecordSequence(
      const vector<const IndexRecord::RecordInfo*>& records,
      bool clearSequence = true);

  /// Check if a file record is available for immediate loading (e.g. on disk or in-cache).
  /// If not, begin background prefetching at the requested frame (but do not wait for results).
  /// @param recordInfo: RecordInfo reference of the record to read.
  /// @return true if available, false if unavailable (e.g. would require a network fetch).
  bool isRecordAvailableOrPrefetch(const IndexRecord::RecordInfo& recordInfo);

  /// Read a file's record. Preferred way to read records.
  /// @param recordInfo: RecordInfo reference of the record to read.
  /// @return 0 on success, or a non-zero error code.
  /// If there is no StreamPlayer hooked up for the stream, no read operation is done and 0 is
  /// returned.
  int readRecord(const IndexRecord::RecordInfo& recordInfo);

  /// Internal: Read a record using a specific stream player.
  /// Prefer using setStreamPlayer and the single parameter version of readRecord().
  /// Avoid calling directly, as it won't work as expected unless the player was properly setup
  /// for decoding the stream before.
  /// @param recordInfo: RecordInfo reference of the record to read.
  /// @param streamPlayer: StreamPlayer object to use for this operation only. The default stream
  /// player that might have been setup using setStreamPlayer() will not receive any callback.
  /// @return 0 on success or if streamPlayer is null, or a non-zero error code.
  int readRecord(const IndexRecord::RecordInfo& recordInfo, StreamPlayer* streamPlayer);

  /// Read all the records of an open file. Call closeFile() when you're done.
  /// Guaranties that records are read sorted by timestamp.
  /// @return 0 if all records were read successfully, some other value otherwise.
  int readAllRecords();

  /// Get a list of the file's chunks, path + size in bytes.
  /// @return A vector of pairs path-file size.
  vector<std::pair<string, int64_t>> getFileChunks() const;

  /// Get the size of the whole file.
  /// @return The number of bytes of all the chunks combined.
  int64_t getTotalSourceSize() const;

  /// Close the underlying file, if one is open.
  /// @return 0 on success or if no file was open, or some file system error code.
  int closeFile();

  /// VRS might not have a valid index, for instance, if the format is too old, or
  /// if there was a crash while the file was being produced.
  /// If there is no index, one will be created when the file is opened, but it will cost time.
  /// Requirements: the file must be opened for this method to be valid.
  /// @return True if the actual file has an index. Either way, we always have an in-memory index.
  bool hasIndex() const;

  /// Get the set of StreamId for all the streams in the file.
  /// @return The set of stream IDs for which there are records.
  const set<StreamId>& getStreams() const {
    return streamIds_;
  }

  /// Get a set of StreamId for a specific type, and an optional flavor.
  /// @param typeId: a recordable type id, maybe a Recordable Class.
  /// Use RecordableTypeId::Undefined to match any recordable type.
  /// @param flavor: an option flavor.
  /// @return A vector of stream ids of the given type, and of the provided flavor (if any).
  vector<StreamId> getStreams(RecordableTypeId typeId, const string& flavor = {}) const;

  /// Find a stream for a specific device type, by index number.
  /// Use isValid() to tell if a device was found.
  /// @param typeId: The RecordableTypeId of the type of device to look for.
  /// @param indexNumber: An index.
  /// @return A StreamId. Call isValid() to know if a matching StreamId was actually found.
  StreamId getStreamForType(RecordableTypeId typeId, uint32_t indexNumber = 0) const;

  /// Find a stream from an absolute or relative numeric name.
  /// Absolute numeric names are in the form <numeric_recordable_type_id>-<instance_id>, eg 1201-1
  /// Relative numeric names are in the form <numeric_recordable_type_id>+<instance_id>, eg 1201+1
  /// Relative numeric names have instance ids interpreted as the nth stream of that type, eg
  /// 1201+3 is the 3rd stream with the recordable type id 1201 (if there is such a stream).
  /// In all cases, use isValid() to verify that the stream was found in the file.
  /// @param name: an absolute or relative numeric name
  /// @return a StreamId, valid only if the numeric name exists in the file.
  StreamId getStreamForName(const string& name) const;

  /// Find a stream of a specific flavor, by index number.
  /// Use isValid() to tell if a device was found.
  /// @param typeId: The RecordableTypeId of the type of device to look for.
  /// @param flavor: A recordable flavor to search for.
  /// @param indexNumber: An index.
  /// @return A StreamId. Call isValid() to know if a matching StreamId was actually found.
  StreamId
  getStreamForFlavor(RecordableTypeId typeId, const string& flavor, uint32_t indexNumber = 0) const;

  /// Find the first stream with given tag name/value pair.
  /// @param tagName: The name of the tag to look for.
  /// @param tag: The tag value to look for.
  /// @param typeId: The RecordableTypeId to limit the search to, or RecordableTypeId::Undefined to
  /// look for any device type. Note: if more than one stream match the criteria, the "first" one is
  /// returned, which means the one with the lowest RecordableTypeId enum value, or if equal, the
  /// one with the lowest StreamId instanceId.
  /// @return A StreamId. Call isValid() to know if a matching StreamId was actually found.
  StreamId getStreamForTag(
      const string& tagName,
      const string& tag,
      RecordableTypeId typeId = RecordableTypeId::Undefined) const;

  /// Find the stream with the specified stream serial number.
  StreamId getStreamForSerialNumber(const string& streamSerialNumber) const;

  /// Get the index of the VRS file, which is an ordered array of RecordInfo, each describing
  /// the records, sorted by timestamp.
  /// @return The index.
  const vector<IndexRecord::RecordInfo>& getIndex() const {
    return recordIndex_;
  }

  /// Get a record index limited to a specific stream.
  /// @param streamId: StreamId of the record stream to consider.
  /// @return The index of the file, with all the records.
  const vector<const IndexRecord::RecordInfo*>& getIndex(StreamId streamId) const;

  /// Get the number of records in the whole file.
  /// @return The number of records in the whole VRS file, or 0, if no file is opened.
  uint32_t getRecordCount() const {
    return static_cast<uint32_t>(recordIndex_.size());
  }

  /// Get the number of records of a specific stream.
  /// @param streamId: StreamId of the record stream to consider.
  /// @return The number of records of the specified stream.
  uint32_t getRecordCount(StreamId streamId) const;

  /// Get the number of records for a specific stream and specific record type.
  /// Attention: this computation has a linear complexity, so cache the result!
  /// @param streamId: StreamId of the record stream to consider.
  /// @param recordType: Type of records to count.
  /// @return Number of records for the specified stream id & record type.
  uint32_t getRecordCount(StreamId streamId, Record::Type recordType) const;

  /// Find a specific record, regardless of its stream or type, by its absolute index number in the
  /// file.
  /// @param globalIndex: Index of the record to look for.
  /// @return Pointer to the record info, or nullptr the index exceeds the total number of records.
  const IndexRecord::RecordInfo* getRecord(uint32_t globalIndex) const;

  /// Find a specific record for a specific stream, regardless of type, by index number.
  /// @param streamId: StreamId of the record stream to consider.
  /// @param indexNumber: Index of the record to look for.
  /// @return Pointer to the record info, or nullptr if the streamId,
  /// or a record with the indexNumber wasn't found.
  const IndexRecord::RecordInfo* getRecord(StreamId streamId, uint32_t indexNumber) const;

  /// Find a specific record for a specific stream and type, by index number.
  /// nullptr is returned if no record is found for that index number.
  /// @param streamId: StreamId of the record stream to consider.
  /// @param recordType: Type of the records to consider.
  /// @param indexNumber: Index of the record to look for.
  /// @return Pointer to the record info,
  /// or nullptr if the streamId or the indexNumber wasn't found for that specific type.
  const IndexRecord::RecordInfo*
  getRecord(StreamId streamId, Record::Type recordType, uint32_t indexNumber) const;

  /// Find the last record for a specific stream and specific type.
  /// @param streamId: StreamId of the record stream to consider.
  /// @param recordType: Type of the records to consider.
  /// @return Pointer to the record info,
  /// or nullptr if the streamId or no record of the type was found.
  const IndexRecord::RecordInfo* getLastRecord(StreamId streamId, Record::Type recordType) const;

  /// Find the first record at or after a timestamp.
  /// @param timestamp: timestamp to seek.
  /// @return Pointer to the record info, or nullptr (timestamp is too big?).
  const IndexRecord::RecordInfo* getRecordByTime(double timestamp) const;
  /// Find the first record of a specific type at or after a timestamp.
  /// @param recordtype: record type to find.
  /// @param timestamp: timestamp to seek.
  /// @return Pointer to the record info, or nullptr (timestamp is too big?).
  const IndexRecord::RecordInfo* getRecordByTime(Record::Type recordType, double timestamp) const;
  /// Find the first record of a specific stream at or after a timestamp.
  /// @param streamId: StreamId of the stream to consider.
  /// @param timestamp: timestamp to seek.
  /// @return Pointer to the record info, or nullptr (timestamp is too big?).
  const IndexRecord::RecordInfo* getRecordByTime(StreamId streamId, double timestamp) const;
  /// Find the first record of a specific stream of a specific type at or after a timestamp.
  /// @param streamId: StreamId of the stream to consider.
  /// @param recordtype: record type to find.
  /// @param timestamp: timestamp to seek.
  /// @return Pointer to the record info, or nullptr (timestamp is too big?).
  const IndexRecord::RecordInfo*
  getRecordByTime(StreamId streamId, Record::Type recordType, double timestamp) const;

  /// Find the nearest record of a specific stream within
  /// the range of (timestamp - epsilon) - (timestamp + epsilon).
  /// @param timestamp: timestamp to seek.
  /// @param epsilon: the threshold we search for the index.
  /// @param streamId: StreamId of the stream to consider. Leave undefined to search all streams
  /// @param recordType: record type to find, or Record::Type::UNDEFINED for any record type.
  /// @return Pointer to the record info, or nullptr (timestamp is too big?).
  const IndexRecord::RecordInfo* getNearestRecordByTime(
      double timestamp,
      double epsilon,
      StreamId streamId = {},
      Record::Type recordType = Record::Type::UNDEFINED) const;

  /// Get a record's index in the global index.
  /// @param record: pointer of the record.
  /// @return Index in the global index, or getRecordCount() is record is nullptr.
  uint32_t getRecordIndex(const IndexRecord::RecordInfo* record) const;

  /// Get a record's index in the record's stream index.
  /// @param record: pointer of the record.
  /// @return Index in the record's stream index, or getRecordCount() is record is nullptr.
  uint32_t getRecordStreamIndex(const IndexRecord::RecordInfo* record) const;

  /// Timestamp for the first data record in the whole file.
  /// @return The timestamp for the file data record, or 0, if the file contains no data record.
  double getFirstDataRecordTime() const;

  /// Helper function: Read a stream's first configuration record.
  /// This might be necessary to properly read records containing image or audio blocks,
  /// if their configuration is defined in a configuration record using datalayout conventions.
  /// Notes:
  ///  - a RecordFormatStreamPlayer must be attached to the reader prior to making this call,
  ///    so it can find the RecordFormat and DataLayout definitions.
  ///  - if you provide a stream player, the caching happens in that stream player only.
  ///  - if the stream contains more than one configuration record, the first configuration record
  ///    probably won't be right for all the data records.
  /// @param streamId: StreamId of the record stream to consider.
  /// @param streamPlayer (optional): provide a streamPlayer that will receive the records.
  /// If provided and a stream player was previously registered to receive records for that stream,
  /// only the provided stream player will receive the data.
  /// @return True if a config record was read for the given stream.
  bool readFirstConfigurationRecord(StreamId streamId, StreamPlayer* streamPlayer = nullptr);

  /// Helper function: Read every stream's first configuration record.
  /// See readFirstConfigurationRecord() for more information.
  /// @param streamPlayer (optional): provide a stream player that will receive the records.
  /// @return True if a config record was read for the given stream.
  bool readFirstConfigurationRecords(StreamPlayer* streamPlayer = nullptr);

  /// Helper function: Read the first configuration record of all the streams of a particular
  /// recordable type. See readFirstConfigurationRecord() for more information.
  /// @param typeId: The RecordableTypeId of the type of device to look for.
  /// @param streamPlayer (optional): provide a stream player that will receive the records.
  /// @return true if a configuration record was properly read for each matching stream.
  bool readFirstConfigurationRecordsForType(
      RecordableTypeId typeId,
      StreamPlayer* streamPlayer = nullptr);

  /// Get the tags map for the whole file. Does not include any stream tag.
  /// @return The file's tags map.
  const map<string, string>& getTags() const {
    return fileTags_;
  }

  /// Get a specific tag by name.
  /// @param name: Name of the tag to look for.
  /// @return The tag value, or the empty string if the tag wasn't found.
  const string& getTag(const string& name) const {
    return getTag(fileTags_, name);
  }

  /// Get the tags for a specific record stream.
  /// @param streamId: StreamId of the record stream to consider.
  /// @return The tags for the stream.
  /// If the streamId doesn't exist in the file, the map returned will be an empty map.
  const StreamTags& getTags(StreamId streamId) const;

  /// Get the tags for all the streams at once.
  const map<StreamId, StreamTags>& getStreamTags() const {
    return streamTags_;
  }

  /// Get a specific tag for a specific record stream.
  /// @param streamId: StreamId of the record stream to consider.
  /// @param name: Name of the tag to look for.
  /// @return The tag value, or the empty string if the tag wasn't found.
  const string& getTag(StreamId streamId, const string& name) const {
    return getTag(getTags(streamId).user, name);
  }

  /// RecordableTypeId text descriptions may change over time, so at the time of recording,
  /// we capture the text name, so that we can see what it was when the file was recorded.
  /// @param streamId: StreamId of the record stream to consider.
  /// @return The original text description for the corresponding RecordableTypeId.
  const string& getOriginalRecordableTypeName(StreamId streamId) const;

  /// Streams using << Recordable Class >> ids require a << flavor >>,
  /// which must be provided when the stream was created.
  /// Use this API to get the recordable flavor provided, if any, when the stream was created.
  /// @param streamId: StreamId of the record stream to consider.
  /// @return The flavor for the corresponding RecordableTypeId, or an empty string,
  /// if no flavor was provided when the stream was created.
  const string& getFlavor(StreamId streamId) const;

  /// Get a stream's serial number.
  /// When streams are created, they are assigned a unique serial number by their Recordable object.
  /// That serial number is universally unique and it will be preserved during file copies, file
  /// processing, and other manipulations that preserve stream tags.
  /// @param streamId: StreamId of the record stream to consider.
  /// @return The stream's serial number, or the empty string if the stream ID is not
  /// valid. When opening files created before stream serial numbers were introduced,
  /// RecordFileReader automatically generates a stable serial number for every stream based on the
  /// file tags, the stream's tags (both user and VRS internal tags), and the stream type and
  /// sequence number. This serial number is stable and preserved during copy and filtering
  /// operations that preserve stream tags.
  const string& getSerialNumber(StreamId streamId) const;

  /// Get a string describing the stream configuration, with stream type, serial number, and record
  /// counts, so it can be used to verify that the file looks a specific way without doing a full
  /// checksum. The stream Instance ID are NOT used, as they might be modified during copies.
  string getStreamsSignature() const;

  /// Tell if a stream might contain at least one image (and probably will).
  /// This is a best guess effort, but it is still possible that no images are actually found!
  /// @param streamId: StreamId of the record stream to check.
  /// @return True if at least one Data record RecordFormat definition found in the stream has at
  /// least one image content block, and the stream contains at least one data record.
  bool mightContainImages(StreamId streamId) const;

  /// Tell if a stream might contain some audio data (and probably will).
  /// This is a best guess effort, but it is still possible that no audio will actually be found!
  /// @param streamId: StreamId of the record stream to check.
  /// @return True if at least one Data record RecordFormat definition found in the stream has at
  /// least one audio content block, and the stream contains at least one data record.
  bool mightContainAudio(StreamId streamId) const;

  /// Get the RecordFormat for a specific stream, record type & record format version.
  /// Mostly useful for testing.
  /// @param streamId: StreamId of the record stream to consider.
  /// @param recordType: Type of the records to consider.
  /// @param formatVersion: Version to consider.
  /// @param outFormat: RecordFormat reference to set.
  /// @return True if outFormat value was set, false if no RecordFormat version was found.
  bool getRecordFormat(
      StreamId streamId,
      Record::Type recordType,
      uint32_t formatVersion,
      RecordFormat& outFormat) const;

  /// Get all the RecordFormat description used in this VRS file.
  /// Mostly useful for tools like VRStool & VRSplayer.
  /// @param streamId: StreamId of the record stream to consider.
  /// @param outFormats: Reference to be set.
  /// @return Number of formats found.
  uint32_t getRecordFormats(StreamId streamId, RecordFormatMap& outFormats) const;
  std::unique_ptr<DataLayout> getDataLayout(StreamId streamId, const ContentBlockId& blockId) const;

  /// Option to control logging when opening a file.
  /// @param progressLogger: a logger implementation, or nullptr, to disable logging.
  void setOpenProgressLogger(ProgressLogger* progressLogger);

  /// Set & get the current file handler's Caching strategy.
  /// This should be called *after* opening the file, as open might replace the file handler.
  bool setCachingStrategy(CachingStrategy cachingStrategy) {
    return file_->setCachingStrategy(cachingStrategy);
  }
  CachingStrategy getCachingStrategy() const {
    return file_->getCachingStrategy();
  }

  /// Set callback function for cache stats
  bool setStatsCallback(FileHandler::CacheStatsCallbackFunction statsCallback) {
    return file_->setStatsCallback(statsCallback);
  }

  /// If the underlying file handler caches data on reads, purge its caches to free memory.
  /// Sets the caching strategy to Passive, and clears any pending read sequence.
  /// @return True if the caches were purged, false if they weren't for some reason.
  /// Note: this is a best effort. If transactions are pending, their cache blocks won't be cleared.
  bool purgeFileCache() {
    return file_->purgeCache();
  }

  /// Public yet methods that public API users should probably avoid, because there are better ways
  /// to get the same results. Maybe these are useful only for testing or for VRS' internal needs.

  /// Provide a different file handler, maybe to stream files off a network storage.
  /// By default, RecordFileReader uses DiskFile to read/write files from/to a local disk.
  /// Prefer using URIs or FileSpec objects to specify a custom FileHandler when opening a file.
  /// @param fileHandler: File handler to access files using a different method than the default
  /// DiskFile, for instance, to directly access files off a network location.
  void setFileHandler(std::unique_ptr<FileHandler> fileHandler);

  /// Get a clone of the current file handler, for use elsewhere.
  /// @return A copy of the current file handler.
  std::unique_ptr<FileHandler> getFileHandler() const;

  /// Convert a path to a FileSpec, including resolution of local chunked files
  /// @param filePath: a local file path, a URI, or a file spec in json format.
  /// @param outFileSpec: on exit and on success, set to the resulting file spec.
  /// @param checkLocalFile: only resolve links and look for additional chunks after validating that
  /// the local file is a VRS file, by reading the file's header and checking VRS signatures.
  /// @return A status code, 0 meaning success.
  static int
  vrsFilePathToFileSpec(const string& filePath, FileSpec& outFileSpec, bool checkLocalFile = false);

  class RecordTypeCounter : public std::array<uint32_t, enumCount<Record::Type>()> {
    using ParentType = std::array<uint32_t, enumCount<Record::Type>()>;

   public:
    RecordTypeCounter() {
      fill(0);
    }
    inline uint32_t operator[](Record::Type recordType) const {
      return ParentType::operator[](static_cast<uint32_t>(recordType));
    }
    inline uint32_t& operator[](Record::Type recordType) {
      return ParentType::operator[](static_cast<uint32_t>(recordType));
    }
    uint32_t totalCount() const;
  };

 private:
  int doOpenFile(const FileSpec& fileSpec, bool autoWriteFixedIndex, bool checkSignatureOnly);
  int readFileHeader(const FileSpec& fileSpec, FileFormat::FileHeader& outFileHeader);
  int readFileDetails(
      const FileSpec& fileSpec,
      bool autoWriteFixedIndex,
      FileFormat::FileHeader& fileHeader);
  bool readConfigRecords(
      const set<const IndexRecord::RecordInfo*>& configRecords,
      StreamPlayer* streamPlayer);

  const string& getTag(const map<string, string>& tags, const string& name) const; ///< private
  bool mightContainContentTypeInDataRecord(StreamId streamId, ContentType type) const; ///< private

  // Members to read an open VRS file
  std::unique_ptr<FileHandler> file_;
  UncompressedRecordReader uncompressedRecordReader_;
  CompressedRecordReader compressedRecordReader_;

  // Source of truth describing the VRS file: must never change while the file is open.
  set<StreamId> streamIds_;
  map<StreamId, StreamTags> streamTags_;
  map<string, string> fileTags_;
  vector<IndexRecord::RecordInfo> recordIndex_;
  mutable map<StreamId, RecordTypeCounter> streamRecordCounts_;

  // Pointers to stream players to notify when reading records. These are NOT owned by the class.
  map<StreamId, StreamPlayer*> streamPlayers_;

  // Misc less critical members, for presentation or optimization
  ProgressLogger defaultProgressLogger_;
  ProgressLogger* openProgressLogger_{&defaultProgressLogger_};
  unique_ptr<std::thread> detailsSaveThread_;
  mutable map<StreamId, vector<const IndexRecord::RecordInfo*>> streamIndex_;
  // Location of the last record searched for a specific stream & record type
  // The pair: index of the record for the type (query), index of the record in the stream (result)
  mutable map<pair<StreamId, Record::Type>, pair<uint32_t, size_t>> lastRequest_;
  int64_t endOfUserRecordsOffset_;
  uint32_t recordHeaderSize_;
  bool fileHasAnIndex_;
};

/// The method to search the nearest record from the index list.
/// Find the nearest record of a specific stream within
/// the range of (timestamp - epsilon) - (timestamp + epsilon).
/// @param timestamp: timestamp to seek.
/// @param epsilon: the threshold we search for the index.
/// @param recordType: record type to find, or Record::Type::UNDEFINED for any record type.
/// @return Pointer to the record info, or nullptr (timestamp is too big?).
const IndexRecord::RecordInfo* getNearestRecordByTime(
    const std::vector<const IndexRecord::RecordInfo*>& index,
    double timestamp,
    double epsilon,
    Record::Type recordType = Record::Type::UNDEFINED);

} // namespace vrs
