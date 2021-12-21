# What is in this folder?

The code in this folder implements VRS' core functionality, and only the core
functionality, in a flat structure. Rather than having a strict model with one
class per cpp/h pair, which quickly leads to a huge number of files, VRS might
define multiple related minor and small classes in the same cpp/h pair.

The following classification of headers is designed to help new users navigate
the code and focus on the pieces they are interested in, separating the class
they will want to use from the implementation layers, that users of VRS should
never need to even look at. You are virtually guaranteed to directly create or
interact with the top ones, but should probably never need to even look as the
last ones in the list.

## Reading VRS Files

- `RecordFileReader.h`: `RecordFileReader`, to open and read VRS files.
- `RecordFormatStreamPlayer.h`: `RecordFormatStreamPlayer`, to receive data from
  read records using `RecordFormat` and `DataLayout`.
- `StreamId.h`: `StreamId`, to identify the streams in a file.
- `MultiRecordFileReader.h`: `MultiRecordFileReader`, to read multiple related
  VRS files at once. (Advanced use case, when a single recording was split into
  multiple files).
- `DataReference.h`: `DataReference` to tell when read data should be written
  and avoid unnecessary data copies.

## Writing VRS Files

- `RecordFileWriter.h`: `RecordFileWriter`, to create new VRS files.
- `Recordable.h`: `Recordable`, to create a stream in a `RecordFileWriter`.
- `DataSource.h`: `DataSource` and associated classes, to specify where the data
  of a new record is found.

## Representing Data in Records

- `RecordFormat.h`: `RecordFormat`, to specify how a record is structured in
  `ContentBlock` parts.
- `DataLayout.h`: `DataLayout`, to manage datalayout content blocks.
- `DataPieces.h`, `DataPieceArray.h`, `DataPieceString.h`,
  `DataPieceStringMap.h`, `DataPieceTypes.h`, `DataPieceValue.h`,
  `DataPieceVector.h`: `DataPiece` types, the element parts that constitute a
  `DataLayout`.
- `DataLayoutConventions.h`: Definitions that constitute the "datalayout
  conventions", that make `RecordFormat` and `DataLayout` work.
- `TagConventions.h`: Definitions of file and stream tag conventions.

## File Operation Helpers

- `FileHandler.h`: `FileHandler` abstract interface, to read files wherever they
  actually live, depending on implementation: local machine, cloud storage, etc.
- `FileHandlerFactory.h`: `FileHandlerFactory`, to handle `FileHandler`
  creation.
- `DiskFile.h`: `DiskFile`, to read local files, VRS or not.
- `WriteFileHandler.h`: `WriteFileHandler` abstract interface, to write files in
  different places.
- `NewChunkHandler.h`: `NewChunkHandler`, to be notified when a new VRS file
  chunk is created.
- `ErrorCode.h`: Helpers to handle error codes from unrelated modules.
- `FileCache.h`, `FileDetailsCache.h`: Helper infra, to cache data when
  accessing cloud storage in particular.
- `TelemetryLogger.h`: `TelemetryLogger`, for usage telemetry. The open source
  implementation doesn't implement any actual telemetry, and merely writes
  messages to the log output.

## Misc Helpers

- `Record.h`: `Record`, used when creating data.
- `RecordManager.h`: `RecordManager`, to manage a `Recordable` object's `Record`
  objects during file creation.
- `StreamPlayer.h`: `StreamPlaer`, the base class defining core callbacks when
  reading records.
- `ContentBlockReader.h`: `ContentBlockReader` and associates, to read
  `ContentBlock` parts.
- `RecordReaders.h`: `RecordReader`, to read data from a record, losslessly
  compressed or not.
- `Compressor.h`: `Compressor`, to losslessly compress data.
- `Decompressor.h`: `Decompressor`, to read losslessly compressed data.
- `ForwardDefinitions.h`: Forward definition, to help limit header includes.
- `LegacyFormatsProvider.h`: `LegacyFormatsProvider` to make it possible to read
  files created before the introduction of `RecordFormat` and `DataLayout` using
  `RecordFormat` and `DataLayout`. Hopefully never needed in open source use
  cases!
- `ProgressLogger.h`: `ProgressLogger` to track progress when opening a file.
  Probably only needed with handling files in cloud storage.

## File Format Core Internal

These are the building blocks that define that actual VRS file format. Note that
there is no VRS file format specification document, in part to discourage the
temptation to re-implement the spec to support another language. Instead, please
build language bindings that rely on this open source C++ implementation. How
VRS files actually look like is not locked down intentionally, so that we can
make the file format evolve as radically and often as we might need.

- `FileFormat.h`: `FileHeader` and `RecordHeader`, to hold data describing VRS
  files and VRS records.
- `IndexRecord.h`: To read and write VRS index records, which contain the VRS
  file's index.
- `DescriptionRecord.h`: To read and write description records, which contain
  the VRS file's metadata, including all the file and stream tags, including
  `RecordFormat` and `DataLayout` definitions.
- `TagsRecord.h`: To read and write tag records, needed when streams are created
  after the file was initially created.

In effect, the VRS file format is defined by the definitions found in
`FileFormat.h`, `IndexRecord.h`, `DescriptionRecord.h`, `TagsRecord.h`, but also
`RecordFormat.h`, `DataLayout.h` and associated `DataPiece` objects,
`DataLayoutConventions.h`, using json, lz4, and zstd.
