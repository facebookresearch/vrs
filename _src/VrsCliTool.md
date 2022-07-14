---
sidebar_position: 9
title: The vrs Command Line Tool
---

import Link from '@docusaurus/Link';

The `vrs` command line tool is a Swiss army knife utility to manipulate VRS files in different ways.

The VRS file format that is designed for very efficient and robust data collection of large volumes of sensor data in realtime. The priorities are:

- any data written to disk is "safe".
- write to a single file, but file chunking is supported if requested.
- ability to access data quickly, avoiding indirections as much as possible.
- minimal amortized data processing, in particular, very efficient serialization.
- transparent lossless data compression in realtime during capture, as much as possible (if possible at all, depending on CPU).
- data compactness and fidelity (no text to string conversions).
- huge file size support (as large as the filesystem will allow).
- simplicity and safety, to avoid data loss.

As a compromise, VRS file modifications are basically not possible. Instead, files can be copied, record-by-record, and during copies, certain transformations can be applied, but editing an existing VRS file in place is not a goal.

## File Inspection

To peek into a VRS file, simply run:

<!-- prettier-ignore -->
```
vrs vrs/oss/test_data/VRS_Files/sample_file.vrs
```

<!-- prettier-ignore -->
:::note
This is the only command that doesn't expect a command name to be specified as the first parameter.
:::

A sample output is:

```
VRS file: 'vrs/oss/test_data/VRS_Files/sample_file.vrs', 81.1 KB.
  Tag: capture_time_epoch = 1636165950 -- Sat Nov  6 03:32:30 2021 CET
  Tag: os_fingerprint = MacOS 20.6.0
  Tag: purpose = sample_code
Found 3 devices, 307 records, 301 data records from 1493865.291 to 1493866.378 (1.087s, 275.9rps).
102 Generic Audio Stream #1 - sample/audio [101-1] records.
  VRS Tag: RF:Data:1 = audio/pcm/int16le/channels=1/rate=44100
  VRS Tag: VRS_Original_Recordable_Name = Generic Audio Stream
  VRS Tag: VRS_Recordable_Flavor = sample/audio
  1 configuration record, at 1493865.286.
  1 state record, at 1493865.286.
  100 data records, from 1493865.298 to 1493866.378 (1.081s, 91.6rps).
103 Forward Camera Class #1 - sample/camera [200-1] records.
  VRS Tag: DL:Configuration:1:0 = {"data_layout":[{"name":"image_width","type":"DataPieceValue<uint32_t>","offset":0},{"name":"image_height","type":"D...
  VRS Tag: DL:Data:1:0 = {"data_layout":[{"name":"exposure_time","type":"DataPieceValue<uint64_t>","offset":0},{"name":"exposure","type":"DataPieceVal...
  VRS Tag: RF:Configuration:1 = data_layout
  VRS Tag: RF:Data:1 = data_layout/size=24+image/raw
  VRS Tag: VRS_Original_Recordable_Name = Forward Camera Class
  VRS Tag: VRS_Recordable_Flavor = sample/camera
  Tag: Position = Top
  1 configuration record, at 1493865.286.
  1 state record, at 1493865.286.
  101 data records, from 1493865.291 to 1493866.373 (1.082s, 92.46rps).
102 Motion Data Class #1 - sample/motion [372-1] records.
  VRS Tag: DL:Configuration:1:0 = {"data_layout":[{"name":"some_motion_stream_param","type":"DataPieceValue<double>","offset":0}]}
  VRS Tag: DL:Data:1:0 = {"data_layout":[{"name":"motion_data","type":"DataPieceVector<Matrix3Dd>","index":0}]}
  VRS Tag: RF:Configuration:1 = data_layout/size=8
  VRS Tag: RF:Data:1 = data_layout
  VRS Tag: VRS_Original_Recordable_Name = Motion Data Class
  VRS Tag: VRS_Recordable_Flavor = sample/motion
  1 configuration record, at 1493865.286.
  1 state record, at 1493865.286.
  100 data records, from 1493865.297 to 1493866.378 (1.081s, 91.61rps).
```

