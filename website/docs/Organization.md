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
  You'll find the generated HTML at `website/static/doxygen/index.html`.
- [Sample code](https://github.com/facebookresearch/vrs/tree/main/sample_code), which demonstrates the APIs and is compiled as a library rather than run as an app.
  - [SampleRecordAndPlay.cpp](https://github.com/facebookresearch/vrs/blob/main/sample_code/SampleRecordAndPlay.cpp) demonstrates two ways to create a file: writing all the records from memory at once, or writing records in the background while continuing to create them.
  - [SampleImageReader.cpp](https://github.com/facebookresearch/vrs/blob/main/sample_code/SampleImageReader.cpp) demonstrates how to read records containing metadata and an image.
  - [SampleRecordFormatDataLayout.cpp](https://github.com/facebookresearch/vrs/blob/main/sample_code/SampleRecordFormatDataLayout.cpp) demonstrates how to write and read metadata blocks.
- [Sample apps](https://github.com/facebookresearch/vrs/tree/main/sample_apps), which are runnable examples. [The recording app](https://github.com/facebookresearch/vrs/blob/main/sample_apps/SampleRecordingApp.cpp) generates a VRS file with different record types containing sample data. [The playback app](https://github.com/facebookresearch/vrs/blob/main/sample_apps/SamplePlaybackApp.cpp) reads that file and verifies its records.
