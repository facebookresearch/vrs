// Copyright (c) Facebook, Inc. and its affiliates. Confidential and proprietary.

#pragma once

#include <map>
#include <string>
#include <vector>

#include "Record.h"

namespace vrs {

using std::get;
using std::map;
using std::move;
using std::pair;
using std::string;
using std::tuple;
using std::vector;

/// Type of a record's block.
enum class ContentType : uint8_t {
  CUSTOM = 0, ///< Custom format, or unknown/unspecified data format
  EMPTY, ///< No data (interna).
  DATA_LAYOUT, ///< DataLayout block.
  IMAGE, ///< Image block.
  AUDIO, ///< Audio block.
  COUNT ///< Count of values in this enum type. @internal
};

string toString(ContentType contentType);

/// Image format type.
enum class ImageFormat : uint8_t {
  UNDEFINED = 0, ///< Unknown/unspecified.
  RAW, ///< Raw pixel data.
  JPG, ///< JPEG data.
  PNG, ///< PNG data.
  VIDEO, ///< Video codec encoded frame.
  COUNT ///< Count of values in this enum type. @internal
};

string toString(ImageFormat imageFormat);

/// Pixel format type, then the image format is ImageFormat::RAW.
enum class PixelFormat : uint8_t {
  UNDEFINED = 0, ///< Unknown/unrecognized.

  GREY8 = 1, ///< 1 uint8_t.
  BGR8, ///< 3 uint8_t values, blue + green + red.
  DEPTH32F, ///< 1 32 bit float value, representing a depth.
  RGB8, ///< 3 uint8_t values, red + green + blue.
  YUV_I420_SPLIT, ///< 3 uint8_t values, 4:2:0. The 3 planes are stored separately.
  RGBA8, ///< 4 uint8_t values, red + blue + green + alpha.
  RGB10, ///< uses 16 bit little-endian values. 6 most significant bits are unused and set to 0.
  RGB12, ///< uses 16 bit little-endian values. 4 most significant bits are unused and set to 0.
  GREY10, ///< uses 16 bit little-endian values. 6 most significant bits are unused and set to 0.
  GREY12, ///< uses 16 bit little-endian values. 4 most significant bits are unused and set to 0.
  GREY16, ///< uses 16 bit little-endian values.
  RGB32F, ///< 1 32 bit float value.
  SCALAR64F, ///< 1 64 bit float value, representing high precision image data.
  YUY2, ///< 4 uint8_t values, 4:2:2, single plane.
  RGB_IR_RAW_4X4, ///< As seen on the OV2312, a 4x4 pattern of BGRG GIrGIr RGBG GIrGIr
                  ///< where Ir means infrafred.
  RGBA32F, ///< 1 32 bit float value.
  BAYER8_RGGB, ///< 8 bit per pixel, RGGB bayer pattern.
  RAW10, /// https://developer.android.com/reference/android/graphics/ImageFormat#RAW10
  RAW10_BAYER_RGGB, ///< 10 bit per pixel, RGGB bayer pattern.

