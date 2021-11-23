---
sidebar_position: 6
title: Record Format
---

import Tabs from '@theme/Tabs';
import TabItem from '@theme/TabItem';

VRS offers standardized methods to represent common data types, but it does not prescribe how to address specific use cases. 
You can use VRS to collect data coming from a Quest device, or collect traffic going over USB, and each can be done in different ways.
Of course, data format conventions are desirable, in order to enable data exchange between teams working on identical or similar enough use cases, but they are out scope for VRS.

What VRS offers are methods to store data in flexible ways, both compact and robust to format evolutions.

## `RecordFormat`

With `RecordFormat`, you describe your VRS records as a sequence of typed content blocks. This structure applies to Configuration, State, and Data records alike.

The content block types are: `image`, `audio`, `datalayout` and `custom`. VRS saves `RecordFormat` specifications as a string that will be generated & parsed for you, but which was designed to be very expressive and compact. Content block description may contain additional information, specific to the content type. Here are some examples of records made of a single content block:

* `image`
* `image/png`
* `image/raw`
* `image/raw/640x480/pixel=grb8`
* `image/raw/640x480/pixel=grey8/stride=648`
* `image/video`
* `image/video/codec=H.264`
* `audio`
* `audio/pcm`
* `audio/pcm/uint24be/rate=32000/channels=1`
* `datalayout`
* `datalayout/size=48`
* `custom`
* `custom/size=160`

`image` and `audio` content blocks should work as you'd expect when you read them. `datalayout` blocks contain structured metadata information, which we will describe in details below. `custom` content blocks are blocks of raw data, which format is only known to you, and which you will be responsible for interpreting.

If you need to store more than a single content block per record, you can assemble virtually as many content blocks as you like, which can look like this:

* `datalayout+image/raw`
* `datalayout+datalayout+audio/pcm`

Again, these text descriptions are generated & parsed for you, and you will only see those when using a tool like VRStool and dump a stream's VRS tags, which are tags associated with each stream for VRS' internal usage. That's where a stream's `RecordFormat` and `DataLayout` descriptions are stored in a VRS file.
In practice, the overwhelming majority of the records used in VRS today use one of the following record formats:

* `datalayout`: for records containing a single block of metadata, which is typical of configuration records.
* `datalayout+image/raw`: for records containing some image specific metadata and the raw pixel data of an image.
* `datalayout+image/jpg` and `datalayout+image/video`: for records containing some image specific metadata and compressed image data.

## DataLayout Content Blocks