That's a lot of information, that can be overwhelming the first time you see what's in a VRS file!

### Part 1: Global File Information

<!-- prettier-ignore -->
```
VRS file: 'vrs/oss/test_data/VRS_Files/sample_file.vrs', 81.1 KB.
  Tag: capture_time_epoch = 1636165950 -- Sat Nov  6 03:32:30 2021 CET
  Tag: os_fingerprint = MacOS 20.6.0
  Tag: purpose = sample_code
Found 3 devices, 307 records, 301 data records from 1493865.291 to 1493866.378 (1.087s, 275.9rps).
```

- file chunks & sizes
- file tags
- count of streams: "found 3 devices"
- count of records (any types): "307 records"
- count of data records: "301 data records"
- data records timestamp range (if applicable): "data records from 1493865.291 to 1493866.378"
- time duration when data records are available (if applicable): "1.087s"
- data records rate in rps ("records per second", if applicable): "275rps"

### Part 2: Stream Information (Once per Stream)

<!-- prettier-ignore -->
```
103 Forward Camera Class #1 - sample/camera [200-1] records.
  VRS Tag: DL:Configuration:1:0 = {"data_layout":[{"name":"image_width","type":"DataPieceValue<uint32_t>","offset":0},{"name":"image_height","type":"D...
  VRS Tag: DL:Data:1:0 = {"data_layout":[{"name":"exposure_time","type":"DataPieceValue<uint64_t>","offset":0},{"name":"exposure","type":"DataPieceVal...
  VRS Tag: RF:Configuration:1 = data_layout
  VRS Tag: RF:Data:1 = data_layout/size=24+image/raw
  VRS Tag: VRS_Original_Recordable_Name = Forward Camera Class
  VRS Tag: VRS_Recordable_Flavor = sample/camera
  Tag: Position = Top
  1 configuration record, at 1493865.286.
  1 state record, at 1493865.286.
  101 data records, from 1493865.291 to 1493866.373 (1.082s, 92.46rps).
```

- Count of records in that stream: "103" ... "records."
- `RecordableTypeID` name: "Forward Camera Class"
- Stream instance ID: "#1"
- Stream flavor (if used): "sample/camera"
- Stream identifier: "200-1". `200` is the stream's `RecordableTypeId` numeric value, and `1` is the stream's instance ID.
- Stream VRS tags, which show, among other things, the stream's `RecordFormat` and `DataLayout` definitions in json form. These definitions are truncated if they are too long. See the `record-formats` sub-command below if you need complete definitions.
- Stream (user) tags.
- count of Configuration records, timestamp range and rps when there are more than one.
- count of State records, timestamp range and rps when there are more than one.
- count of Data records, timestamp range and rps when there are more than one.

All that information is presented in a human readable format, but you can get the same information in json form using:

```
vrs json-description file.vrs
```

## Data Exploration/Printing

### Basic Overview

_Commands: `list`._

To list records with their top-level metadata, simply run:

```
vrs list file.vrs
```

This command lists all the records with timestamp, stream name and instance ID, stream identifier, the record type, and the size of the uncompressed record payload. "Uncompressed" refers to the file format lossless compression using lz4 or zstd that might be happening at the record level, rather than image or video codec compression. Records are treated as a black box, so this command will work with any VRS file. For instance:

```
1493865.286 Motion Data Class #1 [372-1], Configuration record, 8 bytes total.
1493865.286 Motion Data Class #1 [372-1], State record, 0 bytes total.
1493865.286 Generic Audio Stream #1 [101-1], Configuration record, 0 bytes total.
1493865.286 Generic Audio Stream #1 [101-1], State record, 0 bytes total.
1493865.286 Forward Camera Class #1 [200-1], Configuration record, 44 bytes total.
1493865.286 Forward Camera Class #1 [200-1], State record, 0 bytes total.
1493865.291 Forward Camera Class #1 [200-1], Data record, 307224 bytes total.
...
```

## Taking Advantage of `RecordFormat` and `DataLayout`

