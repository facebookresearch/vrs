---
sidebar_position: 7
title: Image Support
---

_This page describes how VRS handles content blocks using the DataLayout Conventions._

Prerequisite: Understanding how `RecordFormat` works.

Image content blocks can have the following subtypes:

- `image/raw`\
   The image is stored as a buffer of raw pixels, which may use a packed pixel format such as [RAW10](https://developer.android.com/reference/android/graphics/ImageFormat#RAW10), but is not encoded by an image or video codec.\
   See the [list of pixel formats](https://github.com/facebookresearch/vrs/blob/main/vrs/RecordFormat.h#L68-L100).
- `image/png`, `image/jpg`, `image/jxl`, and `image/custom_codec`\
   PNG, JPEG, and JPEG XL blocks contain the same bytes as a standalone image file. The custom codec defines its own payload format.\
   _Note that when using a custom codec, you must specify a codec name, or the image content block will be reported as unsupported on read._
- `image/video`\
   The image is compressed according to the spec of a video codec standard, such as H.264 or H.265.\
   _VRS does not specify which video codecs a decoder must support. The optional FFmpeg-based XPRS decoder supports the codecs enabled in its build._

## Additional Image Properties

Image content blocks, particularly `image/raw` blocks, may need these additional properties:

- Resolution
- Stride
- Pixel format

Raw images need a resolution and pixel format. Specify strides if the rows include padding beyond the format’s defaults. These properties can be provided in two ways:

- The image properties can be part of the `RecordFormat` description itself. They are then the same for every record using that format.  
  The content block description might look like this:
  - `image/raw/640x480/pixel=grey8`
  - `image/raw/640x480/pixel=yuv_i420_split/stride=640`
- The image properties can be provided in a datalayout content block before the `image` block in the same record, or in a configuration record for the same stream. Use the types and labels defined by the [DataLayout Conventions](https://github.com/facebookresearch/vrs/blob/main/vrs/DataLayoutConventions.h#L30-L50). Read the applicable configuration record before reading the image record.

When VRS resolves the block’s format and size, `RecordFormatStreamPlayer::onImageRead()` receives a `ContentBlock` with the available image properties. If VRS cannot resolve the block, it calls `onUnsupportedBlock()` instead.

### Specifying Additional Image Properties

For `image/raw`, `image/custom_codec`, and `image/video`, use the fields from [`vrs::datalayout_conventions::ImageSpec`](https://github.com/facebookresearch/vrs/blob/main/vrs/DataLayoutConventions.h#L65) in a configuration record or a `DataLayout` before the image content block in the same record.

The `DataLayout` fields are:

```cpp
  DataPieceValue<ImageSpecType> width{kImageWidth};
  DataPieceValue<ImageSpecType> height{kImageHeight};
  DataPieceValue<ImageSpecType> stride{kImageStride};
  DataPieceValue<ImageSpecType> stride2{kImageStride2};
  DataPieceEnum<PixelFormat, ImageSpecType> pixelFormat{kImagePixelFormat};

  // For image/custom_codec and image/video only
  DataPieceString codecName{kImageCodecName}; // required unless provided by the RecordFormat
  DataPieceValue<ImageSpecType> codecQuality{kImageCodecQuality}; // optional
```

For `image/video` images, specify the keyframe timestamp and the keyframe index using a [`vrs::datalayout_conventions::VideoFrameSpec`](https://github.com/facebookresearch/vrs/blob/main/vrs/DataLayoutConventions.h#L101) or the following `DataLayout` fields, in the `DataLayout` block immediately before the image content block:

```cpp
  DataPieceValue<double> keyFrameTimestamp{kImageKeyFrameTimeStamp};
  DataPieceValue<ImageSpecType> keyFrameIndex{kImageKeyFrameIndex};
```

## Image Format Differences

### `image/raw`

The properties for interpreting an `image/raw` content block are:

- Required properties: width, height, pixel format.
- Optional properties: stride, stride2.

When using the DataLayout Conventions, put the width, height, pixel format, and any non-default strides in a single `DataLayout`. VRS does not combine these properties across layouts or fill in a partial `RecordFormat` description from a `DataLayout`.

### `image/png`, `image/jpg` and `image/jxl`

PNG, JPEG, and JPEG XL payloads are self-describing. VRS does not look up their image properties in `ImageSpec` fields. It must still know the block size; if it cannot infer the size from the record format and remaining bytes, specify it with `NextContentBlockSpec` in the preceding `DataLayout`.

### `image/custom_codec`

- Required properties: codec name.
- Optional properties: width, height, pixel format, stride, stride2, and codec quality.

Provide the codec name in a `DataLayout` using `ImageSpec`. If the name is in the `RecordFormat` instead, the reader still needs width, height, and pixel format from an `ImageSpec`. A custom decoder may require more properties. VRS does not ship decoders for custom codecs by default.

### `image/video`

Video image content blocks require additional properties to decode images:

- Required properties: width, height, pixel format, and codec name. Provide the keyframe timestamp and index for decoding frames out of sequence.
- Optional properties: stride, stride2, and codec quality.

As with raw images, put the pixel format and dimensions in a single `DataLayout`. Use a configuration record if they change only when the stream is reconfigured; otherwise use a layout before the image block in the same record.  
VRS reads the keyframe timestamp and index only from the `DataLayout` immediately before the `image/video` block. The timestamp identifies the group’s keyframe. The index is `0` for the keyframe, `1` for the next frame, and so on.

## Reading Images

Image data is received by the `RecordFormatStreamPlayer::onImageRead()` callback:

```cpp
bool onImageRead(
    const CurrentRecord& record,
    size_t blockIndex,
    const ContentBlock& contentBlock);
```

The `RecordFormatStreamPlayer::onDataLayoutRead()` callback happens **after** the `DataLayout` data has been read from disk.

The `RecordFormatStreamPlayer::onImageRead()` callback runs **before** the image payload is read. The `contentBlock` provides the properties VRS has resolved:

- For all types: `contentBlock.getBlockSize()` gives the number of bytes in the stored image payload.
- For `image/raw` images: resolution, strides, and pixel format are defined.
- For `image/custom_codec` images: the codec name is defined. Resolution, strides, and pixel format may also be available.
- For `image/video` images: pixel format, resolution, and codec name are defined. Keyframe timestamp and index are available when both fields were supplied in the preceding layout.

Use `record.reader` to read the image payload and `contentBlock.image()` to get its `ImageContentBlockSpec`. Consume the entire image block before returning `true` to continue to the next block.

### `image/raw` images

Allocate or reuse a buffer for the image payload. `stride` is the number of bytes between successive rows of the first plane, and `stride2` is the stride of the remaining planes. Use `getPlaneStride()` and `getPlaneHeight()` to account for padding and subsampling.

### `image/png`, `image/jpg`, and `image/jxl` images

Use `vrs::utils::PixelFrame::readFrame()` or another image decoder to read and decode the payload. JPEG XL decoding requires a build with JPEG XL support.

### `image/custom_codec` images

Read and decode the payload with a decoder for the named codec, or register a decoder with `vrs::utils::DecoderFactory` and use `PixelFrame::readFrame()`.

### `image/video` images

Use `vrs::utils::VideoRecordFormatStreamPlayer` to read and decode video frames. For FFmpeg-based decoding, enable XPRS with `-DBUILD_WITH_XPRS=ON` on a supported build. When seeking to a non-keyframe, decoding may require reading the preceding frames back to the keyframe.
