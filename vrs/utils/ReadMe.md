# What is in this folder?

The code here provides functionality commonly used to work with VRS files,
sometimes in command line tools. In particular, it provides standardized methods
to iterate over the records of a VRS file with filters for streams, or for time
ranges. This can be the base for tools that extracts data, processes files or
parts of the file depending on injected arguments (command line tools).

Note that when talking about "filters", two types of filtering are not to be
confused:

- FilteredFileReader filters records based on their metadata (stream id, record
  type, timestamps...) and "filtering" here means iterating over a subset of
  records (records are possibly filtered out).
- FilterCopy operations provides methods to apply filtering at the record
  content level while using one or more FilteredFileReader objects. So not only
  can you filter which records are processed, but you can filter the records'
  content while they are being copied/merged.

## FilteredFileReader

Methods to iterate over the records of a file, filtered by stream, record type,
or timestamps.

## FilterCopyHelpers

Methods to process records, maybe copying them "as is", or making modifications
on the fly during a copy operation.

## FilterCopy

Methods to process one or more VRS input files to generate an output VRS file
that contains data copied from the source files, maybe filtered in the two ways
described above.

## ThrottleHelpers

Helper classes for FilterCopy operations. Only needed for advanced operations.

## Validation

Methods to validate a VRS file's content, and produce checksums. Useful to check
the integrity of a data collection pipeline.

## RecordFileInfo

Methods to print a description of a VRS file, to have an understanding of what
it contains.
