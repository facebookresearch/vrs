---
sidebar_position: 4
title: File Creation
---

To create a VRS file:

* Create a [`RecordFileWriter`](https://github.com/facebookresearch/vrs/blob/main/vrs/RecordFileWriter.h) object.
* Create as many [`Recordable`](https://github.com/facebookresearch/vrs/blob/main/vrs/Recordable.h) objects as needed (one per *device*). Each recordable will create a stream of records for itself. Recordables do not need to be aware of either the file writer or any other Recordable. 
* Add tags to the file writer and/or the recordables, as necessary, to describe your devices & setup (serial numbers, global configuration that's not explicit, maybe a user name, location, or whatever rocks your boat, like the height of the user). Just make sure you capture all the context you'll need when you'll play back this VRS file.  **Important:**
   * Use the tag name conventions defined in [`<VRS/TagConventions.h>`](https://github.com/facebookresearch/vrs/blob/main/vrs/TagConventions.h) as much as possible. If you need to use different tag names, please consider if they should become part of the conventions.
   * You must capture all the tags upfront, in case the recording is interrupted (out of disk space? crash?).
* Add the Recordable objects to the file writer.
* Have your recordables start to produce records, by calling [`Recordable::createRecord()`](https://github.com/facebookresearch/vrs/blob/main/vrs/Recordable.h#L236) whenever they need to, from any thread. Create a record maybe when an IMU packet is received, or an image is received from a camera.  Recordables need not know when recording is active or not, they should just produce records as often as necessary, all the time.  
  :::note
  *Every record has a timestamp (a `double` count of seconds), and the records of all the recordables must use the **same time domain**, as records will be sorted based on their timestamp!*
  :::
* Tell the file writer how often to discard "old" records, and what "old" actually means.
* Tell the file writer when to start recording. Older records will be discarded, newer records will start to be compressed (by default) & written to disk in realtime, in a background thread.
* Create the file & start recording. Just after creating the file, maybe explicitly discard records older than when you care.
* Tell the file writer when to stop recording. Newer records will be discarded.
* Stop recording & close the file.
* Stop producing records, eventually... :-)

## Choosing a `RecordableTypeId`

While initialy each device were basically given their own `RecordableTypeId` enum value, which implied the modification of the
[`<vrs/StreamId.h>`](https://github.com/facebookresearch/vrs/blob/main/vrs/StreamId.h)
file every time, we now recommend that you use an existing «Recordable Class» enum value of `RecordableTypeId` enum, defined in [`<vrs/StreamId.h>`](https://github.com/facebookresearch/vrs/blob/main/vrs/StreamId.h),
that best matches your use case, along with an associated with a «Recordable Flavor», which is merely is text string describing more specifically your use case.

«Recordable Flavors» should respect the following conventions, that will help make «flavors» unique, and more important, make them meaningful to human readers.

* «Flavors» should preferably start with “`team`”, “`project`”, “`device`” or “`tech`”,
* follow with a name for that team, project, device or tech,
* possibly continue with a secondary description, and even a third description, as necessary/desired,
* separate the different sections using a '/' character.

Flavor examples/suggestions (not actually used):

* `tech/positional_tracking`, if the recordable type id is enough,
* `tech/positional_tracking/top_left_camera` to maybe differentiate different instances of the same recordable type,
* `device/aria`, or maybe `device/aria/positional_camera/top_left`
* `project/quest/groundtruth`

A lot of discretion exists when choosing a flavor, so use your best judgment. 
