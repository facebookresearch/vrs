---
sidebar_position: 9
title: The vrs Command Line Tool
---

import Link from '@docusaurus/Link';

The `vrs` command-line tool lets you inspect VRS files, extract data, and make modified copies. See the [VRS build instructions](https://github.com/facebookresearch/vrs#getting-started) if you need to build it from source.

The VRS file format is designed to collect large volumes of sensor data efficiently and reliably in real time. The priorities are:

- recovery of complete records after an interrupted write.
- write to a single file, but file chunking is supported if requested.
- ability to access data quickly, avoiding indirections as much as possible.
- minimal amortized data processing, in particular, very efficient serialization.
- transparent lossless compression during capture when CPU capacity allows.
- compact storage that preserves data fidelity.
- huge file size support (as large as the filesystem will allow).
- simplicity and safety, to avoid data loss.

VRS files are not generally editable in place. Instead, the tool can copy records into a new file and apply transformations during the copy.

## File Inspection

To peek into a VRS file, simply run:

<!-- prettier-ignore -->
```
vrs vrs/oss/test_data/VRS_Files/sample_file.vrs
```

<!-- prettier-ignore -->
:::note
For file inspection, you can pass the file path as the first argument instead of the `details` command.
:::

A sample output is:

```
VRS file: 'vrs/oss/test_data/VRS_Files/sample_file.vrs', 81.1 KB.
Found 3 streams, 307 records, 301 data records from 1493865.291 to 1493866.378 (1.087s, 275.9rps).
  Tag: capture_time_epoch = 1636165950 -- Sat Nov  6 03:32:30 2021 CET
  Tag: os_fingerprint = MacOS 20.6.0
  Tag: purpose = sample_code
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

The output contains file information followed by a section for each stream.

### Part 1: Global File Information

<!-- prettier-ignore -->
```
VRS file: 'vrs/oss/test_data/VRS_Files/sample_file.vrs', 81.1 KB.
Found 3 streams, 307 records, 301 data records from 1493865.291 to 1493866.378 (1.087s, 275.9rps).
  Tag: capture_time_epoch = 1636165950 -- Sat Nov  6 03:32:30 2021 CET
  Tag: os_fingerprint = MacOS 20.6.0
  Tag: purpose = sample_code
```

- file chunks & sizes
- file tags
- count of streams: "Found 3 streams"
- count of records (all types): "307 records"
- count of data records: "301 data records"
- timestamp range of data records (if applicable): "data records from 1493865.291 to 1493866.378"
- time duration when data records are available (if applicable): "1.087s"
- data record rate in rps ("records per second", if applicable): "275.9rps"

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
- Original recordable type name: "Forward Camera Class"
- Stream instance ID: "#1"
- Stream flavor (if used): "sample/camera"
- Stream identifier: "200-1". `200` is the stream's `RecordableTypeId` numeric value, and `1` is the stream's instance ID.
- Stream VRS tags, which include the stream's `RecordFormat` and `DataLayout` definitions. Long tag values can be truncated. See the `record-formats` command below to print the definitions.
- Stream (user) tags.
- count of Configuration records, timestamp range and rps when there are more than one.
- count of State records, timestamp range and rps when there are more than one.
- count of Data records, timestamp range and rps when there are more than one.

To get the file description as JSON, use `json-description`. Use `json-pretty-description` for indented JSON.

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

This command lists each record with its timestamp, stream name and instance ID, stream identifier, record type, and uncompressed payload size. "Uncompressed" refers to lossless record compression (LZ4 or Zstandard), not image or video codec compression. The command does not interpret the payload. For instance:

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

When records have `RecordFormat` and `DataLayout` definitions, VRS can identify the types of content blocks in each payload and print metadata (`DataLayout`) blocks in a human-readable format or as JSON.

```
vrs print file.vrs
```

This command lists each record with its timestamp, stream name and instance ID, stream identifier, record type, `RecordFormat` definition, and uncompressed payload size. Indentation groups the content blocks under each record.

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

In the output above, `data_layout/size=8` and `data_layout/size=24+image/raw` are `RecordFormat` definitions. VRS prints each `DataLayout` field by name and value, and summarizes image and audio blocks.

A variation of this command provides more low-level details than most users need:

```
vrs print-details file.vrs
```

The output is similar, but the metadata blocks include field types, offsets, and sizes:

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

If you plan to parse the output with code, use JSON:

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

Each record starts with `{"record":{"timestamp":...`, followed by JSON lines for its content blocks. Use `vrs print-json-pretty file.vrs` for the same content with indented `DataLayout` blocks.

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

For stream `1100-7`, named "Polaris Camera" with instance ID 7, the format for version 1 data records is `data_layout/size=24+image/raw`. The `+` separates two content blocks: `data_layout/size=24` at index 0 and `image/raw` at index 1.

The line starting with `Content block 0` gives the `DataLayout` definition for the first block: a `uint64_t` field followed by two `double` fields. Together they always occupy 24 bytes before lossless compression.

It might be useful to examine `RecordFormat` definitions one stream at a time. This can be done easily using stream filtering (fully documented below):

```
vrs record-formats file.vrs + 1100-7
```

### Record Filtering

Because printing everything is rarely what you need (VRS files usually contain way too much data!), you are likely to want to restrict the range of records to print or "consider" for your operation, be it data printing, data extraction, copy, etc.

Most commands that process records accept the following filters.

#### Filtering by Timestamp

_Options: `--before [+|-]<max-timestamp>`, `--after [+|-]<min-timestamp>`, `--range [+|-]<min-timestamp> [+|-]<max-timestamp>`, `--around [+|-]<timestamp> <duration>`._

To print records with timestamps at or before the absolute timestamp `1493866`, use:

```
vrs print file.vrs --before 1493866
```

All timestamps and durations are in seconds. A `+` prefix gives an offset from the first data record in the selected streams; a `-` prefix gives an offset back from the last. When trimming with `copy`, the latest configuration and state records before the start of the range are also copied, unless those record types are filtered out.

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

The last argument to `--around` is the total duration. For example, to print one second of records centered on `1493866`, use:

```
vrs print file.vrs --around 1493866 1
```

#### Filtering by Record Type and by Stream

_Options: `+ [configuration|state|data]`, `- [configuration|state|data]`._

_Options: `+ <recordable_type_id>`, `- <recordable_type_id>`, `+ <recordable_type_id>-<instance_id>`, `- <recordable_type_id>-<instance_id>`._

<!-- prettier-ignore -->
:::important
Include a space between the `+` or `-` sign and the argument that follows.
:::

`+` includes a record type, all streams with a given recordable type ID, or a single stream specified by its type and instance IDs. `-` excludes them. For each category (record types or streams), a first `+` starts with nothing selected, and a first `-` starts with everything selected. For example, `+ configuration` selects only configuration records, while `- configuration` excludes configuration records.

The options apply from left to right. To include all streams with recordable type ID `1100` except instance `3`, use `+ 1100 - 1100-3`.

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

Decimation drops data records based on their timestamps. For example, you can keep at most one data record per second for a stream.

For instance, compare:

`vrs list vrs/oss/test_data/VRS_Files/simulated.vrs + 214-1`

to:

`vrs list vrs/oss/test_data/VRS_Files/simulated.vrs + 214-1 --decimate 214-1 1`.

Use `--bucket-interval` to group timestamps into regularly spaced buckets, with at most one data record kept per stream per bucket. `--bucket-max-delta` limits how far a record can be from the bucket timestamp and has no effect on its own. Use bucketing as an alternative to `--decimate`.

## Data Extraction

_Commands: `extract-images`, `extract-audio`, `extract-all`._

These commands extract images, audio, or metadata into files. Use `--to <folder_path>` to choose an output folder; otherwise, files are written to the current working directory.

For instance, to extract images from a VRS file into image files, use:

```
vrs extract-images file.vrs --to image-folder
```

Raw images are saved as PNG files by default, which can require a lossy pixel format conversion. Use `--raw-images` to save the original bytes in `.raw` files, with the image specifications in the filename. For example, `214-1-00099-147.293-bayer8_rggb-1408x1408-stride_1408.raw` means:

- `214-1` is the stream identifier,
- `00099` is the image number for this extraction run,
- `147.293` the image's timestamp,
- `bayer8_rggb` is the pixel format,
- `1408x1408` are the image dimensions,
- `stride_1408` means there are 1408 bytes per row.

`extract-audio` writes `.wav` files for each stream. It starts a new file if the audio format changes or the current file would reach the 4 GiB WAV limit.

`extract-all` extracts images, audio, and metadata. In `metadata.jsons`, each line is a separate JSON value: a file overview on the first line, then two lines per record (the record header and its content blocks). Parse the file line by line.

Extraction commands also support filters. For example, this command extracts images from the first 10 seconds of stream `200-1`, keeping images from data records at least one second apart.

```
vrs extract-images file.vrs --to image-folder + 200-1 --before +10 --decimate 200-1 1
```

## Copy Operation

_Commands: `copy`, `merge`._

### Copying Files

The `copy` command copies records from an existing VRS file into a new one. Use filters to select streams, trim the recording by timestamp, or decimate data records.

In its most basic form, a copy operation looks like this:

```
vrs copy original.vrs --to new.vrs
```

This will simply copy all the records from `original.vrs` and create a new VRS file named `new.vrs` that contains the same data. It is important to note that such a copy operation:

- processes all the records one after the other.
- compressed records are decompressed, and the new file is written using the selected lossless compression preset. By default, `copy` uses Zstandard at `zmedium` (level 7) on Apple silicon and `zlight` (level 3) elsewhere. LZ4 is an alternative when decompression speed matters more than compression ratio.
- records are written in timestamp order, and a new index is created.

Use `--compression=<level>` to choose `none` (no compression), `fast` or `tight` (LZ4), or `zfaster`, `zfast`, `zlight`, `zmedium`, `zheavy`, `zhigh`, `ztight`, or `zmax` (Zstandard). Use `--chunk-size <size>` to split the output into chunks; specify MiB with `M` or no suffix, or GiB with `G`.

To copy 10 seconds of a file with `zmedium` compression while skipping the first 2 seconds of data records, use:

```
vrs copy original.vrs --to new.vrs --range +2 +12 --compression=zmedium
```

<!-- prettier-ignore -->
:::important
Stream instance IDs may change during a copy. Check the output file for the new stream IDs.
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

Repeat `--file-tag` and `--stream-tag` to override multiple tags in the same command. For `--stream-tag`, use the stream ID in the output file.

##### Zero Files

Use `vrs copy --zero-vrs original.vrs --to new.vrs` to make a copy with recognized image and audio content blocks replaced by zeros. These payloads compress well but may no longer decode.

- the copy retains the metadata for inspection.
- zeroing is not anonymization: tags, metadata, and other payloads are not cleared.
- the copy can help debug issues unrelated to the original image or audio content.

### Merging Files

Both `copy` and `merge` accept multiple input files:

```
vrs copy original.vrs original2.vrs original3.vrs --to new.vrs
vrs merge original.vrs original2.vrs original3.vrs --to new.vrs
```

<!-- prettier-ignore -->
:::danger
Both commands combine input files into one VRS file, but they handle streams differently.
:::

All input timestamps must be in the same time domain. Neither command checks this or aligns the timestamps for you.

File tags are merged. If a tag has conflicting values, the first value keeps the original name. The other values are saved under names such as `name_merged`, then `name_merged-1` if needed.

When a file tag is explicitly overridden with `--file-tag`, conflicting input values for that tag are not preserved under `_merged` names. The explicit value is the only output value for that tag.

#### Merging Files with `copy`

With multiple inputs, **`copy` keeps the selected streams separate**, even when streams from different files have the same ID. For each recordable type, the output IDs follow the input file order, then the instance order within each input file.

Example:

```
File 1: 100-1, 100-2, 200-2
File 2: 100-1
File 3: 300-1, 200-1

Output: 100-1, 100-2, 100-3, 200-1, 200-2, 300-1
Source: File1  File1  File2  File1  File3  File3
```

The two streams of type `200` are ordered by input file, not by their original instance IDs.

#### Merging Files with `merge`

**`merge` combines streams by recordable type and their order within that type**, taking at most one stream from each input file for each output stream. Within a merged stream, it keeps only one record of a given type at each exact timestamp. Use this to rejoin segments of a recording with matching stream layouts.

Example:

```
File 1: 100-1, 100-2, 200-2
File 2: 100-1
File 3: 300-1, 200-1

Output: 100-1, 100-2, 200-1, 300-1
Source: File1  File1  File1  File3
          +             +
        File2         File3
```

Here, `200-2` from File 1 and `200-1` from File 3 are both the first stream of type `200` in their file, so they are merged into `200-1`.

## File Validation

_Commands: `check`, `decode`, `checksum`, `checksums`, `checksum-verbatim`, `hexdump`, `compare`, `compare-verbatim`._

### File Integrity Validation

The `check` command reads records and reports how many it could read and decompress. To also validate record formats and try decoding images, use `vrs decode file.vrs`. The `check` command verifies that:

- the file is recognized as VRS.
- the file has an index or an index was rebuilt (see the logs).
- records are found at the indexed offsets.
- records can be read.
- losslessly compressed record payloads can be decompressed.

```
vrs check file.vrs
```

Each index entry has corresponding metadata in the record header immediately before the payload. A mismatch or a decompression error can reveal corruption, but a successful check does not verify every byte of the payload.

To verify that a transfer preserved every byte, use `compare-verbatim`.

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

If the logical checksums differ, use `checksums` to find which parts differ.

Use `checksum-verbatim` to checksum the raw bytes of a file, whether or not it is a VRS file.

### Viewing Records

The `hexdump` command prints uncompressed record payloads in hexadecimal, with a short header per record. Use filters to limit the output.

### Comparing Files

You can compare files at the logical level, with the `compare` command.

```
vrs compare file1.vrs file2.vrs
```

The output reports whether the files are logically equivalent and describes any differences.

You can also use the `compare-verbatim` command to compare files at the file system byte level.

```
vrs compare-verbatim file1.vrs file2.vrs
```

## Advanced Options

_Commands: `fix-index`, `debug`, `rage`._

An interrupted recording may not have a valid index. VRS can rebuild the index in memory by scanning the recorded data, but that can be slow for large files. To save a rebuilt index, you have two options:

- copy the recording using the `vrs copy` command. The new recording will have a proper index.
- use `fix-index` to patch the original file in place. This can truncate incomplete data, and it requires a writable file. Neither method can recover a partially written record.

```
vrs fix-index file1.vrs
```

To produce enough information for a bug report about a file that's not working right, you use the command:

```
vrs rage file.vrs
```

This command prints the file details and detailed output for the first record of each type in each stream.

To inspect the file format structures and diagnostic information, use:

```
vrs debug file.vrs
```

This option is mostly meant for the VRS file format maintainers, and we mention it here for completeness.
