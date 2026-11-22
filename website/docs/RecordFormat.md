---
sidebar_position: 6
title: Record Format
---

import Tabs from '@theme/Tabs'; import TabItem from '@theme/TabItem';

## Record Format Version

Each record in a stream has its own format version number, which is a `uint32_t` value. However, because records belong to a single stream and each has a record type (Configuration, State, or Data), format version numbers are only meaningful within that stream and for that record type. You do not need to worry about format version collisions between streams and record types.

Before `RecordFormat` was available, record format versioning was critical, because it was the only information about how the record's data was formatted. In the `StreamPlayer` callbacks you received when reading a file, you were responsible for interpreting every byte of data and you also had to manually manage all data format changes. Since record data formats were not self-described within the file, each time you needed to add, remove, or change a field, you had to change the format version and handle a growing number of format versions explicitly in your code. This was unmanageable. Moreover, it was not possible to write standard tools that could interpret what was stored in records or display their images and other metadata.

`RecordFormat` and `DataLayout` were designed to solve these challenges. Changes to `DataLayout` alone rarely require a new record format version. `RecordFormat` structures records as a succession of typed content blocks, embedding descriptions, including `DataLayout` definitions, in the stream itself. VRS uses these embedded descriptions to interpret records, calculate content block boundaries using DataLayout Conventions, and send parsed content blocks to `RecordFormatStreamPlayer` callbacks when reading a VRS file. With `RecordFormat` and the DataLayout Conventions, it is now possible to write generic tools like `vrsplayer` that let you explore a VRS file without knowing how it was recorded.

## `RecordFormat`

Use `RecordFormat` to describe records as a sequence of typed content blocks. This structure applies to configuration, state, and data records alike.

### `ContentBlock`

The content block types are: `image`, `audio`, `data_layout`, and `custom`. VRS generates and parses a compact string for each `RecordFormat` definition. Content block descriptions may contain additional information specific to the content type. Here are some examples of single content block `RecordFormat` definitions:

- `image`
- `image/png`
- `image/jpg`
- `image/jxl`
- `image/raw`
- `image/raw/640x480/pixel=rgb8`
- `image/raw/640x480/pixel=grey8/stride=648`
- `image/custom_codec/codec=my_experiment`
- `image/video`
- `image/video/codec=H.264`
- `audio`
- `audio/pcm`
- `audio/pcm/uint24be/rate=32000/channels=1`
- `data_layout`
- `data_layout/size=48`
- `custom`
- `custom/format=my_own_payload_format`
- `custom/size=160`

`image` and `audio` content blocks are pretty much what you expect when you read their text description. `data_layout` blocks contain structured metadata. `custom` content blocks contain raw data in a format you are responsible for interpreting.

You can assemble as many content blocks as you like in a record, which might look like this:

- `data_layout+image/raw`
- `data_layout+data_layout+audio/pcm`

Again, these text descriptions are generated and parsed for you, so you don't need to worry about their syntax.

The `RecordFormat` and `DataLayout` descriptions of a stream's records are stored in the VRS tags of the stream. You will only see these text descriptions when you are using tools to dump a stream's VRS tags. VRS tags are associated with each stream for VRS internal usage, and are kept separate from the user stream tags.

Common VRS record formats include:

- `data_layout`: for records containing a single metadata content block, which is typical of configuration records.
- `data_layout+image/raw`: for records containing some image-specific metadata and the raw pixel data of an image.
- `data_layout+image/jpg` and `data_layout+image/video`: for records containing some image-specific metadata and compressed image data.

### Datalayout Content Blocks