  COUNT, ///< Count of values in this enum type. @internal
};

string toString(PixelFormat pixelFormat);

/// Audio format type.
enum class AudioFormat : uint8_t {
  UNDEFINED = 0, ///< Unknown/unspecified.
  PCM = 1, ///< Raw PCM audio data.
  COUNT ///< Count of values in this enum type. @internal
};

string toString(AudioFormat audioFormat);

/// Audio sample format, when the audio type is AudioFormat::PCM.
enum class AudioSampleFormat : uint8_t {
  UNDEFINED = 0, ///< Unknown/unspecified.
  S8, ///< LPCM, signed, 8 bit.
  U8, ///< LPCM, unsigned, 8 bit.
  A_LAW, ///< a-law PCM, 8 bit.
  MU_LAW, ///< mu-law PCM, 8 bit.
  S16_LE, ///< LPCM, signed, 16 bit little endian.
  U16_LE, ///< LPCM, unsigned, 16 bit little endian.
  S16_BE, ///< LPCM, signed, 16 bit big endian.
  U16_BE, ///< LPCM, unsigned, 16 bit big endian.
  S24_LE, ///< LPCM, signed, 24 bit little endian.
  U24_LE, ///< LPCM, unsigned, 24 bit little endian.
  S24_BE, ///< LPCM, signed, 24 bit big endian.
  U24_BE, ///< LPCM, unsigned, 24 bit big endian.
  S32_LE, ///< LPCM, signed, 32 bit little endian.
  U32_LE, ///< LPCM, unsigned, 32 bit little endian.
  S32_BE, ///< LPCM, signed, 32 bit big endian.
  U32_BE, ///< LPCM, unsigned, 32 bit big endian.
  F32_LE, ///< LPCM, 32 bit float little endian.
  F32_BE, ///< LPCM, 32 bit float big endian.
  F64_LE, ///< LPCM, 64 bit float little endian.
  F64_BE, ///< LPCM, 64 bit float big endian.
  COUNT ///< Count of values in this enum type. @internal
};

string toString(AudioSampleFormat audioSampleFormat);

class ContentParser; ///< to workaround not being able to forward declare istringstream.
class RecordFormat;

/// Specification of an image content block.
class ImageContentBlockSpec {
 public:
  static constexpr uint8_t kQualityUndefined = 255;
  static constexpr double kInvalidTimestamp = -1E-308; // arbitrary unrealistic value

  ImageContentBlockSpec() {}

  /// Default copy constructor
  ImageContentBlockSpec(const ImageContentBlockSpec&) = default;
  ImageContentBlockSpec(
      const ImageContentBlockSpec& imageSpec,
      double keyFrameTimestamp,
      uint32_t keyFrameIndex);

  /// Specify-everything constructor
  ImageContentBlockSpec(
      ImageFormat imageFormat,
      PixelFormat pixelFormat,
      uint32_t width = 0,
      uint32_t height = 0,
      uint32_t stride = 0,
      const string& codecName = {},
      uint8_t codecQuality = kQualityUndefined,
      double keyFrameTimestamp = kInvalidTimestamp,
      uint32_t keyFrameIndex = 0);

  /// Image formats with encoding (png, jpeg, etc).
  ImageContentBlockSpec(ImageFormat imageFormat, uint32_t width = 0, uint32_t height = 0);

  /// Raw pixels image formats.
  ImageContentBlockSpec(
      PixelFormat pixelFormat,
      uint32_t width,
      uint32_t height,
      uint32_t stride = 0); // number of bytes between lines, if not width * sizeof(pixelFormat)

  /// Video image with codec.
  ImageContentBlockSpec(
      const string& codecName,
      uint8_t codecQuality,
      PixelFormat pixelFormat,
      uint32_t width,
      uint32_t height,
      uint32_t stride = 0);

  /// Constructor used for factory construction. @internal
  explicit ImageContentBlockSpec(const string& formatStr);

  /// When constructing from a string. @internal
  void set(ContentParser& parser);
  /// Clear/reset object to default values.
  void clear();

  /// Convert to string, to store on disk & reconstruct later using factory constructor. @internal
  string asString() const;

  /// Get the number of bytes for this content block, or ContentBlock::kSizeUnknown.
  /// For RAW images, that's the combined size of all the planes, though there is usually only 1.
  size_t getBlockSize() const;

  /// Get the number of bytes for this content block, or ContentBlock::kSizeUnknown.
  /// Use pixel format, dimensions and stride, and compute the size as if the image format is RAW.
  size_t getRawImageSize() const;

  /// Default copy assignment
  ImageContentBlockSpec& operator=(const ImageContentBlockSpec&) = default;

  /// Compare two image block spec.
  bool operator==(const ImageContentBlockSpec& rhs) const;
  bool operator!=(const ImageContentBlockSpec& rhs) const;

  /// Get image format.
  ImageFormat getImageFormat() const {
    return imageFormat_;
  }

  /// Get Image format as string.
  string getImageFormatAsString() const;

