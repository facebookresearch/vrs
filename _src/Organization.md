---
sidebar_position: 2
title: Organization
---

import Link from '@docusaurus/Link';

The VRS documentation has different parts:

- This documentation, which describes the concepts, features, and principles of VRS.
- The <Link target="_blank" to="/doxygen/index.html">API Documentation</Link>, generated using Doxygen.  
  To generate the API documentation from the VRS code, run:
  ```bash
    cd <vrs_repo_top_level_folder>
    doxygen vrs/Doxyfile
  ```
  You'll find the API documentation in html at `website/static/doxygen/index.html`.
- [Sample code](https://github.com/facebookresearch/vrs/tree/main/sample_code), which is not functional, but demonstrates how to use the APIs.
  - [SampleRecordAndPlay.cpp](https://github.com/facebookresearch/vrs/blob/main/sample_code/SampleRecordAndPlay.cpp) demonstrates different ways to create a file, by dumping the whole content from memory to disk after creating all the records in memory, or by writing the record to disk while continuing to create records.
  - [SampleImageReader.cpp](https://github.com/facebookresearch/vrs/blob/main/sample_code/SampleImageReader.cpp) demonstrates how to read typical image records, that is, records containing metadata and an image.
  - [SampleRecordFormatDataLayout.cpp](https://github.com/facebookresearch/vrs/blob/main/sample_code/SampleRecordFormatDataLayout.cpp) demonstrates how to read metadata blocks.
- [Sample apps](https://github.com/facebookresearch/vrs/tree/main/sample_apps), which are runnable apps (though not actually useful). [The first app](https://github.com/facebookresearch/vrs/blob/main/sample_apps/SampleRecordingApp.cpp) generates a VRS file with different record types containing made-up data. [The second app](https://github.com/facebookresearch/vrs/blob/main/sample_apps/SamplePlaybackApp.cpp) reads that VRS file and verifies that the record content is as expected.