_Commands: `print`, `print-details`, `print-json`, `print-json-pretty`._

When records are represented using `RecordFormat` and `DataLayout`, it is possible to interpret a record's payload. VRS can recognize the record's successive content blocks, their type, and print metadata blocks otherwise known as `DataLayout` blocks in a human readable form, or in json.

```
vrs print file.vrs
```

This command lists all the records with timestamp, stream name and instance ID, stream identifier, the record type, the record's `RecordFormat` definition, and the size of the uncompressed record payload. This format is meant to be most human readable, even if it requires a little getting used to. Notice that the text indentation to help group information together.

For instance:

```
1493865.286 Motion Data Class #1 [372-1], Configuration record, data_layout/size=8 = 8 bytes total.
 - DataLayout:
     some_motion_stream_param: 25
1493865.286 Motion Data Class #1 [372-1], State record, <no RecordFormat definition> = 0 bytes total.
1493865.286 Generic Audio Stream #1 [101-1], Configuration record, <no RecordFormat definition> = 0 bytes total.
1493865.286 Generic Audio Stream #1 [101-1], State record, <no RecordFormat definition> = 0 bytes total.
1493865.286 Forward Camera Class #1 [200-1], Configuration record, data_layout = 44 bytes total.
 - DataLayout:
     image_width: 640
     image_height: 480
     image_pixel_format: grey8 (1)
     camera_calibration:  23 53 343 3 12 8
1493865.286 Forward Camera Class #1 [200-1], State record, <no RecordFormat definition> = 0 bytes total.
1493865.291 Forward Camera Class #1 [200-1], Data record, data_layout/size=24+image/raw = 307224 bytes total.
 - DataLayout:
     exposure_time: 47390873
     exposure: 2.5
     frame_counter: 0
     camera_temperature: 38.5
 - Image block, image/raw/640x480/pixel=grey8, 307200 bytes.
1493865.297 Motion Data Class #1 [372-1], Data record, data_layout = 8 bytes total.
 - DataLayout:
     motion_data:
1493865.298 Generic Audio Stream #1 [101-1], Data record, audio/pcm/int16le/channels=1/rate=44100 = 512 bytes total.
 - Audio block, audio/pcm/int16le/channels=1/rate=44100/samples=256, 512 bytes.
 ...
```

In the output above, `data_layout/size=8` and `data_layout/size=24+image/raw` are `RecordFormat` definitions. When records have such a definition, then their content is printed out, depending on the content block types: `DataLayout` content blocks, aka, metadata blocks, are printed field by field, with name and values. An overview of image blocks and audio blocks are printed.

A variation of this command provides more low-level details than most users need:

```
vrs print-details file.vrs
```

The output is similar, but notice how the metadata blocks contain more information, field types, field block offsets and field sizes:

```
1493865.286 Motion Data Class #1 [372-1], Configuration record, data_layout/size=8 = 8 bytes total.
 - DataLayout:
   1 fixed size pieces, total 8 bytes.
     some_motion_stream_param (double) @ 0+8 Value: 25
1493865.286 Motion Data Class #1 [372-1], State record, <no RecordFormat definition> = 0 bytes total.
1493865.286 Generic Audio Stream #1 [101-1], Configuration record, <no RecordFormat definition> = 0 bytes total.
1493865.286 Generic Audio Stream #1 [101-1], State record, <no RecordFormat definition> = 0 bytes total.
1493865.286 Forward Camera Class #1 [200-1], Configuration record, data_layout = 44 bytes total.
 - DataLayout:
   3 fixed size pieces, total 20 bytes.
     image_width (uint32_t) @ 0+4 Value: 640
     image_height (uint32_t) @ 4+4 Value: 480
     image_pixel_format (uint32_t) @ 8+4 Value: grey8 (1)
   1 variable size pieces, total 24 bytes.
     camera_calibration (vector<float>) @ index: 0, count: 6
       Values: 23 53 343 3 12 8
1493865.286 Forward Camera Class #1 [200-1], State record, <no RecordFormat definition> = 0 bytes total.
1493865.291 Forward Camera Class #1 [200-1], Data record, data_layout/size=24+image/raw = 307224 bytes total.
 - DataLayout:
   4 fixed size pieces, total 24 bytes.
     exposure_time (uint64_t) @ 0+8 Value: 47390873
     exposure (float) @ 8+4 Value: 2.5
     frame_counter (uint64_t) @ 12+8 Value: 0
     camera_temperature (float) @ 20+4 Value: 38.5
 - Image block, image/raw/640x480/pixel=grey8, 307200 bytes.
1493865.297 Motion Data Class #1 [372-1], Data record, data_layout = 8 bytes total.
 - DataLayout:
   1 variable size pieces, total 0 bytes.
     motion_data (vector<Matrix3Dd>) @ index: 0, count: 0
1493865.298 Generic Audio Stream #1 [101-1], Data record, audio/pcm/int16le/channels=1/rate=44100 = 512 bytes total.
 - Audio block, audio/pcm/int16le/channels=1/rate=44100/samples=256, 512 bytes.
...
```