  /// Get Pixel format.
  PixelFormat getPixelFormat() const {
    return pixelFormat_;
  }
  /// Get pixel format presented as a readable string, from which it can be reconstructed.
  string getPixelFormatAsString() const;
  /// Get image width, or 0 if unknown/unspecified.
  uint32_t getWidth() const {
    return width_;
  }
  /// Get image height, or 0 if unknown/unspecified.
  uint32_t getHeight() const {
    return height_;
  }
  /// Get image stride (number of bytes between rows) for the first plane.
  uint32_t getStride() const;
  /// Get the raw stride parameter.
  /// Return 0 if no stride value was explicitly provided.
  uint32_t getRawStride() const {
    return stride_;
  }

  /// Get the number of planes for this pixel format.
  uint32_t getPlaneCount() const {
    return getPlaneCount(pixelFormat_);
  }

  /// Get the number of bytes of each line for a specific plane.
  /// Returns 0 if plane index is invalid.
  uint32_t getPlaneStride(uint32_t planeIndex) const;

  /// Get the number of lines in a specific plane.
  /// Returns 0 if plane index is invalid.
  uint32_t getPlaneHeight(uint32_t planeIndex) const;

  /// Get the number of channels of this format. Every pixel format has a channel count, but
  /// it does not tell how the pixel data is arranged in the image buffer (might not be contiguous).
  /// In other words, this should not be used to make assumptions on memory layout in any way.
  /// A value of 1 means the image is greyscale, or some form of 1 dimensional value, such as depth.
  uint8_t getChannelCountPerPixel() const {
    return getChannelCountPerPixel(pixelFormat_);
  }
  /// Get the size of a pixel format, in bytes.
  /// Compliant pixel formats use a fixed number of bytes per pixel, and pixels follow each other
  /// without overlap. Some bits might be unused, such as when using two bytes to store 10 bits.
  /// Pixel formats that don't work that way will return ContentBlock::kSizeUnknown
  size_t getBytesPerPixel() const {
    return getBytesPerPixel(pixelFormat_);
  }

  /// Get name of the video codec used to encode the image, if any.
  /// Only used for ImageFormat::VIDEO
  const string& getCodecName() const {
    return codecName_;
  }
  /// Get codec quality seeting used to encode the image, if any.
  /// 0 means codec-default, 100 means lossless
  /// Only used for ImageFormat::VIDEO
  uint8_t getCodecQuality() const {
    return isQualityValid(codecQuality_) ? codecQuality_ : kQualityUndefined;
  }

  /// Validate that a quality value is valid
  inline static bool isQualityValid(uint8_t quality) {
    return quality <= 100;
  }

  /// Get timestamp of the key frame of the group of frames this video frame belongs to.
  double getKeyFrameTimestamp() const {
    return keyFrameTimestamp_;
  }
  /// Get index of the frame in the group of frames this video frame belongs to.
  /// Key frames and only key frames (i-frames) have have an index of 0.
  /// The first p-frame after an i-frame has a key frame index of 1, and so on.
  uint32_t getKeyFrameIndex() const {
    return keyFrameIndex_;
  }

  /// Get the number channels of a pixel. See getChannelCountPerPixel()
  static uint8_t getChannelCountPerPixel(PixelFormat pixel);
  /// Get the size of a pixel, in bytes. See getBytesPerPixel()
  static size_t getBytesPerPixel(PixelFormat pixel);

  /// Get pixel format presented as a readable string, from which it can be reconstructed.
  inline string getPixelFormatAsString(PixelFormat pixelFormat) {
    return toString(pixelFormat);
  }

  /// Get the number of planes for this pixel format.
  static uint32_t getPlaneCount(PixelFormat pixelFormat);

