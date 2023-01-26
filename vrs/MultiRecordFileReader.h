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

#ifdef GTEST_BUILD
#include <gtest/gtest.h>
class GTEST_TEST_CLASS_NAME_(MultiRecordFileReaderTest, multiFile);
#endif

#include <string>
#include <vector>

#include <vrs/IndexRecord.h>
#include <vrs/RecordFileReader.h>
#include <vrs/TagConventions.h>

namespace vrs {

/// @brief Facilitates reading multiple VRS files simultaneously.
/// Records are sorted by timestamps across all the files, therefore it is essential that
/// *** all the files must have their timestamps in the same time domain. ***
/// Operates in a manner similar to `RecordFileReader`, but with multiple files.
class MultiRecordFileReader {
 public:
  /// External facing StreamId which handles collisions between StreamId across
  /// multiple files (RecordFileReaders).
  /// Since this is just an alias, it doesn't prevent misuse of using StreamId in places where
  /// UniqueStreamId is expected.
  using UniqueStreamId = StreamId;

  MultiRecordFileReader() = default;
  MultiRecordFileReader(const MultiRecordFileReader&) = delete;
  MultiRecordFileReader& operator=(const MultiRecordFileReader&) = delete;
  virtual ~MultiRecordFileReader();

  /// Open the given VRS files.
  /// Only related files are allowed to be opened together. i.e. the files which have the same
  /// values for tags defined in `kRelatedFileTags`. If these tags are present, then the values must
  /// match.
  /// All the files must have their timestamps in the same time domain.
  /// This method is expected to be invoked only once per instance.
  /// @param paths: VRS file paths to open.
  /// @return 0 on success and you can read all the files, or some non-zero
  /// error code, in which case, further read calls will fail.
  int open(const std::vector<std::string>& paths);

  /// Open the given VRS files.
  /// Only related files are allowed to be opened together. i.e. the files which have the same
  /// values for tags defined in `kRelatedFileTags`. If these tags are present, then the values must
  /// match.
  /// All the files must have their timestamps in the same time domain.
  /// This method is expected to be invoked only once per instance.
  /// @param fileSpecs: VRS file specs to open.
  /// @return 0 on success and you can read all the files, or some non-zero
  /// error code, in which case, further read calls will fail.
  int open(const std::vector<FileSpec>& fileSpecs);

  /// Open a single VRS file.
  /// This method is expected to be invoked only once per instance.
  /// @param path: VRS file path to open.
  /// @return 0 on success and you can read the file, or some non-zero error code,
  /// in which case, further read calls will fail.
  int open(const std::string& path) {
    return open(std::vector<std::string>{path});
  }

  /// Open a single VRS file.
  /// This method is expected to be invoked only once per instance.
  /// @param fileSpec: VRS fileSpec to open.
  /// @return 0 on success and you can read the file, or some non-zero error code,
  /// in which case, further read calls will fail.
  int open(const FileSpec& fileSpec) {
    return open(std::vector<FileSpec>{fileSpec});
  }

  /// Tags which determine whether VRS files are related to each other.
  /// Related files are expected to have the same value for these tags.
  static inline const string kRelatedFileTags[] = {
      tag_conventions::kCaptureTimeEpoch,
      tag_conventions::kSessionId};

  /// Close the underlying files, if any are open.
  /// @return 0 on success or if no file was open.
  /// Some file system error code upon encountering an error while closing any of the underlying
  /// files.
  int close();

  /// Get the set of StreamId for all the streams across all the open files.
  /// In case the the same StreamId is used in multiple files, this method generates UniqueStreamIds
  /// for disambiguation and uses those instead.
  /// @return The set of stream IDs for which there are records.
  const set<UniqueStreamId>& getStreams() const;

  /// Tell if files are being read. Must be true for most operations.
  /// @return True if the file is opened.
  bool isOpened() const {
    return isOpened_;
  }

  /// Get the number of records across all open files.
  /// @return The number of records across all open files, or 0, if no file is opened.
  uint32_t getRecordCount() const;

  /// Get the number of records of a specific stream.
  /// @param uniqueStreamId: StreamId of the record stream to consider.
  /// @return The number of records of the specified stream.
  uint32_t getRecordCount(UniqueStreamId uniqueStreamId) const;

