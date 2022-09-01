---
sidebar_position: 6
title: Record Format
---

import Tabs from '@theme/Tabs'; import TabItem from '@theme/TabItem';

## Record Format Version

Each record in a stream has its own format version number, which is a `uint32_t` value. However, because records belong to a single stream and each has a record type (Configuration, State, or Data), format version numbers are only meaningful within that stream and for that record type. You do not need to worry about format version collisions between streams and record types.

Before `RecordFormat` was available, record format versioning was critical, because it was the only information about how the record's data was formatted. You were responsible for interpreting every byte of data. You also had to manually manage all data format changes. Since record data formats were not self-described within the file, each time you needed to add, remove, or change a field, you had to change the format version, and handle a growing number of format versions explicitly in the code. This was unmanageable.

`RecordFormat` and `DataLayout` were designed to solve this challenge, and since, record format version changes are very rarely needed. `RecordFormat` abstracts the description of a record as a succession of typed blocks, embedding descriptions, including `DataLayout` definitions, in the stream itself. VRS uses these embedded descriptions to interpret records, calculate content block boundaries using DataLayout Conventions, and pass parsed content blocks to callbacks.

## `RecordFormat`

Use `RecordFormat` to describe records as a sequence of typed content blocks. This structure applies to configuration, state, and data records alike.

### `ContentBlock`

The content block types are: `image`, `audio`, `datalayout`, and `custom`. VRS saves `RecordFormat` definitions as a string that is generated and parsed for you, but which was designed to be very expressive and compact. Content block descriptions may contain additional information, specific to the content type. Here are some examples of single content block `RecordFormat` definitions:

- `image`
- `image/png`
- `image/raw`
- `image/raw/640x480/pixel=grb8`
- `image/raw/640x480/pixel=grey8/stride=648`
- `image/video`
- `image/video/codec=H.264`
- `audio`
- `audio/pcm`
- `audio/pcm/uint24be/rate=32000/channels=1`
- `datalayout`
- `datalayout/size=48`
- `custom`
- `custom/size=160`

`image` and `audio` content blocks are pretty much what you expect when you read their text description. `datalayout` blocks contain structured metadata information. `custom` content blocks are blocks of raw data, which format is known only to you, and which you are responsible for interpreting.

You can assemble as many content blocks as you like in a record, which might look like this:

- `datalayout+image/raw`
- `datalayout+datalayout+audio/pcm`

Again, these text descriptions are generated and parsed for you, so you don't need to worry about their syntax.

The `RecordFormat` and `DataLayout` descriptions of a stream's records are stored in the VRS tags of the stream. You will only see these text descriptions when you are using tools to dump a stream's VRS tags. VRS tags are associated with each stream for VRS internal usage, and are kept separate from the user stream tags.

In practice, the majority of the records used in VRS today use one of the following record formats:

- `datalayout`: for records containing a single metadata content block, which is typical of configuration records.
- `datalayout+image/raw`: for records containing some image specific metadata and the raw pixel data of an image.
- `datalayout+image/jpg` and `datalayout+image/video`: for records containing some image specific metadata and compressed image data.

### Datalayout Content Blocks

