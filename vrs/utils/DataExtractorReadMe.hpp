/* Markdown text included in the ReadMe.md file when extracting a VRS file. */

R"====(# VRS File Extraction Data Format

## Folder Structure Overview

* `ReadMe.txt`: this file, for humans to read.
* `metadata.jsons`: Metadata file.
* `NNNN-MM` folders: image folders, one folder per stream containing images.

## `ReadMe.txt`

This file is a human readable description of how the data was extracted, with high-level description of the data format, and how to interpret the data.

## `metadata.jsons`

### Overall structure

This file contains the VRS file’s metadata. Because the data in VRS files doesn’t have a fixed format, which might evolve over time, a CSV file isn’t appropriate. Because the metadata file is usually huge (easily over 1 GB), exporting the data as a single json message would be very difficult to write and read/parse. So we’re using an intermediate approach, with combines the benefits of both CSV and json formats.
Each line of the `metadata.jsons` file is guaranteed to be a self-contained json message, which should be parsed independently. Therefore, reading this file can be done like so, in pseudo code:

```
for each line in file # using platform dependent line delimitors
  dic = json_parse(line)
```

### First line: the file’s metadata

The first line of `metadata.jsons` is a top-level description of the file. It’s basically the same information at the end of the `ReadMe.txt` file, below the explanations, but in json format. So humans should read the `ReadMe.txt` file, while code should read the `metadata.jsons` instead. There are minor differences, which are called out below. Here are the different parts:

* the file name and size.
* the file’s tags, which are pairs of strings, with a tag name and a tag value. The tag values may or may not be json messages, but they are always presented at a text string, even when they are really numbers or json messages.
* metadata for the file’s content, including:
    * a count of streams
    * a count of records
    * a range of timestamps for the whole file.
* For each stream in the file:
    * metadata for the stream, including:
        * a device name
        * a device type id, a positive integer, which is the first part of the stream’s id.
        * a device instance id, a positive integer, which is the final part of the stream’s id.
        * a device flavor (optional)
        * device tags, which are also pairs of string, and work the same as file tags.
        * “vrs_tags”, which are VRS internal data descriptions of the records’ data formats, for that stream.
            These tags can be very verbose and obscure, and are truncated when printed for human consumption. But they can provide insight into the records’ metadata formats (use the json version).
        * per record type, a count of records with a timestamp range

### Next lines: the file’s records in pairs of two json messages

The rest of the `metadata.jsons` file contains the data stored in the VRS file’s records, using two json lines per record.
The first line holds the record’s VRS-level metadata.
Example: `{"stream":"283-3","type":"Data","timestamp":2392.14314}`
That json message contains only 3 fields:

* `stream`: the id of the stream this record belongs to. It’s a code made of the device type id and the device instance id separated by the dash. For instance, if the device type id is `283`, and the device instance id is `3`, the stream’s id is `283-3`.
* `type`: the type of record, either `configuration`, `state`, or `data`. See the VRS File Concepts section for details.
* `timestamp`: the record’s timestamp.

The second line hold’s the record’s payload, as json as well, of course. It contains a single value named `content`, which is an array of json messages, each describing a content block.
Each content block is a single value, which names describes the type of block at hand:

* `metadata`: the content block contains a variable set of parameters, each with a text name and a typed value. Sample:
    `{"metadata":[{"name":"nominal_rate","value":10.0},{"name":"image_width","value":640}, ...`
    The specific set of parameters depends on the device, and is application domain dependent, so they can’t be documented in this generic file format documentation.
* `image`: the content block contains an image, which format is usually described in an earlier `medatada` block in the same record, or in the stream’s last `configuration` record. The only value in the json tells the filename in which the image data is stored, for instance: `{"image":"283-3-00001-2392.289.jpg"}`. Images are stored in a folder reserved for that stream, and named after the stream id, in this case `283-3`, so the file is effectively at `283-3/283-3-00001-2392.289.jpg`. The file names are designed to be unique to avoid confusion, which is why the stream’s id included. Then there is an image sequence number, followed by the image’s timestamp in ms, and the file’s type extension.
* `audio` and `custom` content blocks are currently not supported for export, but they may appear. You’ll have to ignore them.

By separating the record’s metadata from the record payload, it is trivial to skip irrelevant records that should be filtered out without having to parse possibly complex json messages. The file parser will simply need to read the line & skip it.

**Attention:** The json messages may use the `NaN` constant for double values, which is not part of the strict json standard, but [can be enabled in most implementations](https://stackoverflow.com/questions/6601812/sending-nan-in-json).

## Basic VRS File Concepts

*VRS is a file format optimized to record & playback streams of sensor data, such as images, audio samples, and any other discrete sensors (IMU, temperature, etc), stored in per-device streams of timestamped records.
The name comes from “Vision Replay System” because it was built by a team named the “vision team” at the time.*

VRS files contain multiple “streams”, each for a specific sensor device, typically, one stream per sensor (a camera, an IMU, etc). There might be multiple streams of the same type (eg, multi-cameras device), which is why streams are identified using a pair of device type id and device instance id. The VRS file and each stream can be decorated by “tags”, which are simply string name-value pairs. Tag names are meant to be simple identifiers, but tag values can be string of any kind, usually using the utf-8 encoding.

For each stream, there are 3 types of records: Configuration, State and Data records.

**Configuration records** are meant to describe how the device is configured. Example: the resolution & exposure settings of a camera. The configuration of a device is expected to be pretty stable. A configuration record is usually required at the start of each stream.
**State records** are meant to store the internal state of a device, for debugging purposes. They are rarely used.
**Data records** are meant to represent the data that flows out of the device, for instance, camera images, and/or other sensor information.

Configuration, State, and Data records are all stored the same way. They are all an ordered succession of **content blocks**. The main types of content blocks are `metadata`, `image`, `audio` and `custom`, as described above.
Typically, configuration records contain one block of metadata, while state records are empty (you’ll typically find one next to configuration records). Data records most often contain a `metadata` block, often followed by an `image` block or an `audio` block.
Because VRS files are designed-built to collect sensor data, the format of each of the streams’ records are usually perfectly stable for that file/stream/record type. In other words, the data records of each stream will look the same throughout the file (same metadata structure, same succession of custom blocks), but of course, the data records of a camera stream and of an IMU sensor stream will look very different.
VRS is designed to record & playback data as it was received by the VRS API, which is why VRS has no intrinsic notion of framerate. In other words, if a camera goes silent for a while, then produces a burst of 100 images in 1 ms, that is what VRS will record and playback.

## VRS File Metadata (human readable form)
)===="
