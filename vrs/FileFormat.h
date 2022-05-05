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

#include "Record.h"

/// Writing headers to disk, you must control endianness and have no padding so that you can read a
/// file written by any system, using any other system.
///
/// The Endian<T> template allows the definition of types stored locally in a endian defined way.

namespace vrs {

/// \brief Namespace for key datastructures of a VRS file.
///
/// File format description:
///
/// Every file starts with one FileHeader structure. It is followed by an arbitrary number of
/// records. That header gives you the size of the FileHeader and the size of the RecordHeader
/// structures used for all the following records.
///
/// Be careful that both FileHeader & RecordHeader structures may grow in the future, so that when
/// you read a file, you must use these given sizes to move around the file, while skipping possible
/// parts of the headers your code doesn't know about.
///
/// Each record starts with a RecordHeader structure, which provides sizes so that you can find
/// both the next and the previous record, and implicitly tells you the size of its raw byte blob.
///
/// Each RecordHeader a typeId, which tells us what handler knows how to manipulate this data. This
/// typeId is extremely important and must not collide between types!
///
/// The RecordHeader gives an instance id, in case multiple streams of data of the same type are
/// needed. Each RecordHeader is immediately followed by a blob of raw bytes, which meaning is only
/// known by the type handler.
///
/// The RecordHeader provides a record format version, to be interpreted by the type handler, so
/// that type handlers can handle changes in the format of their data.
///
/// The RecordHeader also provides a timestamp for the data, to be used when playing back the data,
/// as well as a Record::Type.
///
namespace FileFormat {
#pragma pack(push, 1)

/// \brief Placeholder layer for endianness support, if we ever need it.
///
/// All it currently does is enforce that we read & write native types through get & set methods.
template <class T>
class LittleEndian final {
 public:
  /// Default constructor, using T's default initializer.
  LittleEndian() = default;

  /// Constructor with an init value.
  explicit LittleEndian(T value) {
    set(value);
  }

  /// Getter.
  /// @return Value in host's endianness.
  T get() const {
    return value_;
  }
  /// Setter.
  /// @param value: Value in host's endianness.
  void set(T value) {
    value_ = value;
  }

 private:
  T value_{};
};

/// Every file starts with this header, which may grow but not shrink!
struct FileHeader {
  LittleEndian<uint32_t> magicHeader1; ///< magic value #1
  LittleEndian<uint32_t> magicHeader2; ///< magic value #2
  LittleEndian<uint64_t> creationId; ///< A timestamp, hopefully unique, to match files (future).
  LittleEndian<uint32_t> fileHeaderSize; ///< This header size, in bytes.
  LittleEndian<uint32_t> recordHeaderSize; ///< Record headers' size, in bytes (same for all).
  LittleEndian<int64_t> indexRecordOffset; ///< Index record offset in the whole file.
  LittleEndian<int64_t> descriptionRecordOffset; ///< Description record offset in the whole file.
  LittleEndian<int64_t>
      firstUserRecordOffset; ///< Offset of the first user record in the file. If 0, the first
                             ///< record is just after the description record (original behavior)
  LittleEndian<uint64_t> future2; ///< For future use
  LittleEndian<uint64_t> future3; ///< For future use
  LittleEndian<uint64_t> future4; ///< For future use
  LittleEndian<uint32_t> magicHeader3; ///< magic value #3
  LittleEndian<uint32_t> fileFormatVersion; ///< file format version.