  /// Get the number of records for a specific stream and specific record type.
  /// Attention: this computation has a linear complexity, so cache the result!
  /// @param uniqueStreamId: StreamId of the record stream to consider.
  /// @param recordType: Type of records to count.
  /// @return Number of records for the specified stream id & record type.
  uint32_t getRecordCount(UniqueStreamId uniqueStreamId, Record::Type recordType) const;

  /// Get the tags for a specific record stream.
  /// @param uniqueStreamId: StreamId of the record stream to consider.
  /// @return The tags for the stream.
  /// If the streamId doesn't exist in the file, the map returned will be an empty map.
  const StreamTags& getTags(UniqueStreamId uniqueStreamId) const;

  /// Get a specific file tag by name. (Not to be confused with stream tags)
  /// If multiple files are opened and they have multiple values for the same
  /// tag name, one of the values is returned arbitrarily.
  /// @param name: Name of the tag to look for.
  /// @return The tag value, or the empty string if the tag wasn't found.
  const string& getTag(const string& name) const {
    return getTag(fileTags_, name);
  }

  /// Get a specific tag for a specific record stream.
  /// @param uniqueStreamId: StreamId of the record stream to consider.
  /// @param name: Name of the tag to look for.
  /// @return The tag value, or the empty string if the tag wasn't found.
  const string& getTag(UniqueStreamId uniqueStreamId, const string& name) const {
    return getTag(getTags(uniqueStreamId).user, name);
  }

  /// Get a list of the constituent paths + sizes (in bytes) across all files.
  /// When no file is open, an empty vector is returned.
  /// When a single file is open, the underlying chunks with their sizes are returned.
  /// When multiple files are open, file paths and their sizes are returned.
  /// @return A vector of pairs path-file size in bytes.
  vector<std::pair<string, int64_t>> getFileChunks() const;

  /// Streams using << Recordable Class >> ids require a << flavor >>,
  /// which must be provided when the stream was created.
  /// Use this API to get the recordable flavor provided, if any, when the stream was created.
  /// @param uniqueStreamId: StreamId of the record stream to consider.
  /// @return The flavor for the corresponding RecordableTypeId, or an empty string,
  /// if no flavor was provided when the stream was created.
  const string& getFlavor(UniqueStreamId uniqueStreamId) const;

  /// Get a stream's serial number.
  /// @param uniqueStreamId: StreamId of the record stream to consider.
  /// @return The serial number, or an empty string if the stream wasn't found.
  const string& getSerialNumber(UniqueStreamId uniqueStreamId) const;

  /// Get a set of StreamId for a specific type, and an optional flavor.
  /// @param typeId: a recordable type id, maybe a Recordable Class.
  /// Use RecordableTypeId::Undefined to match any recordable type.
  /// @param flavor: an option flavor.
  /// @return A vector of stream ids of the given type, and of the provided flavor (if any).
  vector<UniqueStreamId> getStreams(RecordableTypeId typeId, const string& flavor = {}) const;

  /// Find the first stream with given tag name + value pair and RecordableTypeId
  /// @param tagName: The name of the tag to look for.
  /// @param tag: The tag value to look for.
  /// @param typeId: The RecordableTypeId to limit the search to, or RecordableTypeId::Undefined to
  /// look for any device type.
  /// Note: if more than one streams match the criteria, the "first" one is
  /// returned, which means the one with the lowest RecordableTypeId enum value, or if equal, the
  /// one with the lowest UniqueStreamId instanceId.
  /// @return A UniqueStreamId.
  /// Call isValid() to know if a matching stream was actually found.
  UniqueStreamId getStreamForTag(
      const string& tagName,
      const string& tag,
      RecordableTypeId typeId = RecordableTypeId::Undefined) const;

  /// Find the stream with the given serial number.
  /// @param serialNumber: the serial number to look for.
  /// @return A UniqueStreamId.
  /// Call isValid() to know if a matching stream was actually found.
  UniqueStreamId getStreamForSerialNumber(const string& serialNumber) const;

  /// Get a record's index in the global index, which is ordered by timestamp across all open files.
  /// @param record: Pointer of the record.
  /// @return Index in the global index, or getRecordCount() is record is nullptr or an invalid
  /// pointer.
  uint32_t getRecordIndex(const IndexRecord::RecordInfo* record) const;

  /// Get the record corresponding to the given index position in the global index.
  /// @param globalIndex: Position in the global index to look up.
  /// @return Corresponding record if present or nullptr if the given index is invalid.
  const IndexRecord::RecordInfo* getRecord(uint32_t globalIndex) const;