Datalayout content blocks, commonly referred to as datalayouts, are `DataLayout` objects that hold a collection of `DataPieceXXX` objects, which are containers of [POD values](https://en.wikipedia.org/wiki/Passive_data_structure) and strings. If you have never seen a `DataLayout` definition, look at the `MyDataLayout` definition in the **`DataLayout` Examples** section below.

`DataLayout` definitions are typically `struct`s containing `DataPieceXXX` members, each with a type and a label. The supported `DataPieceXXX` types are:

`DataPieceValue`, a single value of POD type `T`:

- Type: `DataPieceValue<T>`
- Example: `DataPieceValue<int32_t> exposure{"exposure"};`

`DataPieceEnum`, a single value of enum `ENUM_TYPE` stored as `POD_TYPE`:

- Type: `DataPieceEnum<ENUM_TYPE, POD_TYPE>`
- Example: `DataPieceEnum<PixelFormat, uint32_t> pixelFormat{"pixel_format"};`

`DataPieceArray`, a fixed-size array of values of POD type `T`:

- Type: `DataPieceArray<T>`
- Example: `DataPieceArray<float> calibration{"calibration", 25};`

`DataPieceVector`, a vector of values of type `T`, whose size may vary by record:

- Type: `DataPieceVector<T>`
- Example: `DataPieceVector<int8_t> udpPayload{"udp_payload"};`

`DataPieceStringMap`, the equivalent of `std::map<std::string, T>`:

- Type: `DataPieceStringMap<T>`
- Example: `DataPieceStringMap<Point2Di> labelledPoints{"labelled_points"};`

`DataPieceString`, a `std::string` value:

- Type: `DataPieceString`
- Example: `DataPieceString message{"message"};`

Template class `T` can be any of these built-in POD types:

- Boolean (use `vrs::Bool`)
- Signed or unsigned integer (8, 16, 32, or 64 bits)
- 32-bit `float`
- 64-bit `double`

Template class `T` can also be any of these types (using `float`, `double` or `int32_t` for coordinates):

- 2D, 3D, or 4D points
- 3×3 or 4×4 matrices

`std::string` can be used with `DataPieceVector<T>` and `DataPieceStringMap<T>`, but cannot be used with the other template types.

<!-- prettier-ignore -->
:::note
For integer fields, use fixed-width types from `<cstdint>`. Never use native platform-dependent types like `short`, `int`, `long`, or `size_t`, because their size can vary depending on the architecture or the compiler configuration.
:::

### `DataLayout` Format Resilience

`DataLayout` definitions can add, remove, or reorder `DataPieceXXX` fields. Newer code can still read older files, and older code can read newer files, as long as the fields it needs still match.

VRS matches each `DataPieceXXX` using the following:

- `DataPiece` type
- Label
- Stored element type, when applicable, and the length of a fixed-size array

This unique combination is critical to providing datalayout forward/backward compatibility, without worrying about the actual placement of the data. If you change the type, label, or fixed array length of a field, you will change its signature, and it won't be recognized in older files. The modified field will look like a new field, and the data from older files will no longer be accessible using the updated definition.

`DataLayout` does not support arbitrary or nested `DataPiece` containers. To group fields in nested structs, use `DataLayoutStruct`, as shown in the second example below. For a fixed-size array of those structs, use `DataLayoutStructArray`.

In some situations, such as when you need to save space, it is desirable to store some fields only in some conditions. The `OptionalDataPieces` template makes it easy to control whether a group of fields should be saved or not, but the choice must be made once for the whole file.

If you need more freedom, you can use a format such as JSON in a `DataPieceString` field. If you have binary data, you can use a `DataPieceVector<uint8_t>` field.

We recommend that you use lowercase [snake_case](https://en.wikipedia.org/wiki/Snake_case) as your naming convention for labels. This will limit problems if these names are used as keys in a Python dictionary, in particular when using **pyvrs** to create or read datalayouts.

### `DataLayout` Examples

<Tabs>
  <TabItem value="example_1" label="Example 1: standard case" default>

Here is a sample `DataLayout` definition:

```cpp
struct MyDataLayout : public AutoDataLayout {

  // Fixed size pieces: std::string is NOT supported as a template type.
  DataPieceValue<double> exposureTime{"exposure_time"};
  DataPieceValue<uint64_t> frameCounter{"frame_counter"};
  DataPieceValue<float> cameraTemperature{"camera_temperature"};
  DataPieceEnum<PixelFormat, uint32_t> pixelFormat{"pixel_format"};
  DataPieceArray<Matrix3Dd> arrayOfMatrix3Dd{"matrices", 3}; // array size = 3

  // Variable size pieces: std::string is supported as a template type.
  DataPieceVector<Point3Df> vectorOfPoint3Df{"points"};
  DataPieceVector<string> vectorOfString{"strings"};
  DataPieceString description{"description"}; // Any string. Could be json.
  DataPieceStringMap<Matrix4Dd> aStringMatrixMap{"some_string_to_matrix4d_map"};
  DataPieceStringMap<string> aStringStringMap{"some_string_to_string_map"};

  AutoDataLayoutEnd endLayout;
};
```

The struct must derive from `AutoDataLayout` and end with an `AutoDataLayoutEnd` field. The `DataPieceXXX` constructors register each field with the enclosing layout. Each field has a unique label.

  </TabItem>
  <TabItem value="example_2" label="Example 2: nested definitions">

<!-- prettier-ignore -->
:::note
This option is not commonly needed.
:::

It is possible to define structs that can be nested in a DataLayout definition. For example:

```cpp
struct Pose : public DataLayoutStruct {
  DATA_LAYOUT_STRUCT(Pose) // repeat the name of the struct
  DataPieceVector<vrs::Matrix4Dd> orientation{"orientation"};
  DataPieceVector<vrs::Matrix3Dd> translation{"translation"};
};

struct MyDataLayout : public AutoDataLayout {
  Pose leftHand{"left_hand"};
  Pose rightHand{"right_hand"};
  AutoDataLayoutEnd endLayout;
};
```

Each field label in a `DataLayoutStruct` is prefixed with the struct instance’s name and a `/`. This keeps labels unique within the layout.

Effectively, the declaration above creates the same `DataPiece` fields and the same datalayout definition as the datalayout definition below, which requires different member variable names to avoid conflicts at the struct level:

```cpp
struct MyDataLayout: public AutoDataLayout {
  DataPieceVector<vrs::Matrix4Dd> leftHandOrientation{"left_hand/orientation"};
  DataPieceVector<vrs::Matrix3Dd> leftHandTranslation{"left_hand/translation"};
  DataPieceVector<vrs::Matrix4Dd> rightHandOrientation{"right_hand/orientation"};
  DataPieceVector<vrs::Matrix3Dd> rightHandTranslation{"right_hand/translation"};
  AutoDataLayoutEnd endLayout;
};
```

It is possible to nest a `DataLayoutStruct` within other `DataLayoutStruct` definitions as often as makes sense. The resulting `DataPiece` fields will have labels similarly constructed, with deeper nesting. However, a `DataLayoutStruct` cannot be the element type of a `DataPiece` container.

  </TabItem>
  <TabItem value="example_3" label="Example 3: optional definitions">

<!-- prettier-ignore -->
:::note
This option is only very rarely needed.
:::

You can define fields that are used only when some recording conditions are met, or with some devices. This helps to save space, and makes the records less ambiguous, since they will only show these fields if they were actually used while recording.

For example:

```cpp
/// Sample sensor not always available on all devices
struct TemperatureData {
  DataPieceValue<float> cameraTemperature{"camera_temperature"};
};

struct MyDataLayout : public AutoDataLayout {
  MyDataLayout(bool allocateOptionalFields = true)
      : optionalTemperature(allocateOptionalFields) {}

  DataPieceValue<double> exposureTime{"exposure_time"};
  DataPieceValue<uint64_t> frameCounter{"frame_counter"};

  const OptionalDataPieces<TemperatureData> optionalTemperature;

  AutoDataLayoutEnd endLayout;
};
```

When recording a file, you need to decide upfront, at runtime, whether the optional fields are needed for this recording, and then select the appropriate constructor. This is because the optional fields must be allocated during the datalayout construction.

When reading a file, construct the expected layout with the optional fields allocated (the default in this example), then call `isAvailable()` on each optional field to see whether it was recorded.

  </TabItem>
</Tabs>

### Image, Audio, and Custom Content Blocks

Image, audio, and custom content blocks directly contain their payload and no additional metadata. In some cases, such as for `image/jpg` or `image/png` data, no other information is needed to interpret the data. In other cases, such as with `image/raw` images, which are raw pixel buffers, image dimensions, pixel format, and possibly stride are needed to interpret the image content block. If that information never changes, it may be provided directly in the `RecordFormat` definition. Otherwise, it may need to be provided in a configuration record, or in the data records themselves, using what we call the Datalayout Conventions.

<Tabs>
  <TabItem value="example_1" label="Image Content Block Examples" default>

```cpp
ContentBlock(ContentType::IMAGE); // Image content block, without any detail
ContentBlock(ImageFormat::JPG); // A JPEG image
ContentBlock(ImageFormat::JPG, 640, 480); // A 640x480 JPEG image
ContentBlock(ImageFormat::RAW); // A raw pixel buffer image
ContentBlock(PixelFormat::GREY8, 640, 480); // A raw pixel buffer image, 640x480 large, with 8 bit greyscale pixels.
```

Please refer to the Image Support section for more details on how to manage image content blocks.

  </TabItem>
  <TabItem value="example_2" label="Audio Content Block Examples">

```cpp
ContentBlock(ContentType::AUDIO); // Audio content block, without any detail
ContentBlock(AudioFormat::PCM); // PCM audio data
ContentBlock(AudioFormat::PCM, AudioSampleFormat::S16_LE, 2, 0, 48000); // PCM audio data, int16 little-endian samples, 2 channels, 48 kHz, default stride
```

Audio blocks are handled similarly to image blocks. VRS supports PCM and Opus audio blocks; decoding Opus requires a build with Opus support.

  </TabItem>
  <TabItem value="example_3" label="Custom Content Block Examples">

```cpp
ContentBlock(ContentType::CUSTOM); // No details at all
ContentBlock(ContentType::CUSTOM, 256); // 256 bytes custom content block
CustomContentBlock("my_thing", 48); // 48 bytes custom content block in the format "my_thing"
```

If VRS cannot infer the size of a custom block from the record format and remaining bytes, provide it in the immediately preceding `DataLayout` using the DataLayout Conventions.

  </TabItem>
</Tabs>

## Registering your `RecordFormat` and `DataLayout` Definitions

When you create a `Recordable` object to record a stream, you need to register the `RecordFormat` for its records. For example:

```cpp
// Assuming your recordable has a member variable declared like so:
MyDataLayout config_;

// in your Recordable's constructor, call:
addRecordFormat(
  Record::Type::CONFIGURATION, // record types are defined separately
  kConfigurationRecordFormatVersion, // only change when the RecordFormat changes
  config_.getContentBlock(), // RecordFormat definition: a single datalayout block
  {&config_}); // DataLayout definition for the first block
```

Here is an example of a record that contains a datalayout block, followed by an image block (“`data_layout+image/raw`”):

```cpp
// Assuming your recordable has a member variable declared like so:
MyDataLayoutForDataRecords data_;

// in your Recordable's constructor, call:
addRecordFormat(
  Record::Type::DATA, // record types are defined separately
  kDataRecordFormatVersion, // only change when RecordFormat changes
  data_.getContentBlock() + ContentBlock(ImageFormat::RAW), // RecordFormat definition
  {&data_}); // DataLayout definition for the first block, nothing for the image block
```

Each stream stores one `RecordFormat` and set of `DataLayout` definitions per record type and format version. Use a new format version if the same stream needs to write records of the same type with different block sequences or layout definitions. In `addRecordFormat()`, each layout pointer must be at the index of its corresponding content block; use `nullptr` for preceding blocks that are not `DataLayout` blocks.

<!-- prettier-ignore -->
:::note
You do not need a new record format version just because a `DataLayout` changes between files: each file carries its own definition. Within one stream, all records with the same record type and format version must match the registered definitions.
:::

## Reading Records

To read records described using `RecordFormat` conventions, attach a `RecordFormatStreamPlayer` to the stream using `RecordFileReader::setStreamPlayer()`. Override the callbacks for the content blocks you want to read:

- `onDataLayoutRead()`
- `onImageRead()`
- `onAudioRead()`
- `onCustomBlockRead()`

VRS processes content blocks in order and stops if a callback returns `false` or a read fails. The image, audio, and custom callbacks run before the payload is read; consume the entire block before returning `true` to continue.

### Reading a Datalayout

When reading a `data_layout` content block, you will get an `onDataLayoutRead` callback in your `RecordFormatStreamPlayer` object, with the datalayout already loaded. In the `onDataLayoutRead` callback, you will want to handle records differently, depending on their record type.

For each record type, you will have a specific `DataLayout` definition, describing the latest version of the datalayout you are using. But you cannot know if that definition matches what was read, since the file could be using an older or newer version of the datalayout definition. Use the `getExpectedLayout<MyDataLayout>` API to get a `DataLayout` instance of the type your code is looking for. You can then access each of its fields safely, with the caveat that each field may or may not find actual data in the datalayout that was read from disk.

Fields are matched by label, piece type, stored element type, and fixed array length where applicable. The mapping is cached per stream, record type, format version, and block index. Once mapped, VRS can locate a field directly, though reading a variable-size value still depends on its size.

<!-- prettier-ignore -->
:::tip
When debugging, use `DataLayout::printLayout(std::cout)` to print the incoming datalayout. This will show the field names, their type, and their value, as they are in the record read.
:::

```cpp
class MyCameraStreamPlayer : public RecordFormatStreamPlayer {
bool onDataLayoutRead(const CurrentRecord& record, size_t blockIndex, DataLayout& data) override {
  switch (record.recordType) {
    case Record::Type::CONFIGURATION: {
      MyCameraConfigRecordDataLayout& myConfig =
          getExpectedLayout<MyCameraConfigRecordDataLayout>(data, blockIndex);
      // use the data...
      myConfig.cameraRole.get(); // access the data...
    } break;

    case Record::Type::DATA: {
      // Here are the fields written & expected in the latest version
      MyCameraDataRecordDataLayout& myData =
          getExpectedLayout<MyCameraDataRecordDataLayout>(data, blockIndex);
      // use the data...
      myData.cameraTemperature.get();

      // Rare case: access fields that were removed or renamed
      // e.g., frame_counter's type was changed: fetch the old version if necessary
      uint64_t frameCounter = 0;
      if (myData.frameCounter.isAvailable()) {
        frameCounter = myData.frameCounter.get();
      } else {
        // MyCameraLegacyFields contains removed fields definitions
        MyCameraLegacyFields& legacyData =
            getLegacyLayout<MyCameraLegacyFields>(data, blockIndex);
        frameCounter = myConversionLogic(legacyData.frameCounter.get());
      }
    } break;

    default:
      assert(false); // should not happen, but you want to know if it does!
      break;
  }
  return true; // read next content blocks, if any
}
};
```

### Datalayout Conventions

Datalayout Conventions are a set of names and types that VRS uses to find properties missing from a `RecordFormat`, such as the resolution and pixel format of an `image/raw` block. Datalayout Conventions can also be used to specify the size of a content block when it is ambiguous. Refer to the source header [`<vrs/DataLayoutConventions.h>`](https://github.com/facebookresearch/vrs/blob/main/vrs/DataLayoutConventions.h) to see the actual Datalayout Conventions.

In the examples above, you can determine the size of the datalayout blocks by looking at the actual `DataLayout` definition. However, that only works when all pieces are fixed-size. In that case, the datalayout size is constant no matter what the content is. Look again at the definition of `MyDataLayout` above to see the difference between fixed-size and variable-size pieces.

For a layout with only fixed-size pieces, `getContentBlock()` returns a block described as `data_layout/size=XXX`, where `XXX` is its size in bytes. For a layout with variable-size pieces, the size can change from record to record, so the description is `data_layout`.

A layout with variable-size pieces includes a fixed-size index with one entry per variable-size piece. VRS first reads the fixed-size data and the index. The last entry’s offset and length give the total size of the variable data, which VRS reads separately.

In the second `RecordFormat` example above, we have a datalayout block followed by an image block (`data_layout+image/raw`). Since the image block is the last content block of the record, and VRS knows the overall size of the record, and how to figure out the size of the datalayout, we can see that all the remaining bytes must belong to the `image/raw` block. However, this is not sufficient to interpret the image pixel data. This is when we need the Datalayout Conventions.

Cameras generally have settings such as resolution, color mode, exposure mode, and frame rate that change only when the camera is reconfigured. Store those settings in a datalayout block in a configuration record.

In a more advanced system, a camera’s resolution and color mode may change for each frame, as when driven by a computer vision algorithm or some other heuristic. When you save only a sub-region of a whole image, the crop size of the image might change in every frame. In such cases, the image parameters should not be placed in a configuration record. Those parameters should be specified in the datalayout block preceding the image block.

VRS uses the following heuristics:

- Search each datalayout block before the ambiguous block, in the same record, in reverse content block order. If the `RecordFormat` is `data_layout+data_layout+image/raw+data_layout`, to disambiguate the `image/raw` block, VRS ignores the last datalayout block (because it comes after the image/raw block), and searches the second datalayout block first. If that block does not provide the required properties together, VRS tries the first datalayout block. It does not combine fields from multiple datalayout blocks.

- If VRS cannot find the required image properties in the same record, it looks in the last configuration record read for that stream, provided its timestamp is not later than the current record’s.

<!-- prettier-ignore -->
:::note
Read the applicable configuration record before reading data records that depend on it, including when seeking. Reading a data record does not implicitly read a configuration record.
:::

`RecordFormatStreamPlayer` caches the location of a matching layout for subsequent records of that format. If a stream uses several configuration formats, a cached location can still refer to the last record read of an earlier format.

For example, `onImageRead()` can provide a `ContentBlock` with the resolution and pixel format taken from a previously read configuration record.

If VRS cannot determine the image block’s format or size, it calls `onUnsupportedBlock()` instead of `onImageRead()`.

To give the size of the block immediately after a datalayout block, use `datalayout_conventions::NextContentBlockSpec` (the `next_content_block_size` field). See [`<vrs/DataLayoutConventions.h>`](https://github.com/facebookresearch/vrs/blob/main/vrs/DataLayoutConventions.h).

## Mistakes to Avoid

While `RecordFormat` and `DataLayout` are designed to resolve a large number of backward and forward compatibility issues, you still need to be aware of the following potential problems:

- **Do not persist any raw `struct`, always copy fields one by one.** If you are receiving a data structure, such as a C `struct` from a driver, you need to copy each field needed in that data structure into a `DataLayout`, one by one, no matter how tedious. You will also have to adjust the fields in your `DataLayout` structure when the incoming `struct` changes. You might be tempted to save the whole memory block to your VRS records, to spare yourself the work. However, doing that would make your file format vulnerable to any changes made to the struct definition, which is controlled and updated by the maintainers of that data structure. It would make it extremely difficult to understand why data read from older files looks corrupt, and even harder to be able to read those files. C/C++ `struct`s do not have any data format introspection capability, and this is why VRS does not, and will not, support the use of any arbitrary `<class T>` in its template `DataPiece` containers. Doing so would be a massive design blunder. _Writing the tedious code that copies each field, one by one, from a received data structure to a copycat `DataLayout` protects your data from unexpected data format changes, and will save you hours, maybe even days, of work and frustration._

- **Do not use `short`, `int`, or `size_t`** directly in any `DataPiece` template. Use fixed-width integer types such as `uint8_t` instead, since native integer widths can vary across architectures and compiler configurations.

- **Do not persist enums using `DataPieceValue<ENUM_TYPE>`.** Use `DataPieceEnum<ENUM_TYPE, T>` to cast between the enum and a fixed-width integer storage type such as `int32_t`. VRS stores the integer type and value, not the enum definition.

- **Be very careful when persisting external enums** when casting them as integers to store them, because you might not control external enum definitions. You should convert external enums to text or to your own version of these enums. If the definition of an external enum changes, the code will start to misinterpret data, and it will be very difficult to fix. If you convert enums to text or create your own enums, whose evolution you control, you will avoid difficult debugging issues. If you really want to persist enums by casting their numeric value, you should create a unit test that will break when the enum values change, or you can simply add `static_assert`s to your code.

- Creating `DataLayout` objects is relatively expensive, as they use external memory buffers and indexes, but they usually do not consume too much memory. The amount of memory used is directly proportional to the number and size of the fields in the layout. Prefer **reusing a single instance of each `DataLayout` type** that you need. Update its fields before creating the record, rather than creating a new `DataLayout` object on the stack each time you need to create a record.

- Construct an `AutoDataLayout` on one thread. Different threads can construct separate instances concurrently.

- When using containers (such as `DataPieceVector` and `DataPieceStringMap`), **use `stagedValues()` to update the containers**, rather than creating a new container each time and calling `stage()`. This will avoid doing a new container allocation and copy each time. For most use cases, successive records are very similar, often with an identical memory footprint, and updating containers will be significantly faster this way.

## Additional samples

You can find examples of how to create and read records using `RecordFormat` and `DataLayout` here: [Datalayout sample code](https://github.com/facebookresearch/vrs/blob/main/sample_code/SampleRecordFormatDataLayout.cpp).

## Why Reinvent the Wheel?

_What's so special about DataLayout? Why did you not use JSON, Thrift, or some other existing serialized containers?_

Historically, `DataLayout` was designed to be backward compatible with our early VRS files, which used straight-up structures of POD data. However, we now have better reasons than that. `DataLayout` leverages a specific pattern of sensor data collection, for which VRS was designed, where records are remarkably regular throughout a recording. For each device and record type, the exact same content blocks, using the same `DataLayout`s are recorded over and over again, often many millions of times.

For each record type and format version in a stream, there is one `RecordFormat`, with its own set of `DataLayout` definitions, which is the dictionary of field types and labels in the datalayout content blocks of the stream. Each `DataLayout` block in the record contains only its own data, in raw binary form, which reduces processing overhead to a minimum. Therefore, the marginal cost of a `DataPieceValue<uint8_t>`, before compression, is one byte per record, regardless of its label, and even if it is the only field in the datalayout content block.

Record payloads are stored in binary, and no code generation is required. The reader parses each `DataLayout` definition and maps the fields your code expects to their stored locations once per layout. If a field is missing, its getter returns the field’s default; use `isAvailable()` to tell whether a field is present in the record.

### What is `DataLayout` “really” good at?

All the power of `DataLayout` lies in its ability to amortize costs. Amortized, `DataLayout` objects...

- ...store each fixed-size scalar in its storage width before record-level compression.
- ...avoid parsing the layout or matching fields for each record.
- ...locate mapped fields in constant time, regardless of how many fields the layout has.
- ...are pure binary containers (no string conversions, unlike JSON).
- ...require no pre-processor/code generation: `DataLayout` definitions are directly compiled by a C++ compiler.
- ...can reuse buffers and staged containers across records, including for variable-size fields.
- ...look, behave, and feel like a simple C++ struct: they are very readable, very easy and efficient to read and write to.
- ...map matching fields once even when the reader and writer use different layout definitions.

The key assumption VRS makes is that data collected within each stream is extremely repetitive throughout a particular file, and everything is done to leverage that property to the fullest. `DataLayout` definitions are stored per stream and format. The reader reuses the parsed definitions and field mappings across records.

### What is `DataLayout` not so good at?

- Seamless integration with existing data representations, like structs and other classes. You will need to write converters to copy your data source(s) to your `DataLayout` definitions, field by field.\
  This can feel tedious, but it protects you against unexpected changes and provides a clear place to handle format changes.
- Nested definitions are supported, but with limitations. See [this documentation (in the “Example 2: nested definitions” tab) for details](https://facebookresearch.github.io/vrs/docs/RecordFormat#datalayout-examples). For 99% of sensor data use cases, `DataLayout` works great and this limitation isn’t even apparent, but for advanced use cases with more structured data and variable formats, or when you have nested definitions with variable size data, `DataLayout` conversion becomes a pain point.
- Complex data structures, in particular, arbitrary data structures that might change with every record, or not be known at compile time, so that converter code cannot be written. In that case, you might need to use a self-described container, such as JSON or MessagePack. Looking at the needs of sensor data collection, this should be rare, or needed only for configuration records. Using JSON in configuration records is fine, because it’s typically a one-time cost, and the trade-offs are radically different when you need to do an expensive operation and store data relatively inefficiently once during setup vs. N million times in real time (for each record). For instance, camera calibration is often provided as a JSON string in the stream's configuration record, and there is no reason to change that.