Datalayout content blocks, commonly referred to as datalayouts, are `DataLayout` objects that hold containers of [POD values](https://en.wikipedia.org/wiki/Passive_data_structure) and strings. If you have never seen a `DataLayout` definition, look at the `MyDataLayout` definition in the **`DataLayout` Examples** section below.

`DataLayout` are `struct` objects containing `DataPieceXXX` member variables, that each have their own text label. The supported `DataPieceXXX` types are:

`DataPieceValue`, a single value of POD type `T`:

- Type: `template <class T> DataPieceValue<T>;`
- Example: `DataPieceValue<int32_t> exposure{"exposure"};`

`DataPieceEnum`, a single value of enum `ENUM_TYPE` with the underlying type `POD_TYPE`:

- Type: `template <typename ENUM_TYPE, typename POD_TYPE> DataPieceEnum<ENUM_TYPE, POD_TYPE>`
- Example: `DataPieceEnum<PixelFormat, uint32_t> pixelFormat{"pixel_format"};`

`DataPieceArray`, a fixed size array of values of POD type `T`:

- Type: `template <class T> DataPieceArray<T>;`
- Example: `DataPieceArray<float> calibration{"calibration", 25};`

`DataPieceVector`, a vector of values of type `T`, which size may change for each record:

- Type: `template <class T> DataPieceVector<T>`
- Example: `DataPieceVector<int8_t> udpPayload{"udp_payload"};`

`DataPieceStringMap`, the equivalent of `std::map<std::string, T>`:

- Type: `template <class T> DataPieceStringMap<T>`
- Example: `DataPieceStringMap<Point2Di> labelledPoints{"labelled_points"};`

`DataPieceString`, a `std::string` value:

- Type: `DataPieceString`
- Example: `DataPieceString message{"message"};`

Template class `T` can be any of these built-in POD types:

- Boolean (use `vrs::Bool`)
- Signed or unsigned integer (8, 16, 32, or 64 bits)
- 32 bit float
- 64 bit double

Template class `T` can also be any of these vector types (using `float`, `double` or `int32_t` for coordinates):

- 2, 3, or 4D points
- 3 or 4D matrices

`std::string` can be used with `DataPieceVector<T>` and `DataPieceStringMap<T>`, but cannot be used with the other template types.

<!-- prettier-ignore -->
:::note
Always use `<cstdint>` definitions. Never use platform dependent types like `short`, `int`, `long`, or `size_t`. The actual size will vary depending on the architecture or the compiler configuration.
:::

### `DataLayout` Format Resilience

`DataLayout` objects are structs, so it is very simple to add, remove, and reorder `DataPieceXXX` fields. But datalayouts definitions are very resilient to definition changes, so that even when making such changes, newer code can read older files, and older code can read newer files.

Datalayouts format resilience is possible, because each `DataPieceXXX` object is identified by a unique combination of these elements:

- `DataPiece` type
- Label
- Template class `T`, except for `DataPieceString`

This unique combination is critical to providing datalayout forward/backward compatibility, without worrying about the actual placement of the data. If you change the type or the label of a field, you will change its signature, and it won't be recognized in older files. The modified field will look like a new field, and the data from older files will no longer be accessible using the updated definition.

`DataLayout` definitions do not support other types of containers or nested containers, because that would make it impossible to guarantee forward/backward compatibility. However, it is possible to use repeated and nested structs, using `DataLayoutStruct`, as shown in the second example below.

In some situations, such as when you need to save space, it is desirable to store some fields only in some conditions. The `OptionalDataPieces` template makes it easy to specify and control if a group of fields should be saved or not, but the choice must be made once for the whole file.

If you need more freedom, you can use a free form container such as JSON in a `DataPieceString` field. If you have binary data, you can use a `DataPieceVector<uint8_t>` field.

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

Notice that this struct must derive from `AutoDataLayout`, and finish with an `AutoDataLayoutEnd` field. This is required to make the `DataLayout` magic happen. Under the hood, the `DataPieceXXX` constructors will register themselves to the enclosing `AutoDataLayout`. As we will generally only create a single `DataLayout` instance, the overhead is minimal and does not matter. Also, notice that each field has a unique label.

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

The name of each field in the `DataLayoutStruct` is prepended by the name of the `DataLayoutStruct` itself, with a ‘`/`’ added to make it look like a path. This also makes it unique at the datalayout level.

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

It is possible to nest a `DataLayoutStruct` within other `DataLayoutStruct` definitions as often as makes sense. The resulting `DataPiece` fields will have labels similarly constructed, with deeper nesting. However, it is not possible to use `DataLayoutStruct` definitions in template containers.

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
  MyDataLayout(bool allocateOptionalFields = false)
      : optionalFields(allocateOptionalFields) {}

  DataPieceValue<double> exposureTime{"exposure_time"};
  DataPieceValue<uint64_t> frameCounter{"frame_counter"};

  const OptionalDataPieces<TemperatureData> optionalTemperature;

  AutoDataLayoutEnd endLayout;
};
```

When recording a file, you need to decide upfront, at runtime, whether the optional fields are needed for this recording, and then select the appropriate constructor. This is because the optional fields must be allocated during the datalayout construction.

When reading a file, you can try to use the appropriate constructor, or you can always include the optional fields and test if data is present by checking the `isAvailable()` method for each optional field.

  </TabItem>
</Tabs>

### Image, Audio, and Custom Content Blocks

Image, audio, and custom content blocks directly contain their payload, and no additional metadata. In some cases, such as for `image/jpg` or `image/png` data, no other information is needed to interpret the data. In other cases, such with `images/raw` images, which are raw pixel buffers, image dimensions, pixel format and possibly stride information are required to know how to interpret the image content block. If that information never changes, it may provided directly in the `RecordFormat` definition, otherwise, it might need to be provided in a configuration record, or in the data records themselves, using what we call the Datalayout Conventions.

<Tabs>
  <TabItem value="example_1" label="Image Content Block Examples" default>

```cpp
ContentBlock(ContentType::IMAGE); // Image content block, without any detail
ContentBlock(ContentType::JPG); // A jpeg image
ContentBlock(ContentType::JPG, 640, 480); // A 640x480 jpeg image
ContentBlock(ContentType::RAW); // A raw pixel buffer image
ContentBlock(PixelFormat::GREY8, 640, 480); // A raw pixel buffer image, 640x480 large, with 8 bit greyscale pixels.
```

Please refer to the Image Support section for more details on how to manage image content blocks.

  </TabItem>
  <TabItem value="example_2" label="Audio Content Block Examples">

```cpp
ContentBlock(ContentType::AUDIO); // Audio content block, without any detail
ContentBlock(AudioFormat::PCM); // PCM audio data
ContentBlock(AudioSampleFormat::S16_LE, 2, 48000); // PCM audio data, int16 little endian samples, 2 channels, 48 kHz
```

Audio blocks are analog to image blocks, and are handled the same way.

  </TabItem>
  <TabItem value="example_3" label="Custom Content Block Examples">

```cpp
ContentBlock(ContentType::CUSTOM); // No details at all
ContentBlock(ContentType::CUSTOM, 256); // 256 byte custom content block
```

If they are not the last content block in the record, custom content blocks may need to have their size provided using the Datalayout conventions.

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

Here is an example of a record that contains a datalayout block, followed by an image block (“`datalayout+image/raw`”):

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

Each record has a record format version number. Each `RecordFormat`, its record format version number, and its `DataLayout` definitions are tied to a particular stream. Therefore, it is possible to have records using different `RecordFormat`/`DataLayout` definitions within a particular stream, by using different record format version numbers.

<!-- prettier-ignore -->
:::note
`DataLayout` definitions fully describe what is stored in a datalayout content block. So, you can freely change `DataLayout` definitions without changing the record format version.
:::

## Reading Records

To read records described using `RecordFormat` conventions, attach a `RecordFormatStreamPlayer` to your `RecordFileReader`. Then, hook code to whichever of these virtual methods is appropriate for your records:

- `onDataLayoutRead()`
- `onImageRead()`
- `onAudioRead()`
- `onCustomBlockRead()`

You will get one callback per content block, until one of the callbacks returns `false`, signaling that the end of the record should not be decoded.

### Reading a Datalayout

When reading a `datalayout` content block, you will get an `onDataLayoutRead` callback in your `RecordFormatStreamPlayer` object, with the datalayout already loaded. In the `onDataLayoutRead` callback, you will want to handle records differently, depending on their record type.

For each record type, you will have a specific `DataLayout` definition, describing the latest version of the datalayout you are using. But you cannot know if that definition matches what was read, since the file could be using an older or newer version of the datalayout definition. Use the `getExpectedLayout<MyDataLayout>` API to get a `DataLayout` instance of the type your code is looking for. You can then access each of its fields safely, with the caveat that each field may or may not find actual data in the datalayout that was read from disk.

Each data field is mapped according to its data type and label only. So, you do not need to worry whether fields have been added, removed, or moved. Mapping is cached per file/stream/type. So, after the first record is mapped, mapping is extremely cheap, and fields are read in constant time, no matter how complicated the datalayouts are.

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

      // Rare case: access field that were removed or renamed
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
```

