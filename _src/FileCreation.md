---
sidebar_position: 4
title: File Creation
---

To create a VRS file:

- Create a [`RecordFileWriter`](https://github.com/facebookresearch/vrs/blob/main/vrs/RecordFileWriter.h) object.
- Create as many [`Recordable`](https://github.com/facebookresearch/vrs/blob/main/vrs/Recordable.h) objects as needed, one per _device_. Each Recordable object will create a stream of records for itself. Recordables do not need to be aware of either the file writer or any other Recordables.
- Add tags to the file writer and the Recordables as necessary to describe your devices and setup. Add things such as serial numbers, global configuration settings, user names, locations, user heights, etc. Make sure you capture all the context you will need when you play back the VRS file.

- The following best practices are extremely important:
  - Use the tag name conventions defined in [`<VRS/TagConventions.h>`](https://github.com/facebookresearch/vrs/blob/main/vrs/TagConventions.h) as much as possible.
  - Capture all the tags upfront, in case the recording is interrupted (out of disk space or crashes).

- Add the Recordable objects to the file writer.
- Have your Recordables start to produce records, by calling [`Recordable::createRecord()`](https://github.com/facebookresearch/vrs/blob/main/vrs/Recordable.h#L236) as needed, from any thread. Create a record when an IMU packet is received, or an image is received from a camera. Recordables do not need to know when recording is active. They just need to produce records as often as necessary, all the time.

<!-- prettier-ignore -->
:::note
*Every record has a timestamp (a `double` count of seconds), and the records of all the Recordables must use the **same time domain**, as records will be sorted based on their timestamp!*
:::

- Tell the file writer how often to discard "old" records, and what "old" actually means.
- Tell the file writer when to start recording. Older records will be discarded. Newer records will start to be compressed (by default) and written to disk in realtime in a background thread.
- Create the file and start recording. Just after creating the file, explicitly discard records that are older than you need.
- Tell the file writer when to stop recording. Newer records will be discarded.
- Stop recording and close the file.
- Stop producing records, eventually.

## Choosing a `RecordableTypeId`

Originally, each device was given its own `RecordableTypeId` enum value, which implied the modification of the [`<vrs/StreamId.h>`](https://github.com/facebookresearch/vrs/blob/main/vrs/StreamId.h) file every time.

We now recommend that you use an existing «Recordable Class» enum value of `RecordableTypeId` enum, defined in [`<vrs/StreamId.h>`](https://github.com/facebookresearch/vrs/blob/main/vrs/StreamId.h). Choose the one that best matches your use case, and associate it with a «Recordable Flavor», which is a text string describing your specific use case.

«Recordable Flavors» should respect the following conventions, that will help make «flavors» unique, and more importantly, make them meaningful to human readers.

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