 private:
  ImageFormat imageFormat_{ImageFormat::UNDEFINED};
  PixelFormat pixelFormat_{PixelFormat::UNDEFINED};
  uint32_t width_{0};
  uint32_t height_{0};
  uint32_t stride_{0};
  // for ImageFormat::VIDEO
  string codecName_;
  double keyFrameTimestamp_{kInvalidTimestamp};
  uint32_t keyFrameIndex_{0};
  uint8_t codecQuality_{kQualityUndefined};
};

/// Specification of an audio content block.
class AudioContentBlockSpec {
 public:
  AudioContentBlockSpec() {}
  /// Default copy constructor
  AudioContentBlockSpec(const AudioContentBlockSpec&) = default;
  /// For audio formats with encoding (mp3, flac, etc).
  explicit AudioContentBlockSpec(AudioFormat audioFormat, uint8_t channelCount = 0);
  /// For PCM audio formats.
  AudioContentBlockSpec(
      AudioSampleFormat sampleFormat,
      uint8_t channelCount = 0,
      uint32_t sampleRate = 0,
      uint32_t sampleCount = 0,
      uint8_t sampleBlockStride = 0);

  /// Constructor used for factory construction.
  /// @internal
  explicit AudioContentBlockSpec(const string& formatStr);

  /// when constructing from a string.
  /// @internal
  void set(ContentParser& parser);
  /// Clear/reset object to default values.
  void clear();

  /// Convert to string, to store on disk & reconstruct later using factory constructor.
  /// @internal
  string asString() const;

  /// Get the number of bytes for this content block, or ContentBlock::kSizeUnknown.
  size_t getBlockSize() const;

  /// Default copy assignment
  AudioContentBlockSpec& operator=(const AudioContentBlockSpec&) = default;

  /// Compare two audio block spec.
  bool operator==(const AudioContentBlockSpec& rhs) const {
    return sampleFormat_ == rhs.sampleFormat_ && channelCount_ == rhs.channelCount_ &&
        sampleBlockStride_ == rhs.sampleBlockStride_ && sampleCount_ == rhs.sampleCount_ &&
        sampleRate_ == rhs.sampleRate_;
  }
  bool operator!=(const AudioContentBlockSpec& rhs) const {
    return !operator==(rhs);
  }
  /// Tell if two audio block have identical audio formats.
  bool isCompatibleWith(const AudioContentBlockSpec& rhs) const {
    return sampleFormat_ == rhs.sampleFormat_ && channelCount_ == rhs.channelCount_ &&
        sampleRate_ == rhs.sampleRate_;
  }
  /// Get audio format.
  AudioFormat getAudioFormat() const {
    return audioFormat_;
  }
  /// Get audio sample format.
  AudioSampleFormat getSampleFormat() const {
    return sampleFormat_;
  }
  /// Get audio sample format as a string.
  string getSampleFormatAsString() const;
  /// Tell if the audio sample format is little endian.
  bool isLittleEndian() const {
    return isLittleEndian(sampleFormat_);
  }
  /// Tell if the audio sample format is an IEEE float.
  bool isIEEEFloat() const {
    return isIEEEFloat(sampleFormat_);
  }
  /// Get the number of bits per audio sample.
  uint8_t getBitsPerSample() const {
    return getBitsPerSample(sampleFormat_);
  }
  /// Get the number of bytes per audio sample.
  uint8_t getBytesPerSample() const {
    return (getBitsPerSample(sampleFormat_) + 7) / 8; // round up
  }
  /// Number of bytes used by group of synchronous audio samples, including padding
  uint8_t getSampleBlockStride() const;
  /// Get the number of audio channels in a sample block (not per content block).
  uint8_t getChannelCount() const {
    return channelCount_;
  }
  /// Get the audio sample rate.
  uint32_t getSampleRate() const {
    return sampleRate_;
  }
  /// Get the number of audio sample blocks in the block.
  uint32_t getSampleCount() const {
    return sampleCount_;
  }
  /// Tell if the audio sample format is fully defined.
  /// For instance, PCM audio data when we have enough details: sample format & channel count.
  bool isSampleBlockFormatDefined() const {
    switch (audioFormat_) {
      case AudioFormat::PCM:
        return sampleFormat_ != AudioSampleFormat::UNDEFINED && channelCount_ != 0;
      case AudioFormat::UNDEFINED:
      case AudioFormat::COUNT:
      default:
        return false;
    }
  }

