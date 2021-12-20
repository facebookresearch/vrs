---
sidebar_position: 5
title: File Playback
---

To playback a VRS file:

- Create a [`RecordFileReader`](https://github.com/facebookresearch/vrs/blob/main/vrs/RecordFileReader.h) object, and point it to a VRS file.
- (Optional) Query the tags.
- Query for the streams the file contains, and check their tags if you want.
- Attach [`StreamPlayer`](https://github.com/facebookresearch/vrs/blob/main/vrs/StreamPlayer.h) objects to the streams you want to read.
- _Play_ the file. Callbacks of the stream player objects will be invoked for each record in turn, as fast as possible.
- or: Use the file and/or stream indexes to choose the records to read, one at a time, using the same [`StreamPlayer`](https://github.com/facebookresearch/vrs/blob/main/vrs/StreamPlayer.h) callbacks.

Creating and reading VRS files is demonstrated in a number of other places, [including pre-`RecordFormat` & `DataLayout` sample code](https://github.com/facebookresearch/vrs/blob/main/sample_code/SampleRecordAndPlay.cpp), [`RecordFormat` & `DataLayout` sample code](https://github.com/facebookresearch/vrs/blob/main/sample_code/SampleRecordFormatDataLayout.cpp), and [unit tests](https://github.com/facebookresearch/vrs/blob/main/vrs/test/file_tests/SimpleFileHandlerTest.cpp).

### Why are records read using a [`StreamPlayer`](https://github.com/facebookresearch/vrs/blob/main/vrs/StreamPlayer.h) callback object, rather than a "regular" blocking call?

New users of VRS are sometimes surprised that they cannot "just read" a record to retrieve an image or some other parts of a record using a blocking call, which would feel more convenient to them. Instead, they are forced to use `StreamPlayer` callback objects that feel cumbersome and slow. Why is that?

The reasons are flexibility and performance.

If the data in a record had to be returned by a "classic" API, you would not be able to specify how the record needs to be interpreted. When using callback `StreamPlayer` objects, the interpretation of the data is delegated to an object (therefore, code) that can implement the exact method you choose to interpret a particular type of record, and it can decide which part(s) need to be read, and how and where the data needs to be saved. This should help to avoid unnecessary memory copy, which is particularly important with image buffers.

Maybe you want to interpret the data using the `RecordFormat` conventions with a `RecordFormatStreamPlayer`, but maybe not. Maybe you need video codec support, which will probably force a dependency on ffmpeg, but maybe not, and maybe you prefer to pass a video codec buffer to an HW decoder yourself.

`StreamPlayer` objects are not asynchronous callback objects, nor are they some kind of "futures". After a `readRecord` call returns, the record has been read and all the callbacks have been completed. No latency is introduced in that process - on the contrary. Since the record's data interpretation is delegated to code that knows best what to do with the record's data, latencies are reduced to a minimum. For instance, the data layout part of a record might be read, while the image data that follows could easily be skipped.
