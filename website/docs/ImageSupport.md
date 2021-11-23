---
sidebar_position: 7
title: Image Support
---

*This page describes how VRS handles image formats using the `DataLayout` Conventions.*

Prerequisite: Understanding how record format works.

A VRS record can contain an image in record content blocks of type `image`, which can be of different subtypes:

* `image/raw`
    The image is stored as a buffer of raw pixels, which might be compacted (eg, RAW10 vs. GREY10), but not compressed using a `PixelFormat`. The exact [list of pixel formats](https://github.com/facebookresearch/vrs/blob/main/vrs/RecordFormat.h#L49-L68) grows over time.
* `image/jpg` and `image/png`
    The image is compressed using jpeg or png compression. The payload of the content block contains exactly what a jpg or png file would contain.
* `image/video`
    The image is compressed using according to the spec of a video codec standard, such as H.264 or H.265.  
    *Note that the list of supported codecs is deliberately not part of the spec, but rather, is meant to be implementation dependent.
	VRS Open Source doesn't current provide any implementation.*

## Additional Image Properties

To be complete, image content blocks may require complementary properties, providing the image’s resolution, the image’s stride information, and a pixel format. Without this information, `image/raw` images just can’t be interpreted safely. These properties can be provided in two distinct ways:

* the image properties can be part of the record format description itself, meaning that records using that record format are all using the exact same pixel format. The content block description might look like this:
    * `image/raw/640x480/pixel=grey8`
    * `image/raw/640x480/pixel=yuv_i420_split/stride=640`
* the image properties can be provided in a `datalayout` content block found either before the `image` content block in the same record, or in the stream’s last configuration record. When using this method, the properties must be specified using the [DataLayout Conventions](https://github.com/facebookresearch/vrs/blob/main/vrs/DataLayoutConventions.h#L61-L64), with is merely a collection of data types and labels to use in your DataLayout to specify the image properties. 

Whichever the method, VRS collects the images properties for you, so that the `RecordFormatStreamPlayer::onImageRead()` callback provides a content block object that you can query for the image format directly, no matter how/where it was specified.

### Specifying Additional Image Properties

For `image/raw` and `image/video` images, use an [`vrs::DataLayoutConventions::ImageSpec`](https://github.com/facebookresearch/vrs/blob/main/vrs/DataLayoutConventions.h#L59)
to describe your image in a configuration record, or in a DataLayout block just before the image content block of your data records. The DataLayout fields are:

```cpp
  DataPieceValue<ImageSpecType> width{kImageWidth};
  DataPieceValue<ImageSpecType> height{kImageHeight};
  DataPieceValue<ImageSpecType> stride{kImageStride};
  DataPieceEnum<PixelFormat, ImageSpecType> pixelFormat{kImagePixelFormat};

  // For image/video only
  DataPieceString codecName{kImageCodecName}; // required
  DataPieceValue<ImageSpecType> codecQuality{kImageCodecQuality}; // optional
```

For `image/video` images, specify the key frame timestamp and key frame index, using the following DataLayout fields:

```cpp
  DataPieceValue<double> keyFrameTimestamp{kImageKeyFrameTimeStamp};
  DataPieceValue<ImageSpecType> keyFrameIndex{kImageKeyFrameIndex};
```

## Image Format differences

### `image/raw`

Without additional properties, `image/raw` image content blocks can not be interpreted.

* Required properties: width, height, pixel format.
* Optional properties: stride.

When properties are provided using DataLayout conventions, all the additional properties must be provided by a single DataLayout (that is, for instance, you can’t put the pixel format in a configuration record, and the image dimensions in the data record. Instead you must put both pixel format and dimensions all in the same DataLayout in a configuration record, or all in the datalayout of the data record).

### `image/jpg` and `image/png`

jpg and png payloads are the exact same as files of that type, which are fully self described, therefore, when such image formats are used, properties specified using the DataLayout Conventions are simply ignored by VRS.

### `image/video`

*image/video images support isn't ready for open sourcing at this time. Stay tuned!*

Video image content blocks require additional properties to decode images.

* Required properties: width, height, pixel format, codec name, key frame timestamp, keyframe index.
* Optional properties: compression quality, which may affect how to decode the image.

Like for `image/raw` images, pixel format and image dimension information must be provided in a single DataLayout, either in a configuration record, or the image’s data record. But key frame timestamp and key frame index properties need to change with every image, and therefore will be searched only in the DataLayout in the same data record, before the image content block. These two locations may be the same or not.

Typically, pixel format and image resolution do not change without a configuration change, and are therefore best stored in a configuration record, while key frame timestamp, keyframe index, which must change for every frame, are provided in a DataLayout just before image content blocks in data record.

## Reading Images

Image data is received by the `RecordFormatStreamPlayer::onImageRead()` callback:

```cpp
bool onImageRead(
    const CurrentRecord& record,
    size_t blockIndex,
    const ContentBlock& contentBlock);
```

While the `RecordFormatStreamPlayer::onDataLayoutRead()` callback happens after data has been read from disk, the `RecordFormatStreamPlayer::onImageRead()` callback happens before any image data has been read, however, the provided `contentBlock` object holds all the image properties, so that:

* for all types: the `contentBlock`’s size is known.
* for `image/raw` images: pixel format and resolution are defined.
* for `image/video` images: pixel format, resolution, key frame timestamp and key frame index are defined.

The image data itself can be read using the `RecordReader` object provided by the `CurrentRecord` object.
The image is described by an `ImageContentBlockSpec` object provided by `contentBlock.image()`.

### `image/raw` images

Allocate and/or reuse a buffer in which you can read the image data, which is provided in the format described by `contentBlock`, and beware of the image’s stride, which is how it was when the image was saved.

### `image/jpg` and `image/png` images

You’ll need to read and decode the compressed image data using some standard jpg or png library. 

### `image/video` images

`image/video` images support isn't ready for open sourcing at this time. Stay tuned!