  /// Initialize the structure's fixed values with default values for a regular VRS file.
  void init();
  /// Initialize the structure's fixed values, with configuration options.
  void init(uint32_t magic1, uint32_t magic2, uint32_t magic3, uint32_t formatVersion);
  /// Check the sanity of the file header.
  /// @return True if the header looks valid for a VRS file, false otherwise.
  bool looksLikeAVRSFile() const;
  /// Check the sanity of the file header.
  /// @return True if the header looks valid a header we might have created, false otherwise.
  bool looksLikeOurFiles(uint32_t magic1, uint32_t magic2, uint32_t magic3) const;
  /// Check if the file format is supported.
  /// Might fail if the file was written by a recent version of VRS that changed the file format.
  bool isFormatSupported() const;
  /// By default, the index record is written at the end of the file (original behavior), and
  /// the first user record is just after the description record.
  /// You can reserve space for the index record between the description record and the first user
  /// record, so that it is possible the read the file forward only, for streaming.
  /// But if you do that, the VRS file can only be read by a newer version of VRS, so you must bump
  /// the file version number by calling this method.
  void enableFrontIndexRecordSupport();
  /// Get a best guess as to where user records end. If the file has no index, this value may be
  /// innacurate, but a sensible estimation will be returned (probably the end of the file).
  /// Only call this methods for file headers read from an actual VRS file.
  /// @param fileSize: the size of the VRS file, value returned if no better estimate can be made,
  /// or when the file's index is at the start of the file (in which case it is the right answer).
  /// @return The index of the first byte after the last user record.
  int64_t getEndOfUserRecordsOffset(int64_t fileSize) const;
};

// Re-interpret legacy recordable Type id
RecordableTypeId readRecordableTypeId(const FileFormat::LittleEndian<int32_t>& recordableTypeId);

/// \brief Every record starts with this header, and is followed by a raw data blob,
/// which semantic is private to the data type handler.
struct RecordHeader {
  RecordHeader();
  RecordHeader(
      Record::Type recordType,
      StreamId streamId,
      double timestamp,
      uint32_t formatVersion,
      CompressionType compressionType,
      uint32_t previousRecordSize,
      uint32_t recordSize,
      uint32_t uncompressedSize);
  LittleEndian<uint32_t> recordSize; ///< byte count to the next record, header + data
  LittleEndian<uint32_t> previousRecordSize; ///< byte count to the previous record, header + data
  LittleEndian<int32_t> recordableTypeId; ///< record handler type id
  LittleEndian<uint32_t> formatVersion; ///< data format version, as declared by the data producer
  LittleEndian<double> timestamp; ///< record presentation time stamp
  LittleEndian<uint16_t> recordableInstanceId; ///< record handle instance id
  LittleEndian<uint8_t> recordType; ///< See Record::Type
  LittleEndian<uint8_t> compressionType; ///< compression used, or 0 for none
  LittleEndian<uint32_t>
      uncompressedSize; ///< uncompressed payload size without header. 0 if not compressed.

  /// Set the record's type.
  /// @param type: Type of the record, as an enum.
  void setRecordType(Record::Type type) {
    recordType.set(static_cast<uint8_t>(type));
  }

  /// Get the record type, as an enum.
  /// @return The record type.
  Record::Type getRecordType() const {
    return static_cast<Record::Type>(recordType.get());
  }

  /// Set the recordable type id for this record.
  /// @param typeId: Recordable type id.
  void setRecordableTypeId(RecordableTypeId typeId) {
    recordableTypeId.set(static_cast<int32_t>(typeId));
  }

  /// Get the recordable type id if for this record.
  /// @return A recordable type id.
  RecordableTypeId getRecordableTypeId() const {
    return readRecordableTypeId(recordableTypeId);
  }

  /// Get the stream id for this record.
  /// @return The stream id for this record.
  StreamId getStreamId() const {
    return {getRecordableTypeId(), recordableInstanceId.get()};
  }

  /// Get the compression type used when writing the payload of this record.
  /// @return Compression type.
  CompressionType getCompressionType() const {
    return static_cast<CompressionType>(compressionType.get());
  }

  /// Set the compression type used when writing the payload of this record.
  /// @param type: Compression type.
  void setCompressionType(CompressionType type) {
    this->compressionType.set(static_cast<uint8_t>(type));
  }

  /// Initialize this header, for use as an index record.
  /// @param formatVersion: Record format version for this index record.
  /// @param indexSize: Payload size of the index record (without header).
  /// @param previousRecordSize: Size of the previous record, including the header.
  /// @param compressionType: Type of compression used. Don't forget to set uncompressedSize!
  void initIndexHeader(
      uint32_t formatVersion,
      uint32_t indexSize,
      uint32_t previousRecordSize,
      CompressionType compressionType);

  /// Initialize this header, for use as a description record.
  /// @param formatVersion: Record format version for this description record.
  /// @param descriptionRecordSize: Payload size of the description record (without header).
  /// @param previousRecordSize: Size of the previous record, including the header.
  void initDescriptionHeader(
      uint32_t formatVersion,
      uint32_t descriptionRecordSize,
      uint32_t previousRecordSize);
};

#pragma pack(pop)

/// Assemble four letters into a uint32_t to make "good looking" magic numbers on disk...
/// Because we're specific letters, we reduce chances of an accidental match.
constexpr uint32_t fourCharCode(char a, char b, char c, char d) {
  return static_cast<uint32_t>(a) | (static_cast<uint32_t>(b) << 8) |
      (static_cast<uint32_t>(c) << 16) | (static_cast<uint32_t>(d) << 24);
}

/// Debug method to printout key internal details about a VRS file for debugging purposes.
/// @return True if the file looks "good", false if anything doesn't look right.
bool printVRSFileInternals(FileHandler& file);

} // namespace FileFormat

} // namespace vrs