Interpretation of `image_width (uint32_t) @ 0+4 Value: 640`:

- `image_width` is the field's name,
- `uint32_t` is the field's type,
- `@ 0+4` means the data starts at byte offset 0 and uses 4 bytes (the size of `uint32_t`),
- `640` is the field's actual value.

If you plan on parsing the output with code, then json is probably a better format. You can use:

```
vrs print-json file.vrs
```

Output:

```
{"record":{"timestamp":1493865.286,"device":"372-1","type":"Configuration","size":8}}
{"data_layout":[{"value":25.0,"name":"some_motion_stream_param","type":"Value<double>"}]}
{"record":{"timestamp":1493865.286,"device":"372-1","type":"State","size":0}}
{"record":{"timestamp":1493865.286,"device":"101-1","type":"Configuration","size":0}}
{"record":{"timestamp":1493865.286,"device":"101-1","type":"State","size":0}}
{"record":{"timestamp":1493865.286,"device":"200-1","type":"Configuration","size":44}}
{"data_layout":[{"value":640,"name":"image_width","type":"Value<uint32_t>"},{"value":480,"name":"image_height","type":"Value<uint32_t>"},{"value":1,"name":"image_pixel_format","type":"Value<uint32_t>"},{"value":[23.0,53.0,343.0,3.0,12.0,8.0],"name":"camera_calibration","type":"Vector<float>"}]}
{"record":{"timestamp":1493865.286,"device":"200-1","type":"State","size":0}}
{"record":{"timestamp":1493865.291,"device":"200-1","type":"Data","size":307224}}
{"data_layout":[{"value":47390873,"name":"exposure_time","type":"Value<uint64_t>"},{"value":2.5,"name":"exposure","type":"Value<float>"},{"value":0,"name":"frame_counter","type":"Value<uint64_t>"},{"value":38.5,"name":"camera_temperature","type":"Value<float>"}]}
{"image":"image/raw/640x480/pixel=grey8"}
{"record":{"timestamp":1493865.297,"device":"372-1","type":"Data","size":8}}
{"data_layout":[{"name":"motion_data","type":"Vector<Matrix3Dd>"}]}
{"record":{"timestamp":1493865.298,"device":"101-1","type":"Data","size":512}}
{"audio":"audio/pcm/int16le/channels=1/rate=44100/samples=256"}
...
```

Notice that every record starts with `{"record":{"timestamp":...`, follow by a number of content blocks lines. Because `DataLayout` blocks are printed in a single line json blob, they can be difficult for a human to read. Use `vrs print-json-pretty file.vrs` to get the same content, but with `DataLayout` blocks printed out using indented json.

```
{"record":{"timestamp":1493865.286,"device":"372-1","type":"Configuration","size":8}}
{
    "data_layout": [
        {
            "value": 25.0,
            "name": "some_motion_stream_param",
            "type": "Value<double>"
        }
    ]
}
{"record":{"timestamp":1493865.286,"device":"372-1","type":"State","size":0}}
...
```

