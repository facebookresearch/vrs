---
sidebar_position: 4
title: File Creation
---

To create a VRS file:

- Create a [`RecordFileWriter`](https://github.com/facebookresearch/vrs/blob/main/vrs/RecordFileWriter.h) object.
- Create one [`Recordable`](https://github.com/facebookresearch/vrs/blob/main/vrs/Recordable.h) per _stream_. A stream can represent a physical device, an algorithm, or another source of records. Recordables do not need to know about the writer or one another.
- Add file tags for the overall recording setup, and stream tags for details specific to a Recordable, such as its device serial number. Capture the context you will need during playback.

- Set tags before creating the file:
  - Use the tag name conventions defined in [`<vrs/TagConventions.h>`](https://github.com/facebookresearch/vrs/blob/main/vrs/TagConventions.h) where applicable.
  - Set all tags before opening the file for writing. VRS writes them at file creation; changes made afterward are not saved.

- Add the Recordable objects to the file writer with `addRecordable()`. The writer does not take ownership, so the Recordables must outlive it.
- Have your Recordables produce records by calling [`Recordable::createRecord()`](https://github.com/facebookresearch/vrs/blob/main/vrs/Recordable.h) as data arrives, for example when an IMU packet or camera frame is received. Recordables can produce records from multiple threads. Synchronize access to any buffers or `DataLayout` objects shared by those threads.

<!-- prettier-ignore -->
:::note
*Every record has a timestamp (a `double` count of seconds), and the records of all the Recordables must use the **same time domain**, as records are indexed by timestamp!*
:::

- If you buffer records before creating the file, use `autoPurgeRecords()` to discard records outside the window you want to keep.
- Call `createFileAsync()` to open the file and start the background writer. Automatic purging is suspended while the file is open.
- To trim the initial buffer further, call `purgeOldRecords()` before sending the first batch to the writer. It keeps the latest configuration and state records before the cutoff for each stream.
- Call `writeRecordsAsync()` periodically, or configure `autoWriteRecordsAsync()`, to send records older than a moving timestamp cutoff to the background writer.
- Stop producing records for this file, then call `waitForFileClosed()` to write any remaining records and close the file. Check its return value for write errors.
- If all the records are already in memory and no file is open, you can use `writeToFile()` instead of the asynchronous writing calls.

## Choosing a `RecordableTypeId`

Originally, each device was given its own `RecordableTypeId` enum value, which required editing [`<vrs/StreamId.h>`](https://github.com/facebookresearch/vrs/blob/main/vrs/StreamId.h).

Use an existing «Recordable Class» value of `RecordableTypeId`, defined in [`<vrs/StreamId.h>`](https://github.com/facebookresearch/vrs/blob/main/vrs/StreamId.h). Choose the class that best matches your data, and pair it with a «Recordable Flavor», a string describing your specific use case.

Use the following «Recordable Flavor» conventions to make flavors distinct and meaningful to readers.

- «Flavors» should preferably start with “team”, “project”, “device”, or “tech”.
- Follow it with a name for that team, project, device, or tech.
- Continue with a secondary description, and even a third description, as desired.
- Separate the different sections using a '/' character.

Flavor examples/suggestions (not actually used):

- `tech/positional_tracking`, if the recordable type ID is enough.
- `tech/positional_tracking/top_left_camera` to differentiate different instances of the same recordable types.
- `device/aria`, or maybe `device/aria/positional_camera/top_left`
- `project/quest/groundtruth`

A lot of discretion exists when choosing a flavor, so use your best judgment.