  /// Find a specific record for a specific stream, regardless of type, by index number.
  /// @param streamId: UniqueStreamId of the record stream to consider.
  /// @param indexNumber: Index position (for streamId - not global index) of the record to look
  /// for.
  /// @return Pointer to the record info, or nullptr if the streamId,
  /// or a record with the indexNumber wasn't found.
  const IndexRecord::RecordInfo* getRecord(UniqueStreamId streamId, uint32_t indexNumber) const;

  /// Find a specific record for a specific stream and type, by index number.
  /// nullptr is returned if no record is found for that index number.
  /// @param streamId: UniqueStreamId of the record stream to consider.
  /// @param recordType: Type of the records to consider.
  /// @param indexNumber: Index of the record to look for.
  /// @return Pointer to the record info,
  /// or nullptr if the streamId or the indexNumber wasn't found for that specific type.
  const IndexRecord::RecordInfo*
  getRecord(UniqueStreamId streamId, Record::Type recordType, uint32_t indexNumber) const;

  /// Find the last record for a specific stream and specific type.
  /// @param streamId: UniqueStreamId of the record stream to consider.
  /// @param recordType: Type of the records to consider.
  /// @return Pointer to the record info,
  /// or nullptr if the streamId or no record of the type was found.
  const IndexRecord::RecordInfo* getLastRecord(UniqueStreamId streamId, Record::Type recordType)
      const;

  /// Get a record index limited to a specific stream.
  /// @param streamId: UniqueStreamId of the record stream to consider.
  /// @return The index of the file, with all the records corresponding to the selected stream.
  const vector<const IndexRecord::RecordInfo*>& getIndex(UniqueStreamId streamId) const;

  /// RecordableTypeId text descriptions may change over time, so at the time of recording,
  /// we capture the text name, so that we can see what it was when the file was recorded.
  /// @param streamId: UniqueStreamId of the record stream to consider.
  /// @return The original text description for the corresponding RecordableTypeId.
  const string& getOriginalRecordableTypeName(UniqueStreamId streamId) const {
    return getTag(getTags(streamId).vrs, Recordable::getOriginalNameTagName());
  }

  /// Hook a stream player to a specific stream after opening a file and before reading records.
  /// The file player does *not* take ownership of the StreamPlayer.
  /// Using the same StreamPlayer instance for multiple streams is supported.
  /// So the caller is responsible for deleting the StreamPlayer objects after the file is read.
  /// Disconnect the StreamPlayer by passing a nullptr for the stream id.
  /// @param streamId: UniqueStreamId to hook the stream player to.
  /// @param streamPlayer: StreamPlayer to attach.
  void setStreamPlayer(UniqueStreamId streamId, StreamPlayer* streamPlayer);

  /// Get all the RecordFormat description used in this VRS file.
  /// Mostly useful for tools like VRStool & VRSplayer.
  /// @param streamId: UniqueStreamId of the record stream to consider.
  /// @param outFormats: Reference to be set.
  /// @return Number of formats found.
  uint32_t getRecordFormats(UniqueStreamId streamId, RecordFormatMap& outFormats) const;

  /// Preferred way to read records.
  /// @param recordInfo: RecordInfo reference of the record to read.
  /// @return 0 on success, or a non-zero error code.
  /// If there is no StreamPlayer hooked up for the stream, no read operation is done and 0 is
  /// returned.
  int readRecord(const IndexRecord::RecordInfo& recordInfo);

  /// Set Caching strategy for all the underlying file handlers.
  /// This should be called *after* opening the files, as open might replace the file handler.
  /// @param cachingStrategy: Caching strategy desired.
  /// @return True if the caching strategy was set.
  /// False if any of the underlying file handlers doesn't support the requested strategy, or any
  /// particular strategy.
  bool setCachingStrategy(CachingStrategy cachingStrategy);

  /// Get Caching strategy for all the underlying file handlers.
  /// The same strategy is supposed to be used by all file handlers.
  CachingStrategy getCachingStrategy() const;

  /// When streaming VRS files from the cloud, it may be very beneficial to tell before hand which
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

  /// If the underlying file handler caches data on reads, purge its caches to free memory.
  /// Sets the caching strategy to Passive, and clears any pending read sequence.
  /// @return True if all the underlying caches were purged, false if they weren't for some reason.
  /// Note: this is a best effort. If transactions are pending, their cache blocks won't be cleared.
  bool purgeFileCache();