All the commands above rely on the `RecordFormat` and `DataLayout` definitions embedded in your file. Each stream has a copy of the definitions needed to interpret its own records. To print these definitions, use:

```
vrs record-formats file.vrs
```

...which will show something like:

```
...
1100-7 Polaris Camera #7 Data v1: data_layout/size=24+image/raw
Content block 0: {
    "data_layout": [
        {
            "name": "hal_frame_counter",
            "type": "Value<uint64_t>"
        },
        {
            "name": "hal_exposure_time",
            "type": "Value<double>"
        },
        {
            "name": "hal_arrival_time",
            "type": "Value<double>"
        }
    ]
}
...
```

Which means: For the stream `1100-7` named "Polaris Camera", stream instance ID 1, the record format definition for data records version 1 is `data_layout/size=24+image/raw`. A plus sign '+' separates successive content blocks, so here, we have two content blocks. Content block index 0 is `data_layout/size=24`, and content block index 1 is `image/raw`.

The line starting with `Content block 0` provides the `DataLayout` definition of the content block index 0, which has 3 values, an `uint64_t` field, followed by two `double` fields, for a total of 24 bytes. So no matter what the record's content is, the first content block will always use 24 bytes (before lossless compression), which is why that content block's definition is `data_layout/size=24`.

It might be useful to examine `RecordFormat` definitions one stream at a time. This can be done easily using stream filtering (fully documented below):

```
vrs record-formats file.vrs + 1100-7
```

### Record Filtering

Because printing everything is rarely what you need (VRS files usually contain way too much data!), you are likely to want to restrict the range of records to print or "consider" for your operation, be it data printing, data extraction, copy, etc.

Specifying record filters options as additional parameters works with most commands for which it makes sense.

#### Filtering by Timestamp

_Options: `--before [+|-]<max-timestamp>`, `--after [+|-]<min-timestamp>`, ` --range [+|-]<min-timestamp> [+|-]<max-timestamp>`, `--around [+|-]<timestamp> <time-range>`._

To only print records which timestamps is less than the absolute timestamp `1493866`, do:

```
vrs print file.vrs --before 1493866
```

Because timestamps vary from file to file, there are shortcuts to compute timestamps relative to the file's first and last data records. By convention, timestamps specified with a '+' sign are relative to the first data record's timestamp. Timestamps specified with a '-' sign are relative to the last data record's timestamp.

Therefore, to print records up to 1 second after the first data record, do:

```
vrs print file.vrs --before +1
```

To skip the first second and the last second of data records, use:

```
vrs print file.vrs --after +1 --before -1
```

...or:

```
vrs print file.vrs --range +1 -1
```

To focus around a particular timestamp, for instance, to print 1 second of records around a particular timestamp:

```
vrs print file.vrs --around 1493866 1
```

#### Filtering by Record Type and by Stream

_Options: `+ [configuration|state|data]`, `- [configuration|state|data]`._

_Options: `+ <recordable_type_id>`, `- <recordable_type_id>`, `+ <recordable_type_id>-<instance_id>`, `- <recordable_type_id>-<instance_id>`._

<!-- prettier-ignore -->
:::important
Notice the space between the `+` or `-` sign, and the argument that follows!
:::

`+` options allow you to include a particular record type, all the streams with a particular recordable type ID, or a stream fully identified by recordable type ID and instance ID. `-` options allow you to exclude a particular record type, all the streams with a particular recordable type ID, or the stream fully identified by recordable type ID and instance ID. If the first of the options starts with a `+`, it is assumed nothing was included before for that category (record type or streams), whereas if the first option is `-`, it is assumed that all the elements for that category is the starting set. In other words `+ configuration` will filter out all the record types except configuration records, whereas `- configuration` will filter out all configuration records.

These options function in accumulation, left to right, so you can include all the stream with the recordable type ID `1100` except the one with the instance ID `3` by doing: `+ 1100 - 1100-3`.

#### First Records Filter

_Option: `--first-records`._