  /// Tell if a specific audio sample format is little endian.
  static bool isLittleEndian(AudioSampleFormat sampleFormat);
  /// Tell if a specific audio sample format is an IEEE float.
  static bool isIEEEFloat(AudioSampleFormat sampleFormat);
  /// Get the number of bits per audio sample for a specific audio sample format.
  static uint8_t getBitsPerSample(AudioSampleFormat sampleFormat);
  /// Get an audio sample format as a string.
  inline string getSampleFormatAsString(AudioSampleFormat sampleFormat) {
    return toString(sampleFormat);
  }

 private:
  AudioFormat audioFormat_;
  AudioSampleFormat sampleFormat_;
  uint8_t sampleBlockStride_;
  uint8_t channelCount_;
  uint32_t sampleRate_;
  uint32_t sampleCount_;
};

/// Specification of a VRS record content block.
///
/// VRS records are described by RecordFormat as a succession of ContentBlocks,
/// which each describe the data they contain.
///
/// Each ContentBlock has a type, described by a ContentType. Typical types are:
///  - DataLayout
///  - Image
///  - Audio
///  - Custom
///
/// Each block may have a fixed size, or an unknown size if the size might vary between records.
class ContentBlock {
 public:
  /// Special value used to represent an unknown block size.
  static const size_t kSizeUnknown;

  /// Very generic block description.
  ContentBlock(ContentType type = ContentType::EMPTY, size_t size = kSizeUnknown);

  /// Factory-style reconstruct from persisted description as string on disk.
  explicit ContentBlock(const string& formatStr);

  /// Image formats with encoding (png, jpeg, etc).
  ContentBlock(ImageFormat imageFormat, uint32_t width = 0, uint32_t height = 0);

  /// Image formats with custom codec encoding.
  ContentBlock(
      const std::string& codecName,
      uint8_t codecQuality,
      PixelFormat pixelFormat = PixelFormat::UNDEFINED,
      uint32_t width = 0,
      uint32_t height = 0,
      uint32_t stride = 0);

  /// Raw image formats: a PixelFormat and maybe resolutions & raw stride.
  ContentBlock(PixelFormat pixelFormat, uint32_t width, uint32_t height, uint32_t stride = 0);

  ContentBlock(const ImageContentBlockSpec& imageSpec, size_t size = kSizeUnknown);
  ContentBlock(
      const ContentBlock& imageContentBlock,
      double keyFrameTimestamp,
      uint32_t keyFrameIndex);

  /// Very generatic audio block description.
  ContentBlock(AudioFormat audioFormat, uint8_t channelCount = 0);

  /// PCM audio block description.
  ContentBlock(
      AudioSampleFormat sampleFormat,
      uint8_t numChannels = 0,
      uint32_t sampleRate = 0,
      uint32_t sampleCount = 0,
      uint8_t sampleBlockStride = 0);

  /// Default copy constructor
  ContentBlock(const ContentBlock&) = default;

  /// Copy constructor, except for the size.
  ContentBlock(const ContentBlock& other, size_t size)
      : contentType_{other.contentType_}, size_{size} {
    if (contentType_ == ContentType::IMAGE) {
      imageSpec_ = other.imageSpec_;
    } else if (contentType_ == ContentType::AUDIO) {
      audioSpec_ = other.audioSpec_;
    }
  }

  /// Default copy assignment
  ContentBlock& operator=(const ContentBlock& rhs) = default;

  /// Conversion to string, to store on disk & reconstruct later using constructor.
  /// @Internal
  string asString() const;

  /// Get the content block size, if available or calculable.
  size_t getBlockSize() const;

  /// Get the ContentType of the block.
  ContentType getContentType() const {
    return contentType_;
  }

  bool operator==(const ContentBlock& rhs) const {
    // first compare generic content blocks
    if (contentType_ != rhs.contentType_ || size_ != rhs.size_) {
      return false;
    }
    // now specific parts for given format type
    switch (contentType_) {
      case ContentType::IMAGE:
        return imageSpec_ == rhs.imageSpec_;
      case ContentType::AUDIO:
        return audioSpec_ == rhs.audioSpec_;
      default:
        // non-specific content type
        return true;
    }
  }