### Datalayout Conventions

Datalayout Conventions are a set of names and types that VRS uses to find missing `RecordFormat` specifications, such as the resolution and pixel format, if they are missing in the definition of an `“image/raw”` content block. Datalayout Conventions can also be used to specify the size of a content block when it is ambiguous. Refer to the source header [`<vrs/DataLayoutConventions.h>`](https://github.com/facebookresearch/vrs/blob/main/vrs/DataLayoutConventions.h) to see the actual Datalayout Conventions.

In the examples above, you can determine the size of the datalayout blocks by looking at the actual `DataLayout` definition. However, that only works if only fixed type pieces are used. When only fixed type pieces are used, the datalayout size is constant no matter what the content is. Look again at the definition of `MyDataLayout` above to see the difference between fixed size pieces and variable size pieces.

When only fixed size pieces are used, the `getContentBlock()` API generates `"datalayout/size=XXX"`, with `XXX` being the number of bytes. If the datalayout contains any variable size pieces, the size of the datalayout can change from record to record, and the `getContentBlock()` API will return `"datalayout"`.

If any variable size pieces are present, the datalayout will include an index, which has a fixed size. The index's size depends only on the number of variable size pieces declared, not on their actual values. This index makes it possible for VRS to determine the overall size of the `DataLayout` in two successive reads. The first read includes the data for all the fixed size pieces and the index for the variable size pieces. The added sizes found in the variable size index tells the total size of the variable size pieces, which VRS can now read with a second file read call. Therefore, VRS can always read a `DataLayout` block, because we can always determine its actual size.

In the second `RecordFormat` example above, we have a datalayout block followed by an image block (`“datalayout+image/raw”`). Since the image block is the last content block of the record, and VRS knows the overall size of the record, and how to figure out the size of the datalayout, we can see that all the remaining bytes must belong to the `“image/raw”` block. However, this is not sufficient to interpret the image pixel data. This is when we need the Datalayout Conventions.

When working with a device such as a camera, typically, during the hardware initialization/setup, before the data collection begins, the software stack will configure the camera to function in a particular mode, which includes parameters such as resolution, color mode, exposure mode, and frame rate. These parameters will never change unless the configuration of the camera is changed, which is extremely rare in practice. These parameters all belong to a configuration record and can easily be saved in a datalayout block.

In a more advanced system, a camera’s resolution and color mode may change for each frame, as when driven by a computer vision algorithm or some other heuristic. When you save only a sub-region of a whole image (the way Portal does when it tracks a target and crops the image received from the sensor), the crop size of the image might change in every frame. In such cases, the image parameters should not be placed in a configuration record. Those parameters should be specified in the datalayout block preceding the image block.

VRS uses the following heuristics:

- Search each datalayout block before the ambiguous block, in the same record, in reverse content block order. If the `RecordFormat` is `“datalayout+datalayout+image/raw+datalayout”`, to disambiguate the `“image/raw”` block, VRS ignores the last datalayout block (because it comes after the image/raw block), and searches the second datalayout block first. If that is not enough, it then searches the first datalayout block.

- If the resolution and pixel format values cannot be found in the same record, VRS will search the last read configuration record in that stream.

<!-- prettier-ignore -->
:::note
This look-up uses cached data. The configuration record must have been read before the data record. Reading a record will not cause another record to be read implicitly.
:::

Using cached data works because `RecordFormatStreamPlayer` caches the data of the last record of each type of record it has read.

The Datalayout Conventions make `RecordFormatStreamPlayer` work very efficiently. For example, you get an `onImageRead()` callback, and the `ContentBlock` object is fully fleshed out, with the resolution and pixel format, which might have been specified in a configuration record a long time ago.

If VRS cannot determine, unambiguously, how the image block is formatted, it does not send an `onImageRead()` callback. It sends an `onUnsupportedBlock()` callback instead.

Datalayout Conventions can also be used to specify the size of a content block coming right after a datalayout content block. Refer to [`<vrs/DataLayoutConventions.h>`](https://github.com/facebookresearch/vrs/blob/main/vrs/DataLayoutConventions.h) for implementation details.

## Mistakes to Avoid

While `RecordFormat` and `DataLayout` are designed to resolve a large number of backward and forward compatibility issues, you still need to be aware of the following potential problems:

- **Do not persist any raw `struct`, always copy fields one by one.** If you are receiving a data structure, such as data a C `struct` from a driver, you need to copy each field needed in that data structure into a `DataLayout`, one by one, no matter how tedious. You will also have to adjust the fields in your `DataLayout` structure when the incoming `struct` changes. You might be tempted to save the whole memory block to your VRS records, to spare yourself the work. However, doing that would make your file format vulnerable to any changes made to the struct definition, which is controlled and updated by the maintainers of that data structure. It would make it extremely difficult to understand why data read from older files look corrupt, and even harder to be able to read those files. C/C++ `struct`s do not have any data format introspection capability, and this is why VRS does not, and will not, support the use of any arbitrary `<class T>` in its template `DataPiece` containers. Doing so would be a massive design blunder. _Writing the tedious code that copies each field, one by one, from a received data structure to a copycat `DataLayout` protects your data from unexpected data format changes, and will save you hours, maybe even days, of work and frustration._

- **Do not use `short`, `int`, or `size_t`** directly in any `DataPiece` template, because their size is architecture and compiler dependent, and using them can result in files that cannot be read as expected if the reading code is compiled with different configuration settings than the writing code. You should always use fully sized types, such as `uint8_t` instead. Do not use `size_t` either, because [its size is not dependable](https://stackoverflow.com/questions/918787/whats-sizeofsize-t-on-32-bit-vs-the-various-64-bit-data-models).

- **Do not persist enums using `DataPieceValue<ENUM_TYPE>`** because the type of the enum is captured in the file format (`int` by default), and if the underlying type associated with the enum is ever changed, the data will no longer be accessible. Instead, use `DataPieceEnum<ENUM_TYPE, T>`, which captures the underlying type and performs casting. For the underlying type `T`, use a fixed-size integral type from `<cstdint>`, such as `int32_t` rather than ambiguous types like `int`.

- **Be very careful when persisting external enums** when casting them as integers to store them, because you might not control external enum definitions. You should convert external enums to text or to your own version of these enums. If the definition of an external enum changes, the code will start to misinterpret data, and it will be very difficult to fix. If you convert enums to text or create your own enums, whose evolution you control, you will avoid difficult debugging issues. If you really want to persist enums by casting their numeric value, you should create a unit test that will break when the enum values change, or you can simply add `static_assert`s to your code.

- Creating `DataLayout` objects is relatively expensive, as they use external memory buffers and indexes, but they usually do not consume too much memory. The amount of memory used is directly proportional to the number of fields in your layout, and their size. Prefer **reusing a single instance of each `DataLayout` type** that you need. Update its fields before creating the record, rather than creating a new `DataLayout` object on the stack each time you need to create a record.

- If you are using `AutoDataLayout` to build your `DataLayout` objects (like virtually everyone), be aware that their constructor uses a synchronization lock, which could potentially compromise multi-threading performance. Therefore, you _really_ should be **reusing a single instance of each `DataLayout` type** that you need.

- When using containers (such as `DataPieceVector` and `DataPieceStringMap`), **use `stagedValues()` to update the containers**, rather than creating a new container each time and calling `stage()`. This will avoid doing a new container allocation and copy each time. For most use cases, successive records are very similar, often with an identical memory footprint, and updating containers will be significantly faster this way.

## Additional samples

You can find examples of how to create and read records using `RecordFormat` and `DataLayout` here: [Datalayout sample code](https://github.com/facebookresearch/vrs/blob/main/sample_code/SampleRecordFormatDataLayout.cpp).

## Why Reinvent the Wheel?

_What's so special about DataLayout? Why did you not use JSON, Thrift, or some other existing serialized containers?_

Historically, `DataLayout` was designed to be backward compatible with our early VRS files, which used straight-up structures of POD data. However, we now have better reasons than that. `DataLayout` leverages a specific pattern of sensor data collection, for which VRS was designed, where records are remarkably regular throughout a recording. For each device and record type, the exact same content blocks, using the same `DataLayout`s are recorded over and over again, often many millions of times.

For each record type in a stream, there is one `RecordFormat`, with its own set of `DataLayout` definitions, which is the dictionary of field types and labels in the datalayout content blocks of the stream. Each `DataLayout` block in the record contains only its own data, in raw binary form, which reduces processing overhead to a minimum. Therefore, the marginal cost of a `DataPieceValue<uint8_t>`, before compression, is one byte per record, regardless of its label, and even if it is the only field in the datalayout content block.

When reading and writing records, no binary-ascii conversions are made, only binary copies, and no pre or post processing of the source code is required. `DataLayout` definitions are as readable as possible, since they are `struct` definitions. The `DataLayout` definition is interpreted only once when the file is read, and the `DataLayout` that the reader expects is mapped only once to the `DataLayout` that is actually present in the stream. Therefore, reading the fields of a `DataLayout` happens in amortized constant time, with no parsing of any kind, since only pointer and size checks are required. If a field is not available in a record, the default value for that type is returned, and the `isAvailable()` method can be used to check.

### What is `DataLayout` “really” good at?

All the power of `DataLayout` lies in its ability to amortize costs. Amortized, `DataLayout` objects...

- ...store one byte of payload at the cost of 1 byte of storage (or less, because of record level compression).
- ...have zero serialization/deserialization overhead, both on read and write, including when handling data version mismatch (that’s when the data stored in a file and the definition you have when reading that file don’t match).
- ...have constant field access time, no matter how many you have.
- ...are pure binary containers (no string conversions, unlike json).
- ...require no pre-processor/code generation: `DataLayout` definitions are directly compiled by a C++ compiler.
- ...minimize memory allocations overhead. It’s possible to create and read records without memory allocations beyond record management, even when dealing with variable size arrays (vectors). Again, amortized.
- ...look, behave, and feel like a simple C++ struct: they are very readable, very easy and efficient to read and write to.

The key assumption VRS makes is that data collected within each stream is extremely repetitive throughout a particular file, and everything is done to leverage that property to the fullest. So `DataLayout` stores definitions once per file, parses them once per file-read, maps the `DataLayout` format expected to the `DataLayout` found in the stream once, so all the relatively expensive operations are done only once.

### What is `DataLayout` not good at?

- seamless integration with existing data representations. You will need to write converters to copy your data source(s) to your `DataLayout` definitions, field by field.
- Nested definitions are supported, but with limitations. See [this documentation (in the “Example 2: nested definitions” tab) for details](https://facebookresearch.github.io/vrs/docs/RecordFormat#datalayout-examples). For 99% of sensor data use cases, `DataLayout` works great and this limitation isn’t even apparent, but for advanced use cases with more structured data and variable formats, of when you have nested definitions with variable size data, `DataLayout` conversion becomes a pain point.
- complex data structures, in particular, arbitrary data structures that might change with every record, or not be known at compile time, so that converter code can not be written. In that case, you might need to use a self-described container, such as json or msgpack (which is a binary version of json). Looking at the needs of sensor data collection, this should be rare, or needed only for configuration records, which is fine, because it’s typically a one record need, and the trade offs are radically different when you need to do an operation once during setup vs. N million times in realtime. For instance, camera calibration is often stored as json in a `DataLayout` of a configuration record, and there is no reason to change that.
