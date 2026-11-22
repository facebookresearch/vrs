---
sidebar_position: 5
title: File Playback
---

To play back a VRS file:

- Create a [`RecordFileReader`](https://github.com/facebookresearch/vrs/blob/main/vrs/RecordFileReader.h) object and open a VRS file with `openFile()`.
- (Optional) Query the file tags.
- Query the streams in the file and, if needed, their tags.
- Attach [`StreamPlayer`](https://github.com/facebookresearch/vrs/blob/main/vrs/StreamPlayer.h) objects to the streams you want to read using `setStreamPlayer()`.
- _Play_ the file with `readAllRecords()`. The stream players receive callbacks in timestamp order, without waiting between records to match the recording speed.
- Or use the file or stream index to select records and call `readRecord()` on each one. If a data record relies on its stream's configuration (for example, for raw image dimensions), read the applicable configuration record first. `readRecord()` does not load it automatically.

Creating and reading VRS files is demonstrated in the [recording and playback sample](https://github.com/facebookresearch/vrs/blob/main/sample_code/SampleRecordAndPlay.cpp), the [`RecordFormat` & `DataLayout` sample](https://github.com/facebookresearch/vrs/blob/main/sample_code/SampleRecordFormatDataLayout.cpp), and [unit tests](https://github.com/facebookresearch/vrs/blob/main/vrs/test/file_tests/SimpleFileHandlerTest.cpp).

### Why are records read using a [`StreamPlayer`](https://github.com/facebookresearch/vrs/blob/main/vrs/StreamPlayer.h) callback object, rather than a "regular" blocking call?

New users of VRS are sometimes surprised that reading a record does not directly return an image or other parts of the record. Instead, `readRecord()` passes control to a `StreamPlayer` while reading.

The reasons are flexibility and performance.

A `StreamPlayer` can decide how to interpret the record, which parts to read, and where to put the data. For example, it can read image bytes directly into a destination buffer to avoid an extra copy.

You can use `RecordFormatStreamPlayer` to interpret records described by `RecordFormat`, or use your own `StreamPlayer` when the format or decoding path is specific to your application.

`StreamPlayer` callbacks are synchronous: when `readRecord()` returns, all callbacks for that call have finished. For instance, a player can read a record's metadata and skip the image data that follows.