  bool operator!=(const ContentBlock& rhs) const {
    return !operator==(rhs);
  }

  /// Assembly operator, to construct a RecordFormat for ContentBlock parts.
  RecordFormat operator+(const ContentBlock&) const;

  /// Get the image content spec. Requires the content block to be of type ContentType::IMAGE.
  const ImageContentBlockSpec& image() const;
  /// Get the audio content spec. Requires the content block to be of type ContentType::AUDIO.
  const AudioContentBlockSpec& audio() const;

 private:
  ContentType contentType_ = ContentType::EMPTY;
  size_t size_ = kSizeUnknown;
  ImageContentBlockSpec imageSpec_;
  AudioContentBlockSpec audioSpec_;
};

/// Description of the format of a VRS record as a succession of typed blocks of content.
/// A RecordFormat description can be associated with each record type & record format version,
/// so that we can tell how a particular type of VRS record looks like.
///
/// Major ContentBlock types include DataLayout blocks and image blocks.
/// See enum class ContentType above for the complete list of ContentBlock types.
///
/// See ContentBlock to find out about all the different types of content blocks supported.
class RecordFormat {
 public:
  /// Empty record format definition.
  RecordFormat() {}
  /// Default copy constructor
  RecordFormat(const RecordFormat&) = default;

  /// Build a RecordFormat from a string description.
  /// This constructor is meant for internal VRS usage only.
  /// @internal
  RecordFormat(const string& format) {
    set(format);
  }
  /// Build a RecordFormat from a string description.
  /// This constructor is meant for internal VRS usage only.
  /// @internal
  RecordFormat(const char* format) {
    set(format);
  }
  /// Build a RecordFormat from a single ContentBlock.
  RecordFormat(const ContentBlock& block) {
    blocks_.emplace_back(block);
  }
  /// Build a RecordFormat from a single ContentBlock.
  RecordFormat(ContentBlock&& block) {
    blocks_.emplace_back(block);
  }
  /// Build a RecordFormat from two ContentBlock definitions.
  RecordFormat(const ContentBlock& first, const ContentBlock& second) {
    blocks_.emplace_back(first);
    blocks_.emplace_back(second);
  }
  /// Build a RecordFormat from two ContentBlock definitions.
  RecordFormat(const ContentBlock&& first, const ContentBlock&& second) {
    blocks_.emplace_back(first);
    blocks_.emplace_back(second);
  }
  /// Build a RecordFormat from a simple ContentType & block size.
  /// @param type: Content type of the block.
  /// @param size: Size of the block, or ContentBlock::kSizeUnknown, if the size of the block isn't
  /// known/fixed.
  RecordFormat(ContentType type, size_t size = ContentBlock::kSizeUnknown) {
    blocks_.emplace_back(ContentBlock(type, size));
  }

  /// Append a ContentBlock to this RecordFormat.
  RecordFormat& operator+(const ContentBlock& block) {
    blocks_.emplace_back(block);
    return *this;
  }
  /// Append a ContentBlock to this RecordFormat.
  RecordFormat& operator+(ContentBlock&& block) {
    blocks_.emplace_back(block);
    return *this;
  }

  /// Default copy assignment
  RecordFormat& operator=(const RecordFormat&) = default;
  bool operator==(const RecordFormat& rhs) const {
    size_t usedBlocksCount = getUsedBlocksCount();
    if (usedBlocksCount != rhs.getUsedBlocksCount()) {
      return false;
    }
    for (size_t k = 0; k < usedBlocksCount; ++k) {
      if (getContentBlock(k) != rhs.getContentBlock(k)) {
        return false;
      }
    }
    return true;
  }
  bool operator!=(const RecordFormat& rhs) const {
    return !operator==(rhs);
  }

  void clear() {
    blocks_.clear();
  }

