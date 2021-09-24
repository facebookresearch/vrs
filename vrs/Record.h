// (c) Facebook, Inc. and its affiliates. Confidential and proprietary.

#pragma once

#include <cfloat>

#include <vector>

#include "StreamId.h"

#include "WriteFileHandler.h"

namespace vrs {

class DataSource;
class Compressor;
class RecordManager;
class FileHandler;

/// Type of compression. Used in VRS record headers, so never modify the values.
enum class CompressionType : uint8_t {
  None = 0, ///< No compression.
  Lz4, ///< lz4 compression.
  Zstd, ///< zstd compression.
};

/// Record are containers of data, captured at a specific point in time (their timestamp).
///
/// Records can only be created using Recordable::createRecord(), and are then owned and managed
/// by the recordable's private RecordManager.
///
/// There are 3 types of user records: configuration, state and data records.
///
/// Configuration records are meant to describe how the device (or virtual device, such as an
/// algorithm), is configured: this could represent the resolution of a camera sensor, its
/// framerate, its exposure setting if it's fixed, etc. Whenever the configuration of a device
/// changes, a new configuration record should be generated. At the beginning of any recording,
/// a configuration record is also expected, and VRS itself might call a Recordable's
/// createConfigurationRecord() method to be sure a configuration record is created.
///
/// State records are meant to describe the internal state of the device or virtual device.
/// A camera might be configured in auto-exposure mode, so that the exposure of the camera might
/// evolve over time, based on the images recorded. Similarly, algorithms, in particular vision
/// algorithms, may have an internal state, and it might be useful to record that state.
/// However, we do not necessarily want to record every state change. The internal exposure of a
/// camera might change at every frame, and the potentially very large internal state of a
/// vision algorithm is also likely to change each time a sensor record is processed. Devices
/// with changing internal state are expected to generate state records, as necessary, but at a
/// controlled rate, so as to allow to reproduce replay conditions without generating an
/// overwhelming amount of data. Similarly to configuration records, State records might be
/// requested by VRS, by calling a Recordable's createStateRecord method.
///
/// Data records are used to capture the actual sensor data. Devices are expected to create Data
/// records whenever data is received from some kind of device driver, or abritrarily in the
/// case of synthetic data.
///
/// See Recordable::createRecord() to see how to create records.
class Record final {
 public:
  /// Maximum timestamp for a record.
  static const double kMaxTimestamp;

  /// Record type definitions.
  /// Only Configuration, State and Data records are used by the client users of the APIs.
  /// Tags records are internal to VRS, and will not be exposed in the RecordFileReader's index even
  /// when they are used internaly.
  enum class Type : uint8_t {
    UNDEFINED = 0, ///< don't use.
    STATE = 1, ///< device or algorithm state information.
    CONFIGURATION = 2, ///< device or algorithm configuration.
    DATA = 3, ///< device or algorithm data.
    TAGS = 4, ///< tags record (VRS internal type).
    COUNT ///< Count of enum values
  };

  /// When VRS is done using a record, it recycles it, rather than delete it.
  /// @internal
  void recycle();

  /// Copy data into the record, so that we can write it to disk later if we needed.
  /// @internal
  void set(
      double timestamp,
      Type type,
      uint32_t formatVersion,
      const DataSource& data,
      uint64_t creationOrder);

  /// Compress (if desirable & possible) & write the record to a file (header + data).
  /// @internal
  int compressAndWriteRecord(
      WriteFileHandler& file,
      StreamId streamId,
      uint32_t& inOutRecordSize,
      Compressor& compressor) {
    return writeRecord(file, streamId, inOutRecordSize, compressor, compressRecord(compressor));
  }

  /// Tell if an attempt should be made to compress the record.
  /// If compression can't reduce the size of the record, then the record is written uncompressed.
  /// @internal
  bool shouldTryToCompress() const;

  /// Try to compress the record.
  /// @return The compressed size, or 0 if the compression did not work.
  /// @internal
  uint32_t compressRecord(Compressor& compressor);

  /// Write a possibly compressed record to a file (header + data).
  /// @internal
  int writeRecord(
      WriteFileHandler& file,
      StreamId streamId,
      uint32_t& inOutRecordSize,
      Compressor& compressor,
      uint32_t compressedSize);

  /// Get the record's timestamp.
  double getTimestamp() const {
    return timestamp_;
  }

  uint64_t getCreationOrder() const {
    return creationOrder_;
  }

  /// Get the record's payload size, uncompressed.
  size_t getSize() const {
    return bufferUsedSize_;
  }

  /// Get the record's record type.
  Type getRecordType() const {
    return recordType_;
  }

  /// Get a record type as a text string.
  static const char* typeName(Type type);

 private:
  friend RecordManager;

  /// Records are created & deleted exclusively by a Recordable's RecordManager.
  Record(RecordManager& recordManager) : recordManager_(recordManager) {}
  ~Record() {}

 private:
  double timestamp_;
  Type recordType_;
  uint32_t formatVersion_;
  std::vector<uint8_t> buffer_;
  size_t bufferUsedSize_;
  uint64_t creationOrder_;

  RecordManager& recordManager_;
};

/// Get a record type as a text string.
string toString(Record::Type recordType);

template <class Enum>
Enum toEnum(const string& name);

/// Convert a record type name into an enum value.
template <>
Record::Type toEnum(const string& name);

} // namespace vrs