`DataLayout` content blocks, commonly referred to as “datalayouts”, are containers of [POD values](https://en.wikipedia.org/wiki/Passive_data_structure),
strings, and containers of POD values or strings. If you've never seen a `DataLayout` definition before, take a peek at `MyDataLayout`'s definition in the example section below.

`DataLayout` definitions are structs made of `DataPieceXXX` objects, that each have their own text label. The supported `DataPieceXXX` types are:

`DataPieceValue`, a single value of POD type `T`:  
* Type: `template <class T> DataPieceValue<T>;`  
* Example: `DataPieceValue<int32_t> myCounter{"my_counter"};`

`DataPieceEnum`, a single value of enum `ENUM_TYPE` with the underlying type `POD_TYPE`:  
* Type: `template <typename ENUM_TYPE, typename POD_TYPE> DataPieceEnum<ENUM_TYPE, POD_TYPE>`
* Example: `DataPieceEnum<MyEnum, int32_t> exposureMode{"exposure_mode"};`

`DataPieceArray`, a fixed size array of values of POD type `T`:  
* Type: `template <class T> DataPieceArray<T>;`  
* Example: `DataPieceArray<float> calibration{"calibration", 25};`

`DataPieceVector`, a vector of values of type `T`, which size may change for each record:  
* Type: `template <class T> DataPieceVector<T>`  
* Example: `DataPieceVector<int8_t> numericMessage{"numeric_message"};`


`DataPieceStringMap`, the equivalent of `std::map<std::string, T>`:  
* Type: `template <class T> DataPieceStringMap<T>`  
* Example: `DataPieceStringMap<int32_t> countedTags{"counted_tags"};`

`DataPieceString`, a `std::string` value:  
* Type: `DataPieceString`  
* Example: `DataPieceString message{"message"};`

The template type `T` can be any of the built-in POD types (boolean, signed or unsigned integers of size 8, 16, 32 or 64 bits, 32 bit float or 64 bit double values), 2, 3, or 4D points, as well as 3 or 4D matrices, using either `float`, `double` or `int32_t` for their coordinates. Always use `<cstdint>` definitions, and never use platform dependent types like `short`, `int`, `long`, or `size_t`, which actual size will vary depending on the architecture or your compiler's configuration.
`std::string` can also be used with `DataPieceVector<T>` and `DataPieceStringMap<T>`, but not the other template types.

Datalayouts are defined using structs, and in most ways, you can use handle them like structs, except that:

* `DataPieceXXX` fields can be added or removed,
* `DataPieceXXX` fields can be reordered,

...without breaking the ability to read older files, or preventing older code from reading newer files.

This format change resilience is possible, because each `DataPieceXXX` object is identified by the unique combination of its `DataPiece` type, its template type `T` (if any), and its label. This critical feature allows for forward & backward compatibility of datalayouts, allowing you to add or change fields in your datalayouts without needing to worry about actual data placement.

Conversely, if you change the type or the label of a field, `DataLayout` won’t be able to match the label, and you will break backward compatibility with older files for that field: the updated field will look like a new field, while the data from old files will no longer be accessible when reading them with the new definitions.


`DataLayout` definitions do not support other types of containers, or nested containers, mostly because that would break the forward/backward compatibility contract. However, it is possible to use repeated and/or nested structs, using `DataLayoutStruct`, as shown in the second example below. 


In some situations, it might be desirable to only store some fields in some conditions, in particular, to save space. For an entire recording, the `OptionalDataPieces` template can allow easy control as to whether a group of fields should be saved or not, as long as the choice is consistent throughout the file.

If you need even more freedom, use free form containers like json in a `DataPieceString` field, or a `DataPieceVector<uint8_t>` for binary data.

We recommend that you use the [lowercase snake case naming convention](https://en.wikipedia.org/wiki/Snake_case) for labels, which will limit problems when using these names as keys in a dictionary in Python, should someone ever want to use pyvrs to create or read your datalayouts.

Because each record has a record format version number, and `RecordFormat`/`DataLayout` definitions are tied to a record format version within that stream, it is possible to use multiple `RecordFormat`/`DataLayout` definitions within a particular stream.

## Examples

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
  DataPieceArray<Matrix3Dd> arrayOfMatrix3Dd{"matrices", 3}; // fixed size = 3
  
  // Variable size pieces: std::string is supported as a template type.
  DataPieceVector<Point3Df> vectorOfPoint3Df{"points"}; // variable size
  DataPieceVector<string> vectorOfString{"strings"}; // strings ok
  DataPieceString description{"description"}; // a string. Could be json.
  DataPieceStringMap<Matrix4Dd> aStringMatrixMap{"some_string_to_matrix4d_map"};
  DataPieceStringMap<string> aStringStringMap{"some_string_to_string_map"};

  AutoDataLayoutEnd endLayout;
};
```

Notice that the struct must derive from `AutoDataLayout`, and finish with an `AutoDataLayoutEnd` field, which are required to make the `DataLayout` magic happen. Under the hood, the `DataPieceXXX` constructors will register themselves to the enclosing `AutoDataLayout`. As one generally only creates a single `DataLayout` instance, the overhead doesn't matter much (and it isn't big in the first place).

Also notice that each field has a label. 

  </TabItem>
  <TabItem value="example_2" label="Example 2: nested definitions">

Note: this option isn't very commonly needed.

It is possible to define structs, that can be nested in a DataLayout definition. For instance:

```cpp
struct Pose : public DataLayoutStruct {
  DATA_LAYOUT_STRUCT(Pose) // repeat the name of the struct
  DataPieceVector<vrs::Matrix4Dd> orientation{"orientation"};
  DataPieceVector<vrs::Matrix3Dd> translation{"translation"};
};

struct MyDataLayout: public AutoDataLayout {
  Pose leftHand{"left_hand"};
  Pose rightHand{"right_hand"};
  AutoDataLayoutEnd endLayout;
};
```

The names of the fields in the `DataLayoutStruct` are prepended by the name of the `DataLayoutStruct` itself, with an added ‘`/`’ to make them feel like a path, and so they can be unique at the datalayout level. Effectively, the declaration above creates the same `DataPiece` fields and the same datalayout definition as the following datalayout definition, which requires different member variable names to avoid conflicts at the struct level:

```cpp
struct MyDataLayout: public AutoDataLayout {
  DataPieceVector<vrs::Matrix4Dd> leftHandOrientation{"left_hand/orientation"};
  DataPieceVector<vrs::Matrix3Dd> leftHandTranslation{"left_hand/translation"};
  DataPieceVector<vrs::Matrix4Dd> rightHandOrientation{"right_hand/orientation"};
  DataPieceVector<vrs::Matrix3Dd> rightHandTranslation{"right_hand/translation"};
  AutoDataLayoutEnd endLayout;
};
```

It is possible to nest `DataLayoutStruct` within `DataLayoutStruct` definitions, as often as makes sense, and the resulting `DataPiece` fields will have labels similarly constructed, with deeper nesting. However, it is not possible to use `DataLayoutStruct` definitions in template containers.

  </TabItem>
  <TabItem value="example_3" label="Example 3: optional definitions">

Note: this option is only very rarely needeed.

It is possible to define fields that are only used in some recording conditions or devices, saving space in the records, and making records produced by devices less ambiguous, since they will only show the fields if they were used during recording.

For instance:

```cpp
/// Sample sensor not always available on all devices
struct OptionalFields {
  DataPieceValue<float> cameraTemperature{"camera_temperature"};
};

struct MyDataLayout: public AutoDataLayout {
  MyDataLayout(bool allocateOptionalFields = false)
      : optionalFields(allocateOptionalFields) {}
	
  // Fixed size pieces: std::string is NOT supported as a template type.
  DataPieceValue<double> exposureTime{"exposure_time"};
  DataPieceValue<uint64_t> frameCounter{"frame_counter"};

  const OptionalDataPieces<OptionalFields> optionalFields;

  AutoDataLayoutEnd endLayout;
};
```

At runtime, for recording, you need to decide upfront if the optional fields will be needed for this recording, and select the appropriate constructor, because the optional fields must be allocated during the datalayout's construction.

When reading the file, you can either try to use the appropriate constructor, or always include the optional fields, and test the presence of data in the file by checking each of the fields' `isAvailable()` method.

  </TabItem>
</Tabs>

## Registering your RecordFormat and DataLayout definitions

Once you’ve defined your datalayout, you need to register the `RecordFormat`:

```cpp
// Assuming your recordable has a field declared like so:
MyDataLayout config_;

// in your Recordable's constructor, call:
addRecordFormat(
  Record::Type::CONFIGURATION, // record types are defined separately
  kConfigurationRecordFormatVersion, // only change when the RecordFormat changes
  config_.getContentBlock(), // RecordFormat definition: a single datalayout block
  {&config_}); // DataLayout definition for the first block
```

Let's spice it up, with a record that contains a datalayout block, followed by an image block (“`datalayout+image/raw`”):

```cpp
// Assuming your recordable has a field declared like so:
MyDataLayoutForDataRecords data_;

// in your Recordable's constructor, call:
addRecordFormat(
  Record::Type::DATA, // record types are defined separately
  kDataRecordFormatVersion, // only change when RecordFormat changes
  data_.getContentBlock() + ContentBlock(ImageFormat::RAW), // RecordFormat definition
  {&data_}); // DataLayout definition for the first block, nothing for the image block
```

## Reading records

To read records described using `RecordFormat` conventions, attach a `RecordFormatStreamPlayer` to your `RecordFileReader`, and hook your code in the `onDataLayoutRead()`, `onImageRead()`, `onAudioRead()`, and `onCustomBlockRead()` virtual methods, whichever are appropriate for your records. Instead of getting callbacks for the whole record, you will get one callback per content block, or until one of these callbacks return `false`, signaling that the end of the record should not be decoded.

### Reading a DataLayout

When reading a `DataLayout`, you will get a `onDataLayoutRead` callback in your `RecordFormatStreamPlayer` object, with the datalayout already loaded. You will then want to switch depending on the type of record. For each type, chances are, you will have a specific `DataLayout` definition describing the latest version of datalayout you're using, but you do not know if that definition matches what was read, since the file could be using a older or newer version. Use the `getExpectedLayout<MyDataLayout>` API to get a `DataLayout` instance of the type your code expects. You can then access each of its fields safely, with the caveat that each field may or may not have been “mapped” to actual data in the datalayout read from disk.

Each data field is mapped according to the field's type and label only, so you don't need to worry if fields have been added/removed, or moved around. Also, this mapping is cached per file/stream/type, so after the first record, it's extremely cheap and fields are read in constant time, no matter how complicated the datalayout is.

Tip: For debugging purpose, it can be helpful use `DataLayout::printLayout(std::cout)` to print the incoming datalayout. This will show you the field names, their type, and their value.

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

### DataLayout Conventions

In the examples above, how do we know the size of the datalayout blocks? By looking at the actual `DataLayout` definition? That can work if only fixed type pieces are used, as the datalayout size is constant, no matter its content (look again at the definition of `MyDataLayout` above to see the difference between fixed size pieces and variable size pieces). In that case, the `getContentBlock()` API will generate `"datalayout/size=XXX"`, with `XXX` being the number of bytes.
If the datalayout contains any variable size piece, then the size of the datalayout can change from record to record, and the `getContentBlock()` API will return `"datalayout"`. However, in that case, the datalayout data will include an index which size is fixed, since it depends only on the number of variable size pieces declared (not their actual values). Therefore, VRS will be able to determine the overall size of the datalayout data in two successive reads. The first read will include the data for all the fixed size pieces, and the index for the variable size pieces. The index will reveal the total size of the data for the variable size pieces, which we can then read in a second read operation.
Bottom line: we can always read a datalayout block, because we can always determine its actual size, one way or the other. What about our second example above defined as `“datalayout+image/raw”`? Since the image block is the last of the record and we know both the overall size of the record, and we know how to figure out the size of the datalayout, we know that all the remaining bytes must belong to the `“image/raw”` block. But that not’s enough to interpret the image pixel data, and you could imagine a case where there is another datalayout content block after. This where we need what we call the “datalayout conventions”.

Let's step back a bit, and discuss how a camera device is likely to function.
Typically, during the hardware initialization/setup, before the data collection begins, the software stack will configure the camera to function in a particular mode (resolution, color mode, exposure mode, frame rate, etc). None of these parameters will change unless the configuration of the device is changed, which is rare in practice (if it ever happens during a particular capture). These parameters all belong in a configuration record, and can easily be saved in a datalayout block.
In a more advanced system, we could imagine a camera taking pictures at a resolution and color mode which could potentially change for each frame, maybe driven by a computer vision algorithm or some other heuristic. Or maybe we only want to save a sub-region of the whole image, the way Portal does when it tracks a target and crops the image received from the sensor. The crop size could change at every frame. In such a case, those parameters don't belong in a configuration record, and should be specified in a datalayout block preceding the image block.

“Datalayout conventions” are a set of names and types that VRS will use when necessary, to find missing `RecordFormat` specifications, such as the image resolution and pixel format of an `“image/raw”` block. VRS will use the following heuristic:

* search each datalayout block before the ambiguous block in the same record, in reverse content block order. So if the record's `RecordFormat` is `“datalayout+datalayout+image/raw+datalayout”`, to disambiguate the `“image/raw”` block, VRS will completely ignore the third datalayout block (because it's after), and will search the second datalayout block first. If that was not enough, it will then search the first datalayout block.
*  If resolution and pixel format information could not be found in the same record, VRS will search the last read configuration record for that stream.

See the source header `<VRS/DataLayoutConventions.h>` for the actual conventions. Note that this look-up uses cached data: *the configuration record must have been read before the data record* (reading a record will not cause another record to be read implicitly). This works, because `RecordFormatStreamPlayer` caches the data for the last record of each type it has read.
The datalayout conventions make `RecordFormatStreamPlayer` work a bit magically, as when you get an `onImageRead()` callback, the `ContentBlock` object received is fully fleshed out, and will contain the image resolution & pixel format which might have been specified in a configuration record read a long time ago. In facts, if VRS can not determine unambiguously how the image block is formatted, it won’t make an `onImageRead()` callback, it will make an `onUnsupportedBlock()` callback instead.

### Record format versioning

Each record of each stream and each type have its own “format version” number, an `uint32_t` value. Because records have unique ids (their `StreamId`), and are typed (Configuration, State or Data), these numbers of only meaningful within that space (don't worry about format version collisions between streams). Prior to `RecordFormat`, this versioning was critical, as it probably was the reader's only information about how the record's data actually looked like, and you were responsible for interpreting every byte. You also had to manually manage data format changes. In other words, prior to `RecordFormat`, records' data was not self described within the file.
Also, all the streams of the same `RecodableTypeId` were indistinguishable. So each time you wanted to change what you were storing, each time you wanted to add/remove/change a field, you had to change the format version, and handle a growing number of format versions. A nightmare...
`RecordFormat` & `DataLayout` were designed to solve that challenge, to the point that now, format version changes are very rarely needed now. The reason is that `RecordFormat` abstracts the description of a record as a succession of typed blocks, and embeds that description in the stream itself, within the file. VRS can then use these embedded descriptions to interpret records itself, calculating content blocks boundaries itself (at least, as much as possible), and handing parsed content blocks to callbacks.
Moreover, `DataLayout` definitions fully describe what's stored in the actual record, so you can freely change your `DataLayout` definitions without changing the format version, as shown in the “Reading a DataLayout” section above.

## Mistakes to avoid

While `RecordFormat` and `DataLayout` are designed to resolve a large number of backward/forward compatibility issues, there are still mistakes you should avoid:

* Do not persist any raw struct data, **always copy fields one by one**, no matter how tedious.
    Say you are receiving sensor data from a driver as a C structure, and you need to persist every single piece of data in that struct. It might be tempting to save the whole memory block in your VRS records, which would allow you to not have to copy each and every field one by one, in a copycat `DataLayout` definition. But doing so, you would make your file vulnerable to any changes made to that sensor structure definition, effectively controlled by the driver's authors, and it will be extremely difficult to trace back why, one day, some of the data read from older VRS files looks corrupt. Writing the tedious code that will copy each field one by one, from the driver structure to a copycat `DataLayout` structure protects you from unexpected changes, though you might have to add fields to your `DataLayout` from time to time (which is perfectly fine).
    This is why VRS does not and will not support using any arbitrary `template <class T>` in its containers: doing otherwise would be a massive design blunder.
* **Do not use short, int, or size_t** directly in any `DataPiece` template, because their size is architecture and/or compiler dependent, and could result in files that can't be read as expected when the reading code is compiled with different configuration settings than the writing code. Always using fully sized types, such as `uint8_t` instead.
    Reminder: [you can't rely on `size_t`'s size](https://stackoverflow.com/questions/918787/whats-sizeofsize-t-on-32-bit-vs-the-various-64-bit-data-models).
* **Do not persist enums using `DataPieceValue<ENUM_TYPE>`** — this is dangerous, because the type of the enum is captured in the file format (`int` by default), and if the underlying type associated with the enum is ever changed, the data will no longer be accessible.  Instead, use `DataPieceEnum<ENUM_TYPE, T>` which captures the underlying type and performs casting.  Of course, for the underlying type `T` use the fixed-size integral types from `<stdint.h>`  such as `int32_t` rather than ambiguous types like `int`.
* **Be very careful when persisting “external” enums** by casting them to integers, that is, when storing an enum which definition you do not control. Maybe convert them to text, or to your own version of these enums.
    Should the definition of the external enum be changed, the code will start to misinterpret data, and it will be really ugly to fix that. If you converted the enum to text, or created your own enum which evolution you control, you will avoid really hard to debug and fix issues.
    Tip: Should you really require to persist enums using their cast numeric value, then create a unit test that will break when the enum values change, or simply place a few `static_assert` in your code.
* Creating `DataLayout` objects is relatively expensive, as they use external memory buffers and indexes, but they usually don’t consume too much memory (it’s directly proportional to the number of fields in your layout).
     Prefer **reusing a single instance of each `DataLayout` type** you need. Update its fields before creating a record, rather than creating a new `DataLayout` object on the stack each time you need to create a record.
* If you’re using `AutoDataLayout` to build your `DataLayout` objects (like virtually everyone), be aware that their constructor use a synchronization lock, which potentially could compromise multithreading performance.
    Therefore, you should *really* prefer **reusing a single instance of each `DataLayout`** you need...
* When using containers (`DataPieceVector` and `DataPieceStringMap`), **use `stagedValues()` to update the containers**, rather than creating a new container from scratch each time and calling `stage()`, avoiding a container allocation and copy. For most use cases, successive records are very similar, often with an identical memory footprint, and updating containers will be significantly faster.

## Additional samples

[Here is some sample code](https://github.com/facebookresearch/vrs/blob/main/sample_code/SampleRecordFormatDataLayout.cpp) demonstrating how to create and read records using `RecordFormat` & `DataLayout`. 

## Why reinvent the wheel?
*What's so special about DataLayout? Why did you not use json, Thrift, or some other existing serialized containers?*

Historically, `DataLayout` was designed-built to be backward compatible with our early VRS files, which used straight-up structs of POD data. But at this point, this reason is obsolete.

`DataLayout` leverages the specific pattern of sensor data collection, which is what VRS was designed for, where records are remarkably regular throughout a recording: for each device & record type, the exact same content blocks using the same datalayouts are recorded over and over again, often many millions of times. So for each record type of each stream, you will find a single `RecordFormat` with its own set of `DataLayout` definitions, which are a dictionary of the field types and labels, in the VRS metadata content blocks of the stream. In each of the records’ datalayout blocks, you will find only the datalayout’s data, in raw binary form, reducing overhead to the minimum. Therefore, the pre-compression marginal cost of a `DataPieceValue<uint8_t>` is 1 byte per datalayout block, no matter what its label is, and even if it's the only field of the `DataLayout`. In particular, when writing or reading records, there are no binary-ascii conversions, only binary copies.

There is also no pre or post processing of the source code, and a `DataLayout` definition is pretty much as readable as it gets, since it's a struct. `DataLayout` structures are interpreted once when reading a file, and only once is the `DataLayout` the reader expects mapped to the data actually available in the record. Therefore, reading the fields of a `DataLayout` happens in amortized constant time, with no parsing of any kind, as only pointer & size checks are required. If a field isn't available in the record, the type's default value will be returned, and the method `isAvailable()` can be checked to.
