---
sidebar_position: 7
title: Image Support
---

_This page describes how VRS handles content blocks using the Datalayout Conventions._

Prerequisite: Understanding how `RecordFormat` works.

Image content blocks can be any of the following different subtypes:

- `image/raw`  
   The image is stored as a buffer of raw pixels, which might a compacted pixel format such as [RAW10](https://developer.android.com/reference/android/graphics/ImageFormat#RAW10), but not compressed using a `PixelFormat`.  
   The exact [list of pixel formats](https://github.com/facebookresearch/vrs/blob/main/vrs/RecordFormat.h#L49-L68) grows over time.
- `image/jpg` and `image/png`  
   The image is compressed using jpeg or png compression. The payload of the image content block contains exactly what a jpg or png file would contain.
- `image/video`  
   The image is compressed according to the spec of a video codec standard, such as H.264 or H.265.  
   _Note that the list of supported video codecs is deliberately not part of the spec, but rather, is meant to be implementation dependent, part of a VRS extension. VRS Open Source does not currently provide any implementation._

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
  DataPieceEnum<PixelFormat, ImageSpecType> pixelFormat{kImagePixelFormat};

  // For image/video only
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
- Optional properties: stride.

When properties are provided using the Datalayout Conventions, all additional properties must be provided by a single `DataLayout` structure in one record. For example, you must not put the pixel format in the configuration record, and then put the image dimensions in the data record. You must put both the pixel format and the image dimensions in the `DataLayout` of the configuration record, or you must put both of them in the `DataLayout` of the data record.

### `image/jpg` and `image/png`

jpg and png payloads are exactly the same as jpg and png files, which are fully self-described. Therefore, when those image formats are used, properties specified using the Datalayout Conventions are ignored by VRS.

### `image/video`

_Support for `image/video` images is not ready for open sourcing at this time._

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
- For `image/raw` images: the pixel format and resolution are defined.
- For `image/video` images: the pixel format, resolution, keyframe timestamp, and keyframe index are defined.

The image data itself can be read using the `RecordReader` object provided by the `CurrentRecord` object. The image is described by an `ImageContentBlockSpec` object provided by `contentBlock.image()`.

### `image/raw` images

Allocate or reuse a buffer where you can read the image data. Beware of the image’s stride, which is how it was when the image was saved.

### `image/jpg` and `image/png` images

You will need to read and decode the compressed image data using a standard jpg or png library.

### `image/video` images

Support for `image/video` images is not currently ready for open sourcing. Stay tuned!
