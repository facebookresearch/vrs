// Copyright (c) Facebook, Inc. and its affiliates. Confidential and proprietary.

#pragma once

#include "ForwardDefinitions.h"
#include "Record.h"

namespace vrs {

class RecordReader;

/// Class describing which record is being read. Most fields are really self explanatory.
struct CurrentRecord {
  double timestamp;
  StreamId streamId;
  Record::Type recordType;
  uint32_t formatVersion;
  uint32_t recordSize; ///< Size of the record, uncompressed.
  /// In some situations, some data wasn't read yet, and the RecordReader object can let you:
  /// - know how much has been read.
  /// - know how much has not been read, yet.
  /// - read more data directly.
  RecordReader* reader;
};

/// \brief Class designed to receive record data when reading a VRS file.
///
/// Class to handle data read from records of a VRS file, by attaching an instance to one or more
/// streams of a RecordFileReader. This base class is the bare-bones way to read VRS records.
/// Reading records is now probably better handled by the specialized RecordFormatStreamPlayer.
///
/// For each record, the stream player will be presented the record in a callback named
/// processXXXHeader(), which will tell if the record should be read by returning true, in which
/// case the callback is expected to set the provided DataReference to tell where the record's data
/// should be read. Unpon completion of the read, the matching processXXX() callback will be
/// invoked, allowing the StreamPlayer to interpret/use the read data.
class StreamPlayer {
 public:
  virtual ~StreamPlayer();

  /// Callback called just after the instance was attached to a RecordFileReader.
  /// @param RecordFileReader: The record file reader the instance was attached to.
  /// @param StreamId: Stream the instance was attached to.
  virtual void onAttachedToFileReader(RecordFileReader&, StreamId) {}

  /// Callback called when a record of any type is about to be read.
  ///
  /// The default implementation delegates to the specialized callbacks below.
  /// @param record: Details about the record being read. @see CurrentRecord.
  /// @param outDataReference: Reference to be set, so as to tell where data should be read.
  /// @see DataReference
  /// @return True, if the record should be read.
  virtual bool processRecordHeader(const CurrentRecord& record, DataReference& outDataReference) {
    if (record.recordType == Record::Type::DATA) {
      return processDataHeader(record, outDataReference);
    } else if (record.recordType == Record::Type::CONFIGURATION) {
      return processConfigurationHeader(record, outDataReference);
    } else if (record.recordType == Record::Type::STATE) {
      return processStateHeader(record, outDataReference);
    } else {
      return false;
    }
  }
  /// Called after processRecordHeader() set the DataReference it was given and returned true, and
  /// after data was written to memory specified by the DataReference.
  /// @param record: Details about the record being read. @see CurrentRecord.
  /// @param bytesWrittenCount: Number of bytes read and written to the DataReference.
  /// The default implementation delegates to the specialized callbacks below.
  virtual void processRecord(const CurrentRecord& record, uint32_t readSize) {
    if (record.recordType == Record::Type::DATA) {
      processData(record, readSize);
    } else if (record.recordType == Record::Type::CONFIGURATION) {
      processConfiguration(record, readSize);
    } else if (record.recordType == Record::Type::STATE) {
      processState(record, readSize);
    }
  }

  /// Called when a State record is about to be read.
  /// @param record: Details about the record being read. @see CurrentRecord.
  /// @param outDataReference: Reference to be set, so as to tell where data should be read.
  /// @see DataReference
  /// @return True, if the record should be read.
  virtual bool processStateHeader(
      const CurrentRecord& /* record */,
      DataReference& /* outDataReference */) {
    return false;
  }

  /// Called after processStateHeader() set the DataReference it was given and returned true, and
  /// after data was written to memory specified by the DataReference.
  /// @param record: Details about the record being read. @see CurrentRecord.
  /// @param bytesWrittenCount: Number of bytes read and written to the DataReference.
  virtual void processState(const CurrentRecord& /* record */, uint32_t /* bytesWrittenCount */) {}

  /// Called when a Configuration record is about to be read.
  /// @param record: Details about the record being read. @see CurrentRecord.
  /// @param outDataReference: Reference to be set, so as to tell where data should be read.
  /// @see DataReference
  /// @return True, if the record should be read.
  virtual bool processConfigurationHeader(
      const CurrentRecord& /* record */,
      DataReference& /* outDataReference */) {
    return false;
  }

  /// Called after processConfigurationHeader() set the DataReference it was given and returned
  /// true, and after data was written to memory specified by the DataReference.
  /// @param record: Details about the record being read. @see CurrentRecord.
  /// @param bytesWrittenCount: Number of bytes read and written to the DataReference.
  virtual void processConfiguration(
      const CurrentRecord& /* record */,
      uint32_t /* bytesWrittenCount */) {}

  /// Called when a data record is about to be read.
  /// @param record: Details about the record being read. @see CurrentRecord.
  /// @param outDataReference: Reference to be set, so as to tell where data should be read.
  /// @see DataReference
  /// @return True, if the record should be read.
  virtual bool processDataHeader(
      const CurrentRecord& /* record */,
      DataReference& /* outDataReference */) {
    return false;
  }

  /// Called after processDataHeader() set the DataReference it was given and returned true, and
  /// after data was written to memory specified by the DataReference.
  /// @param record: Details about the record being read. @see CurrentRecord.
  /// @param bytesWrittenCount: Number of bytes read and written to the DataReference.
  virtual void processData(const CurrentRecord& /* record */, uint32_t /* bytesWrittenCount */) {}

  /// Called after a record was read, so maybe a follow-up action can be performed.
  /// @param reader: the VRS file reader used to read the file.
  /// @param recordInfo: the record that was just read.
  virtual int recordReadComplete(
      RecordFileReader& /*reader*/,
      const IndexRecord::RecordInfo& /*recordInfo*/) {
    return 0;
  }

  /// A stream player might be queueing read data for asynchronous processing.
  /// This method can be called to signal that internal data/queues should be flushed,
  /// so processing can be guaranteed to be completed.
  virtual void flush() {}
};

} // namespace vrs
