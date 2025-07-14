---
sidebar_position: 7
title: Image Support
---

_This page describes how VRS handles content blocks using the Datalayout Conventions._

Prerequisite: Understanding how `RecordFormat` works.

Image content blocks can be any of the following different subtypes:

- `image/raw`\
   The image is stored as a buffer of raw pixels, which might a compacted pixel format such as [RAW10](https://developer.android.com/reference/android/graphics/ImageFormat#RAW10), but not compressed using a `PixelFormat`.\
   The exact [list of pixel formats](https://github.com/facebookresearch/vrs/blob/main/vrs/RecordFormat.h#L63-L92) grows over time.
- `image/png`, `image/jpg`, `image/jxl`, and `image/custom_codec`\
   The image is compressed using png, jpeg, jpeg-xl, or a custom codec. The payload of the image content block contains exactly what regular file of that image type would contain\
   _Note that when using a custom codec, you must specify a codec name, or the image content block will be reported as unsupported on read._
- `image/video`\
   The image is compressed according to the spec of a video codec standard, such as H.264 or H.265.\
   _Note that the list of supported video codecs is deliberately not part of the spec, but rather, is meant to be implementation dependent, by an extension. VRS Open Source provides the vrs/xprs module to interface with ffmpeg and expose the codecs it was built with._

## Additional Image Properties

To be complete, image content blocks, in particular `image/raw` content blocks, may require the following complementary properties for an image:

- Resolution
- Stride
- Pixel format

Without these properties, `image/raw` images cannot be interpreted safely. These properties can be provided in two distinct ways:

- The image properties can be part of the `RecordFormat` description itself, meaning that the records using that `RecordFormat` will all be using the exact same pixel format.  
  The content block description might look like this:
  - `image/raw/640x480/pixel=grey8`
  - `image/raw/640x480/pixel=yuv_i420_split/stride=640`
- The image properties can be provided in the datalayout content block found either before the `image` content block in the same record, or in the stream’s last configuration record. When using this method, the properties must be specified using the [Datalayout Conventions](https://github.com/facebookresearch/vrs/blob/main/vrs/DataLayoutConventions.h#L61-L64), which are merely a collection of data types and labels to use in your `DataLayout` to specify properties.

Whichever method you use, VRS collects the image properties for you, so that the `RecordFormatStreamPlayer::onImageRead()` callback can directly provide a content block object that you can query for the image format, regardless of how or where it was specified.

### Specifying Additional Image Properties

For `image/raw` and `image/video` images, use a [`vrs::DataLayoutConventions::ImageSpec`](https://github.com/facebookresearch/vrs/blob/main/vrs/DataLayoutConventions.h#L59) to describe your image in a configuration record, or in a `DataLayout` block just before the image content block of your data records.

The `DataLayout` fields are:

```cpp
  DataPieceValue<ImageSpecType> width{kImageWidth};
  DataPieceValue<ImageSpecType> height{kImageHeight};
  DataPieceValue<ImageSpecType> stride{kImageStride};
  DataPieceValue<ImageSpecType> stride2{kImageStride2};
  DataPieceEnum<PixelFormat, ImageSpecType> pixelFormat{kImagePixelFormat};

  // For image/custom_codec and image/video only
  DataPieceString codecName{kImageCodecName}; // required
  DataPieceValue<ImageSpecType> codecQuality{kImageCodecQuality}; // optional
```

For `image/video` images, specify the keyframe timestamp and the keyframe index, using the following `DataLayout` fields:

```cpp
  DataPieceValue<double> keyFrameTimestamp{kImageKeyFrameTimeStamp};
  DataPieceValue<ImageSpecType> keyFrameIndex{kImageKeyFrameIndex};
```

## Image Format Differences

### `image/raw`

Without the following properties, `image/raw` image content blocks can not be interpreted:

- Required properties: width, height, pixel format.
- Optional properties: stride, stride2.

When properties are provided using the Datalayout Conventions, all additional properties must be provided by a single `DataLayout` structure in one record. For example, you must not put the pixel format in the configuration record, and then put the image dimensions in the data record. You must either put both the pixel format and the image dimensions in a configuration record's `DataLayout`, or you must put them both in a `DataLayout` just before the image content block.

### `image/png`, `image/jpg` and `image/jxl`

png, jpg, and jxl payloads are exactly the same as png, jpg, and jxl files, which are fully self-described. Therefore, when those image formats are used, properties specified using the Datalayout Conventions are ignored by VRS.

### `image/custom_codec`

- Required properties: codec name.
- Optional properties: width, height, pixel format, codec name, keyframe timestamp, keyframe index.

The only property VRS itself requires is a codec name for an image content block to be recognized. However, a particular custom codec may require additional properties, such as width, height, and pixel format. Whether a custom codec requires that information or not an implementation detail of each specific custom codec. VRS does not support any particular custom codec by default, they are meant for experimentations and special cases.

### `image/video`

Video image content blocks require additional properties to decode images:

- Required properties: width, height, pixel format, codec name, keyframe timestamp, keyframe index.
- Optional properties: compression quality, which may affect how to decode the image.

Similar to `image/raw` images, pixel format and image dimension information must be provided in a single `DataLayout`, either in the configuration record or in the data record of the image. Typically, the pixel format and image resolution will not change without a configuration change. Therefore, they are best stored in a configuration record.  
On the other hand, the keyframe timestamp and the keyframe index properties change with every frame, and are therefore searched only in a `DataLayout` that must be immediately before the `image/video` content block.

## Reading Images

Image data is received by the `RecordFormatStreamPlayer::onImageRead()` callback:

```cpp
bool onImageRead(
    const CurrentRecord& record,
    size_t blockIndex,
    const ContentBlock& contentBlock);
```

The `RecordFormatStreamPlayer::onDataLayoutRead()` callback happens **after** the `DataLayout` data has been read from disk.

The `RecordFormatStreamPlayer::onImageRead()` callback happens **before** any image data has been read. However, the provided `contentBlock` object holds all the image properties, so the following information can be found:

- For all types: the `contentBlock` size is known. That's the size of the buffer you will need to read the image data stored in the image content block.
- For `image/raw` images: resolution, strides, and pixel format are defined.
- For `image/custom_codec` images: codec name is defined. Resolution, strides, and pixel format might be defined too, if provided in the stream using the Datalayout Conventions.
- For `image/video` images: the pixel format, resolution, codec name, keyframe timestamp, and keyframe index are defined.

The image data itself can be read using the `RecordReader` object provided by the `CurrentRecord` object. The image is described by an `ImageContentBlockSpec` object provided by `contentBlock.image()`.

### `image/raw` images

Allocate or reuse a buffer where you can read the image data. Be careful to properly handle the image’s stride and stride2. `stride` describes the stride of the first plane (the number of bytes separating the first byte of each successive lines), while `stride2` describes the stride of all the following planes, if the images is stored in a multi-plane format.

### `image/png`, `image/jpg`, and `image/jxl` images

You will need to read and decode the compressed image data using a standard implementation.

### `image/custom_codec` images

You will need to read and decode the compressed image data using your own implementation of the custom codec, which implementation is presumably unknown to VRS.

### `image/video` images

Support for `image/video` images is provided by the vrs/xprs module, that depends on ffmpeg, and exposes the codecs built with it.