  /// Get the tags map for all the underlying files. Does not include any stream tags.
  /// @return The tags map for all underlying files.
  const map<string, string>& getTags() const {
    return fileTags_;
  }

  /// Get the record with smallest timestamp across all streams and files, of a specified record
  /// type.
  /// @param recordType: Type of record to look for.
  /// @return Pointer to the first record of the specified type, or null if no records of
  ///         the specified type exist.
  const IndexRecord::RecordInfo* getFirstRecord(Record::Type recordType) const;

  /// Get the record with largest timestamp across all streams and files, of a specified record
  /// type.
  /// @param recordType: Type of record to look for.
  /// @return Pointer to the last record of the specified type, or null if no records of
  ///         the specified type exist.
  const IndexRecord::RecordInfo* getLastRecord(Record::Type recordType) const;

  /// Find the first record at or after a timestamp.
  /// @param timestamp: timestamp to seek.
  /// @return Pointer to the record info, or nullptr (timestamp is too big?).
  const IndexRecord::RecordInfo* getRecordByTime(double timestamp) const;

  /// Find the first record of a specific stream at or after a timestamp.
  /// @param streamId: UniqueStreamId of the stream to consider.
  /// @param timestamp: timestamp to seek.
  /// @return Pointer to the record info, or nullptr (timestamp is too big?).
  const IndexRecord::RecordInfo* getRecordByTime(UniqueStreamId streamId, double timestamp) const;

  /// Find the first record of a specific stream of a specific type at or after a timestamp.
  /// @param streamId: StreamId of the stream to consider.
  /// @param recordType: record type to find.
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

  /// Get a clone of the current file handler, for use elsewhere.
  /// @return A copy of the current file handler.
  /// nullptr may be returned if no underlying files are open yet.
  std::unique_ptr<FileHandler> getFileHandler() const;

  /// Get UniqueStreamId corresponding to the given record.
  /// This must be used as opposed to reading the StreamId from RecordInfo directly since it handles
  /// StreamId collisions between streams from multiple files.
  /// @param record: Record whose UniqueStreamId is supposed to be determined
  /// @return UniqueStreamId corresponding to the given record. An invalid UniqueStreamId is
  /// returned for an illegal record.
  UniqueStreamId getUniqueStreamId(const IndexRecord::RecordInfo* record) const;

  /// Get the total size of all underlying files.
  /// @return The number of bytes of all the files combined.
  int64_t getTotalSourceSize() const;

  /// Helper function: Read the first configuration record of a particular stream.
  /// This might be necessary to properly read records containing image or audio blocks,
  /// if their configuration is contained in the configuration record using datalayout conventions.
  /// Notes:
  ///  - a RecordFormatStreamPlayer must be attached to the stream prior to making this call,
  ///    because that's where the configuration data is being cached.
  ///  - that this API reads the first configuration record of the stream, and if the stream
  ///    contains more than one configuration record, that record may not be the right one for all
  ///    your data records.
  ///  - if you provide a stream player, the caching happens in that stream player, which won't
  ///    solve the use case described above.
  /// @param uniqueStreamId: UniqueStreamId of the record stream to consider.
  /// @param streamPlayer (optional): provide a streamPlayer that will receive the records.
  /// If provided, and a stream player is already registered to receive records for that stream, the
  /// provided stream player will get the data, and the registered stream player will not get
  /// anything.
  /// @return True if a config record was read for the given stream.
  bool readFirstConfigurationRecord(
      UniqueStreamId uniqueStreamId,
      StreamPlayer* streamPlayer = nullptr);

  /// Helper function: Read the first configuration record of all the streams.
  /// This might be necessary to properly read records containing image or audio blocks,
  /// if their configuration is contained in the configuration record using datalayout conventions.
  /// Notes:
  ///  - a RecordFormatStreamPlayer must be attached to the stream prior to making this call,
  ///    because that's where the configuration data is being cached.
  ///  - that this API reads the first configuration record of the stream, and if the stream
  ///    contains more than one configuration record, that record may not be the right one for all
  ///    your data records.
  ///  - if you provide a stream player, the caching happens in that stream player, which won't
  ///  solve the use
  ///    case described above.
  /// @param streamPlayer (optional): provide a stream player that will receive the records.
  /// If provided, and a stream player is already registered to receive records for that stream, the
  /// provided stream player will get the data, and the registered stream player will not get
  /// anything.
  /// @return True if a config record was read for the given stream.
  bool readFirstConfigurationRecords(StreamPlayer* streamPlayer = nullptr);

