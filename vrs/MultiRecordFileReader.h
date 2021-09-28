// Copyright (c) Facebook, Inc. and its affiliates. Confidential and proprietary.

#pragma once

#ifdef GTEST_BUILD
#include <gtest/gtest.h>
class GTEST_TEST_CLASS_NAME_(MultiRecordFileReaderTest, consolidatedIndex);
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
  /// All the files must have their timestamps in the same time domain.π
  /// This method is expected to be invoked only once per instance.
  /// @param paths: VRS file paths to open.
  /// @return 0 on success and you can read the file, or some non-zero error code, in which case,
  /// further read calls will fail.
  int openFiles(const std::vector<std::string>& paths);

  /// Tags which determine whether VRS files are related to each other.
  /// Related files are expected to have the same value for these tags.
  static inline const string kRelatedFileTags[] = {
      tag_conventions::kCaptureTimeEpoch,
      tag_conventions::kSessionId};

  /// Close the underlying files, if any are open.
  /// @return 0 on success or if no file was open.
  /// Some file system error code upon encountering an error while closing any of the underlying
  /// files.
  int closeFiles();

  /// Get the set of StreamId for all the streams across all the open files.
  /// In case the the same StreamId is used in multiple files, this method generates UniqueStreamIds
  /// for disambiguation and uses those instead.
  /// @return The set of stream IDs for which there are records.
  const set<UniqueStreamId>& getStreams() const;

  /// Tell if files are being read. Must be true for most operations.
  /// @return True if the file is opened.
  bool isOpened() const;

  /// Get the number of records across all open files.
  /// @return The number of records across all open files, or 0, if no file is opened.
  uint32_t getRecordCount() const;

  /// Get the number of records of a specific stream.
  /// @param uniqueStreamId: StreamId of the record stream to consider.
  /// @return The number of records of the specified stream.
  uint32_t getRecordCount(UniqueStreamId uniqueStreamId) const;

  /// Get the number of records for a specific stream and specifc record type.
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
  /// @param streamId: StreamId of the record stream to consider.
  /// @return The flavor for the corresponding RecordableTypeId, or an empty string,
  /// if no flavor was provided when the stream was created.
  const string& getFlavor(UniqueStreamId streamId) const;

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
  /// Call isValid() to know if a matching StreamId was actually found.
  UniqueStreamId getStreamForTag(
      const string& tagName,
      const string& tag,
      RecordableTypeId typeId = RecordableTypeId::Undefined) const;

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

  void createConsolidatedIndex();

  void initializeUniqueStreamIds();

  /// Finds a UniqueStreamId generated based on the given duplicateStreamId
  UniqueStreamId generateUniqueStreamId(StreamId duplicateStreamId) const;

  const StreamIdReaderPair* getStreamIdReaderPair(UniqueStreamId uniqueStreamId) const;

  const string& getTag(const map<string, string>& tags, const string& name) const;

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

#ifdef GTEST_BUILD
  FRIEND_TEST(::MultiRecordFileReaderTest, consolidatedIndex);
#endif
};

} // namespace vrs
