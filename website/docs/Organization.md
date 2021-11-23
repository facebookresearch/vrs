---
sidebar_position: 2
title: Organization
---

The VRS documentation has different parts:
- This documentation, which covers overall concepts and principles.
- The API documentation, generated from the source code's documentation using Doxygen conventions.  
  To generate the API documentation, from the VRS repo, run:
   ```bash
   doxygen vrs/Doxyfile
   ```
- [Sample code](https://github.com/facebookresearch/vrs/tree/main/sample_code),
which isn't really functional, but demonstrates how to use the APIs.
  - [SampleRecordAndPlay.cpp](https://github.com/facebookresearch/vrs/blob/main/sample_code/SampleRecordAndPlay.cpp)
  demonstrates different ways to create a file, dumping the whole content from memory to disk after creating all the records in memory,
  or writing the record to disk while continuing to create records.
  - [SampleImageReader.cpp](https://github.com/facebookresearch/vrs/blob/main/sample_code/SampleImageReader.cpp)
  demonstrates how to read
  typical image records, that is, records containing metadata and an image.
  - [SampleRecordFormatDataLayout.cpp](https://github.com/facebookresearch/vrs/blob/main/sample_code/SampleRecordFormatDataLayout.cpp)
  demonstrates how to read metadata blocks.
- [Sample apps](https://github.com/facebookresearch/vrs/tree/main/sample_apps), which are runable apps (though not actually useful).
[The first app](https://github.com/facebookresearch/vrs/blob/main/sample_apps/SampleRecordingApp.cpp)
generates a VRS file with different record types containing some made up data.
[The second app](https://github.com/facebookresearch/vrs/blob/main/sample_apps/SamplePlaybackApp.cpp)
reads that VRS file and verifies that the record's content is as expected.
