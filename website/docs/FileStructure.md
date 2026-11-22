---
sidebar_position: 3
title: File Structure
---

import Tabs from '@theme/Tabs'; import TabItem from '@theme/TabItem';

## File Tags

VRS files contain file tags. File tags are a set of name/value pairs, both of which are text strings. File tags may be set to any text string, but VRS provides some [tag conventions](https://github.com/facebookresearch/vrs/blob/main/vrs/TagConventions.h) to represent a few common concepts in VRS.

## Streams

VRS files contain multiple streams, each associated with a _device type_, defined by a [`RecordableTypeId`](https://github.com/facebookresearch/vrs/blob/main/vrs/StreamId.h) enum value.

A subset of `RecordableTypeId` values represent generic devices, which we called «Recordable Class» IDs. «Recordable Class» IDs must be paired with «Recordable Flavor» values to identify a user-specific type of device or data contained in that stream.

A device type can also represent a virtual device, such as the output of a vision algorithm, or an event, such as a keyboard, mouse, or orientation event, that can be used to exercise code.

## Stream Tags

Streams also have tags, which are also a set of name/value pairs, both of which are text strings. Stream tags may be set to any string value, but VRS provides some [tag conventions](https://github.com/facebookresearch/vrs/blob/main/vrs/TagConventions.h) for streams to represent a few common concepts.

## Records

Streams contain a time-sorted sequence of records. Records have the following metadata:

<Tabs>
  <TabItem value="timestamp" label="Timestamp" default>

Timestamps are `double` values that count seconds from a fixed point, such as the Unix epoch or system boot time.

<!-- prettier-ignore -->
:::note
All records in a VRS file must use timestamps in the same time domain. VRS indexes records by timestamp and plays them back in that order. If you need to preserve device-specific timestamps, such as those from cameras or IMUs, store them inside the records.
:::

<!-- prettier-ignore -->
Devices such as cameras and IMUs often have their own unsynchronized clocks or counters. Convert their timestamps to the common time domain before using them as VRS record timestamps.

<!-- prettier-ignore -->
:::caution
When recording audio, be aware that audio devices produce samples according to their own clock. If you use a system clock for your VRS records, the two clocks can drift apart. This can make it difficult to synchronize audio with other data, especially images.

You can align the first audio sample with the system clock when recording starts. After that, the sample count divided by the sample rate gives the elapsed time according to the audio device. Over a long recording, that value can differ noticeably from the elapsed time measured by the system clock.

<!-- prettier-ignore -->
Therefore, aligning the first sample alone does not guarantee that you can match later audio samples to images accurately.
:::

  </TabItem>
  <TabItem value="streamid" label="StreamId">

A `StreamId` is composed of a device type ID (`RecordableTypeId` enum value) and an instance ID, which is unique in the file for that device type ID. VRS generates instance IDs, so use stream tags or flavors, not instance IDs, to identify the same source across recordings.

  </TabItem>
  <TabItem value="recordtype" label="Record Type">

Records can be **Configuration**, **State** or **Data** records.

<Tabs>
  <TabItem value="configuration" label="Configuration Records" default>

**Configuration records** contain the settings to configure a device. For example, the resolution and exposure settings of a camera are stored in configuration records. Configuration records can also describe how an algorithm is configured.

The configuration of a device is expected to be somewhat stable (does not change at every moment). However, configuration records are not meant to store parameters that never change, such as serial numbers. For parameters that cannot change (such as serial numbers), use stream tags, which can be queried directly as soon as the VRS file is open for reading.

  </TabItem>
  <TabItem value="state" label="State Records">

**State records** store the state of a device. They can capture internal values from earlier calculations that affect an algorithm's behavior. For example, a vision algorithm might store the feature positions it is tracking at a given instant.

A device's state can change as it processes data. Rather than storing every change, record enough state to resume playback from an earlier point without replaying the entire session.

**Note:** Typically, state records are empty, because the internal state of a device is not commonly tracked.

  </TabItem>
  <TabItem value="data" label="Data Records">

**Data records** contain data that flows from a device or algorithm. For cameras, it is usually images. For an IMU device, it is usually a set of sensor samples. For a vision algorithm, it might be a pose.

The implementer of the device is responsible for the data format. Data records typically include metadata, such as internal counters, timestamps, and other measurements, often alongside a captured image or a block of audio samples.

Use `RecordFormat` and `DataLayout` to make your record format interoperable.

  </TabItem>
</Tabs>

  </TabItem>
  <TabItem value="recordformatversion" label="Record Format Version">

Use a record format version number (`uint32_t`) to tie the record to a record format.

`RecordFormatStreamPlayer` objects find the record's `RecordFormat` and `DataLayout` definitions in the stream's metadata using the record type and format version.

`StreamPlayer` objects can also be used to read any record, but they are responsible for interpreting its payload. This option can be useful when copying records "as is", without interpreting their data.

  </TabItem>
</Tabs>