  /// Helper function: Read the first configuration record for all the streams of a particular
  /// recordable type. See readFirstConfigurationRecord() for limitations.
  /// @param typeId: The RecordableTypeId of the type of device to look for.
  /// @param streamPlayer (optional): provide a stream player that will receive the records.
  /// If provided, and a stream player is already registered to receive records for that stream, the
  /// provided stream player will get the data, and the registered stream player will not get
  /// anything.
  /// @return true if a configuration record was properly read for each matching stream.
  bool readFirstConfigurationRecordsForType(
      RecordableTypeId typeId,
      StreamPlayer* streamPlayer = nullptr);

  /// Get the list of RecordFileReader objects used to read all the streams.
  const std::vector<unique_ptr<RecordFileReader>>& getReaders() const {
    return readers_;
  }

 private:
  using StreamIdToUniqueIdMap = map<StreamId, UniqueStreamId>;
  using StreamIdReaderPair = std::pair<StreamId, RecordFileReader*>;

  /// Are we trying to read only a single file? Useful for special casing / optimizing the single
  /// file use case.
  bool hasSingleFile() const {
    return readers_.size() == 1;
  }

  /// Are the given files related i.e. have the same value for certain pre-specified tags
  /// (`kRelatedFileTags`)?
  /// MultiRecordFileReader will only allow you to open related files.
  /// @return true if the files associated with this instance are related, false otherwise.
  bool areFilesRelated() const;

  void initializeUniqueStreamIds();

  /// This depends on initializeUniqueStreamIds()
  void createConsolidatedIndex();

  void initializeFileTags();

  /// Finds a UniqueStreamId generated based on the given duplicateStreamId
  UniqueStreamId generateUniqueStreamId(StreamId duplicateStreamId) const;

  const StreamIdReaderPair* getStreamIdReaderPair(UniqueStreamId uniqueStreamId) const;

  const string& getTag(const map<string, string>& tags, const string& name) const;

  /// Returns the RecordFileReader corresponding to the given record.
  /// nullptr is returned in case the given record doesn't belong to any of the underlying readers.
  RecordFileReader* getReader(const IndexRecord::RecordInfo* record) const;

  /// Returns the UniqueStreamId corresponding to the StreamId contained in the given record.
  /// These "*Internal" methods will throw in case if invoked with illegal record, reader or
  /// StreamId where as getUniqueStreamId() will return an invalid UniqueStreamId in those cases.
  UniqueStreamId getUniqueStreamIdInternal(const IndexRecord::RecordInfo* record) const;
  UniqueStreamId getUniqueStreamIdInternal(const RecordFileReader* reader, StreamId streamId) const;

  bool timeLessThan(const IndexRecord::RecordInfo* lhs, const IndexRecord::RecordInfo* rhs) const;

  class RecordComparatorGT {
   public:
    explicit RecordComparatorGT(const MultiRecordFileReader& parent) : parent_(parent) {}

    bool operator()(const IndexRecord::RecordInfo* lhs, const IndexRecord::RecordInfo* rhs) const {
      return parent_.timeLessThan(rhs, lhs);
    }

   private:
    const MultiRecordFileReader& parent_;
  };

  bool isOpened_{false};
  /// Underlying RecordFileReader - one per VRS file
  std::vector<unique_ptr<RecordFileReader>> readers_;
  /// Index across all underlying VRS files. `nullptr` in single file case for optimization.
  unique_ptr<std::vector<const IndexRecord::RecordInfo*>> recordIndex_;
  /// StreamId related mapping to tackle collisions across different files
  /// Not meant to be used when hasSingleFile() == true
  set<UniqueStreamId> uniqueStreamIds_;
  map<const RecordFileReader*, StreamIdToUniqueIdMap> readerStreamIdToUniqueMap_;
  map<UniqueStreamId, StreamIdReaderPair> uniqueToStreamIdReaderPairMap_;
  /// File Paths underlying files
  vector<string> filePaths_;
  const RecordComparatorGT recordComparatorGT_{*this};
  map<string, string> fileTags_;

#ifdef GTEST_BUILD
  FRIEND_TEST(::MultiRecordFileReaderTest, multiFile);
#endif
};

} // namespace vrs
