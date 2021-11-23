---
sidebar_position: 5
title: File Playback
---

To playback a VRS file:

* Create a [`RecordFileReader`](https://github.com/facebookresearch/vrs/blob/main/vrs/RecordFileReader.h) object, and point it to a VRS file.
* [optional] Query the tags.
* Query for the streams the file contains, and check their tags if you care.
* Attach [`StreamPlayer`](https://github.com/facebookresearch/vrs/blob/main/vrs/StreamPlayer.h) objects to the streams you want to read.
* *Play* the file, and callbacks of the stream player objects will be invoked for each record in turn, as fast as possible.
* or: use the file and/or stream indexes to chose the records to read, one at a time, using the same [`StreamPlayer`](https://github.com/facebookresearch/vrs/blob/main/vrs/StreamPlayer.h) callbacks.

Creating & reading VRS files is demonstrated in a number of other places,
[including pre-`RecordFormat` & `DataLayout` sample code](https://github.com/facebookresearch/vrs/blob/main/sample_code/SampleRecordAndPlay.cpp),
[`RecordFormat` & `DataLayout` sample code](https://github.com/facebookresearch/vrs/blob/main/sample_code/SampleRecordFormatDataLayout.cpp), and
[unit tests](https://github.com/facebookresearch/vrs/blob/main/vrs/test/file_tests/SimpleFileHandlerTest.cpp).

### Why are records read using a [`StreamPlayer`](https://github.com/facebookresearch/vrs/blob/main/vrs/StreamPlayer.h) callback object, rather than a "regular" blocking call?

New users of VRS are sometimes surprised that they can't they can't "just read" a record to retrieve an image or some other parts of a record using a blocking call, which would feel more convenient to them. Instead, they are forced to use `StreamPlayer` callback objects, that feel cumbersome and slow. Why is that?

There reasons are flexibility and performance.

If the data in a record had to be returned by a "classic" API returning the data, you would not be able to specify how the record needs to be interpreted. Using callback `StreamPlayer` objects, the interpretation of the data is delegated to an object (therefore, code) that can implement the exact method you chose to interpret a particular type of record, deciding which part(s) need to be read, how, and where the data needs to be saved, possibly avoiding unnecessary memory copy, which is particularly important with image buffers.

Maybe you want to interpret the data using the `RecordFormat` conventions with a `RecordFormatStreamPlayer`, but maybe you don't. Maybe you need video codec support, which will probably force a dependency on ffmpeg, but maybe you don't, and maybe you prefer to pass a video codec buffer to a HW decoder yourself.

`StreamPlayer` objects are not asynchronous callback objects or some kind of "futures". When a `readRecord` call returns, the record was read and all the callbacks completed. No latency was introduced by that process. On the contrary, because the record's data interpretation was delegated to code that, presumably at least, knows best what to do with the record's data, latencies can be reduced to the minimum. For instance, the datalayout part of a record might be read, while the image data that follows could easily be skipped.