For debugging purposes, it can be very convenient to only print the information relating to the first record of each type of each stream. For instance, to get an idea of what every stream in a file looks like:

```
vrs print file.vrs --first-records
```

Of course, you can combine this option with stream filtering:

```
vrs print file.vrs --first-records + 1100-7
```

#### Decimation (Advanced)

_Options: `--decimate <recordable_type_id>[-<instance_id>] <timestamp_interval>`, `--bucket-interval <timestamp_interval>`, `--bucket-max-delta <timestamp_delta>`._

The decimation options let your drop data records based on their timestamp relative to other records. For instance, you might only want at most one data record per second for a particular stream, dropping many records, as necessary.

For instance, compare:

`vrs list vrs/oss/test_data/VRS_Files/simulated.vrs + 214-1`

to:

`vrs list vrs/oss/test_data/VRS_Files/simulated.vrs + 214-1 --decimate 214-1 1`.

The options `--bucket-interval` and `--bucket-max-delta` work together, and work across all streams. Use `--bucket-interval` to create "buckets" at regular intervals throughout the file, from which at most one record per stream will be kept. Add the `--bucket-max-delta` to restrict the size of these buckets (this option doesn't work alone).

## Data Extraction

_Commands: `extract-images`, `extract-audio`, `extract-all`._

These commands let you extract images, audio, and metadata into files. Use the `--to <folder_path>` to specify a destination folder where the data is to be extracted, or it will be added to the current working directory.

For instance, to extract images from a VRS file into image files, use:

```
vrs extract-images file.vrs --to image-folder
```

Note that images in RAW format will be saved as png files, and that this operation might require a lossy pixel format conversion. To avoid that, use the `--raw-images` option, which will make RAW images be saved in `.raw` files, with a name that will provide the actual image specifications. Such a name might be `214-1-00099-147.293-bayer8_rggb-1408x1408-stride_1408.raw`, where:

- `214-1` is the stream identifier,
- `00099` the image number in that stream,
- `147.293` the image's timestamp,
- `bayer8_rggb` is the pixel format,
- `1408x1408` are the image dimensions,
- `stride_1408` tells that there are 1408 bytes of data per line.

`extract-audio` will extract audio data in .wav files, one per stream.

`extract-all` will extract images, audio data, and metadata. The metadata is extracted in a single .jsons file that contains a succession of json messages, one per line. Each line corresponds to a single record, in timestamp order, so it is possible to parse it even if the number of records is huge. Saving all the data in a single file prevents saturating your disk with possibly millions of small files.

Of course, the filtering options work with the extract commands, so you can extract only some of the streams, or only a few seconds of recording, etc. For instance, the following command will extract images from the `200-1` stream only, that are in the 10 first seconds of that stream, and at most 1 image per second of data.

```
vrs extract-images file.vrs --to image-folder + 200-1 --before +10 --decimate 200-1 1
```

## Copy Operation

_Commands: `copy`, `merge`._

### Copying Files

The primary use of the `copy` command is to copy records from an existing VRS into a new VRS file. Use the filter options in conjunction with the copy command to selectively copy parts of a file into a new one. With the filters you can do things like select a subset of streams, truncate a file by timestamp, or decimate a file.

In its most basic form, a copy operation looks like this:

```
vrs copy original.vrs --to new.vrs
```

This will simply copy all the records from `original.vrs` and create a new VRS file named `new.vrs` that contains the same data. It is important to note that such a copy operation:

- processes all the records one after the other.
- at the lowest level, all the records are decompressed, then recompressed in the new file, maybe using a different lossless compression setting. Commonly, during capture, files are compressed only very lightly, because data is generated too quickly and/or enough CPU bandwidth isn't available to compress more. During a copy operation, by default, `zstd` is used at the "zlight" level (`zstd` level "3"), but you can select a different lossless compression setting, such as "zmedium" (`zstd` level 5), or "ztight" (`zstd` level 18). Note that if decompression speed is critical for you, you can use `lz4` instead of `zstd`, at the expense of compression ratio.
- the file is "cleaned-up", records are sorted if they were out of order, and an optimized index is created, possibly making the file slightly more efficient to read.

To control the lossless compression applied at the record level, use the `--compression=<level>` option, where `<level>` is one of `none`, `fast`, `tight`, `zfast`, `zlight`, `zmedium`, `ztight`, `zmax`. The compression levels which name starts with a `z` use `zstd`, while the others use `lz4`. In practice, `zlight` or `zmedium` are likely to be your best options, because `ztight` and `zmax` usually offer marginal size gains while being too slow to be practical.

To copy 10 seconds of a file with `zmedium` compression while skipping the first 2 seconds of data records, use:

```
vrs copy original.vrs --to new.vrs --range +2 +12 --compression=zmedium
```

<!-- prettier-ignore -->
:::important
Stream instance ID numbers might not be preserved in the output VRS file. Stream instance IDs are not guaranteed during file creation, and they are not necessarily preserved during copy operations.
:::

### Copying Files While Modifying Data

#### Overwriting Tags

VRS file content modifications during copies are possible, but only simple modifications are offered by the `vrs` command line tool.

Overwriting file and stream tags can be very useful.

`--file-tag <tag_name> <tag_value>` will let you set or overwrite a file tag, while `--stream-tag <stream-id> <tag_name> <tag_value>` will let you set or overwrite a stream tag. For instance:

```
vrs copy original.vrs --to new.vrs --file-tag "date" "April 1, 2022"
vrs copy original.vrs --to new.vrs --stream-tag 1100-1 "position" "top_left"
```

You can set or overwrite as many file and stream tags as you need, by using the `--file-tag` and `--stream-tag ` options as many times as you need in the same command.

##### Zero Files

The `--zero` option lets you copy all the data. Clearing all image and audio data with zeros will dramatically reduce the size of files that have this type of data. This is mainly due to the lossless compression at the record level. Zeroed files can be over 99% smaller than the original files. This can be useful for several reasons:

- you now have a lightweight version of a file, that contains all its metadata.
- since images and audio data are cleared, the copy might be anonymized, should there be no sensitive data in the metadata of the file.
- this file is identical to the original, except for the image and audio data (obviously), which can make it convenient to share over the internet to debug issues not related to the image and audio data.

### Merging Files

The `copy` and `merge` command let you merge files together, simply by specifying more than one input files:

```
vrs copy original.vrs original2.vrs original3.vrs --to new.vrs
vrs merge original.vrs original2.vrs original3.vrs --to new.vrs
```

<!-- prettier-ignore -->
:::danger
Attention, while both `copy` and `merge` allow you to merge multiple files into a single output VRS file, their behavior is very different!
:::

Because VRS files must have timestamps in a single time domain, it is assumed that all the files merged are using the same time domain, but effectively, no validation is performed for you: you need to know what you're doing!

File tags are merged. In case of name collision and the values are different, the first instance "wins". The other values of the tag are preserved, by adding a tag with a different name: "name" becomes "name_merged", then "name_merged-1" if there are more conflicting values, etc.

#### Merging Files with `copy`

When handling multiple source files, the **`copy` operation copies all the streams side-by-side** in the output file, so that the number of streams in the output file will always be the sum of the number of streams in all the input files, even if two or more streams in input files have the same stream identifier. When streams with the same recordable type ID are copied in a single file, the stream instance IDs will be ordered so that the streams with a particular recordable type ID from the first file will have the lowest instance IDs, in the same order they had in their source file, then come the streams from the next file, etc.

Example:

```
File 1: 100-1, 100-2, 200-2
File 2: 100-1
File 3: 300-1, 200-1

Output: 100-1, 100-2, 100-3, 200-1, 200-2, 300-1
Source: File1  File1  File2  File1  File3  File3
```

Notice how the "200" stream are ordered based on their original file, not their original instance ID.

#### Merging Files with `merge`

The **`merge` operation merges streams with the same recordable type ID, one per file, into one stream, in sequence** in the output file. Records with the exact same timestamp are deduplicated, by keeping only the record from the earliest file. This behavior is usually only needed when files have been truncated by timestamp and they are merged together again.

Example:

```
File 1: 100-1, 100-2, 200-2
File 2: 100-1
File 3: 300-1, 200-1

Output: 100-1, 100-2, 200-1, 300-1
Source: File1  File1  File2  File3
          +             +
        File2         File3
```

Notice how the "200" stream are now ordered based on their original file, not their original instance ID.

## File Validation

_Commands: `check`, `checksum`, `checksums`, `checksum-verbatim`, `hexdump`, `compare`, `compare-verbatim`._

### File Integrity Validation

The `check` command "simply" decodes every record in the VRS file and prints how many records were decoded successfully. It proves that the VRS file is correct at the VRS level.

- the file is actually a VRS file (!)
- the file has an index or it was rebuilt (see the logs to tell)
- the records are found where they were expected
- the records were read successfully
- the records were decompressed successfully (VRS-internal lossless decompression only).

```
vrs check file.vrs
```

Records are all referenced in the file's index. They also have a header in the file just before the actual record payload. These two pieces of metadata must match otherwise an error is reported. Records are usually losslessly compressed with lz4 or zstd when record size exceeds a certain number (around 200 bytes). This provides some level of corruption detection, because compressed payloads have an internal checksum.

This type of file corruption rarely happens, and can usually be traced back to a corrupt file transfer.

### Checksums

A logical data checksum can be generated for a VRS file, which will checksum:

- the file tags
- the sequence of streams for each recordable type ID, but not their actual instance ID
- the stream tags, both VRS tags and user tags
- the metadata of all the records
- the payload of all the records.

```
vrs checksum file.vrs
```

The intent is that you can copy a VRS file using the `copy` command, change the lossless compression level, and though the files are very different at the file system level, they will still be identical from a VRS logical content and checksum standpoint.

If two files do not have the same checksum, use the `checksums` command to have separate checksums, which should reveal which parts are different.

If you can checksum any file at the file system level, regardless of whether the file is a VRS file or not, using the `checksum-verbatim` command.

### Viewing Records

If you are curious about how records look like inside a VRS file, you can use the `hexdump` command. This will print record payloads, uncompressed, in hexadecimal, after a minimal header per record. You will probably want to combine this option with the filtering options, to limit the size of the output.

### Comparing Files

You can compare files at the logical level, with the `compare` command.

```
vrs compare file1.vrs file2.vrs
```

The output will tell you whether the files are identical, and if they are not, which part(s) aren't.

You can also use the `compare-verbatim` command to compare files at the file system byte level.

```
vrs compare-verbatim file1.vrs file2.vrs
```

## Advanced Options

_Commands: `fix-index`, `debug`, `rage`._

If a VRS recording was interrupted during creation, the VRS file probably won't have a proper index, because it is written to disk last at the end of the file. Maybe the recording app crashed, or the system ran out of disk space. By design, this situation is ok, and whatever was written is "safe": an index will be rebuilt by scanning the file, discovering records and their metadata by walking the list of daisy-chained records. But this process can be slow if the file is large and/or the disk slow. In order to "fix" such files, you have two options:

- copy the recording using the `vrs copy` command. The new recording will have a proper index.
- use the `fix-index` command, to patch the original file in place. The command might truncate the end of the file to get rid of incomplete data, which is usually simply the last record what wasn't fully written out, then the rebuilt index will be written out at the end of the file, provided there is enough disk space for it. Because the original recording file is modified, it is a bit riskier, but much faster, than copying the file. Note that normally, the resulting file contains the same data, as partially written records can't be recovered by VRS either way.

```
vrs fix-index file1.vrs
```

To produce enough information for a bug report about a file that's not working right, you use the command:

```
vrs rage file.vrs
```

This command simply produces the combined information of `vrs details` and `vrs print-details file.vrs --first-records` in one easy to remember command.

To check some VRS internal data structures and display some diagnostic information about a VRS file, use:

```
vrs debug file.vrs
```

This option is mostly meant for the VRS file format maintainers, and we mention it here for completeness.