  /// Initialize from a string description. For VRS usage only.
  /// @internal
  void set(const string& format);
  /// Convert as a string. For VRS usage only.
  /// @internal
  string asString() const;

  /// Get the size of the record, based on RecordFormat information only.
  /// @return The size of the record, or ContentBlock::kSizeUnknown.
  size_t getRecordSize() const;
  /// Get the number of blocks to read (ignores trailing empty blocks).
  /// @return The number of blocks until the last block, that's not an empty block.
  size_t getUsedBlocksCount() const;
  /// Count the number of blocks of a particular type.
  /// @param type: Content block type to count.
  /// @return The number of content blocks of the requested type.
  size_t getBlocksOfTypeCount(ContentType type) const;
  /// Get a specific ContentBlock. Use getUsedBlocksCount() to know when to stop.
  /// @param index: The index of the block requested.
  /// @return The requested ContentBlock, or an empty block, if there is no block at that index.
  const ContentBlock& getContentBlock(size_t index) const {
    if (index < blocks_.size()) {
      return blocks_[index];
    }
    static ContentBlock sEmptyBlock;
    return sEmptyBlock;
  }
  /// Get first content block.
  /// @return The first ContentBlock, or an empty block, if there are no ContentBlock defined.
  const ContentBlock& getFirstContentBlock() const {
    return getContentBlock(0);
  }
  /// Get the size of all the blocks at and past an index, or ContentBlock::kSizeUnknown.
  /// @param firstBlock: Size of the first content block to count the size of.
  /// @return The size of all the content blocks at and past the index, or
  /// ContentBlock::kSizeUnknown if any of the blocks doesn't have a known size.
  size_t getRemainingBlocksSize(size_t firstBlock) const;
  /// Get the size of a particular block, knowing the remaning record size including this block, or
  /// ContentBlock::kSizeUnknown.
  /// @param firstBlock: Size of the first content block to count the size of.
  /// @param remainingSize: Number of bytes left in the record for the remaining blocks.
  /// @return The size of all the content blocks at and past the index, or
  /// ContentBlock::kSizeUnknown if more than one block doesn't have a known size.
  size_t getBlockSize(size_t blockIndex, size_t remainingSize) const;

  // Static VRS dedicated helper methods, for RecordFormat & DataLayout purposes.

  /// Names of the VRS stream tags used for RecordFormat descriptions.
  static string getRecordFormatTagName(Record::Type recordType, uint32_t formatVersion);
  static string getDataLayoutTagName(Record::Type type, uint32_t version, size_t blockIndex);
  /// Tell if a tag name was generated by getRecordFormatTagName, by returning true,
  /// and setting recordType & formatVersion. Otherwise, it returns false.
  static bool parseRecordFormatTagName(
      const string& tagName,
      Record::Type& recordType,
      uint32_t& formatVersion);

 private:
  vector<ContentBlock> blocks_;
};

/// Map a pair of record type/format version to a record format, for a particular stream.
typedef map<pair<Record::Type, uint32_t>, RecordFormat> RecordFormatMap;

/// A ContentBlock is the part of a record, a defined by RecordFormat.
/// A ContentBlock is uniquely identified by this combo:
/// - a recordable type id,
/// - a record type,
/// - a record format version,
/// - a block index.
class ContentBlockId {
 public:
  ContentBlockId(
      RecordableTypeId typeId,
      Record::Type recordType,
      uint32_t formatVersion,
      size_t blockIndex)
      : typeId(typeId),
        recordType(recordType),
        formatVersion(formatVersion),
        blockIndex(blockIndex) {}

  RecordableTypeId getRecordableTypeId() const {
    return typeId;
  }

  Record::Type getRecordType() const {
    return recordType;
  }

  uint32_t getFormatVersion() const {
    return formatVersion;
  }

  size_t getBlockIndex() const {
    return blockIndex;
  }

 private:
  RecordableTypeId typeId;
  Record::Type recordType;
  uint32_t formatVersion;
  size_t blockIndex;
};

} // namespace vrs
