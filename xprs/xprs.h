/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/**
 * XPRS is a compression API built on top of FFmpeg. It provides a simplified image
 * and video compression API so users can focus on building applications without having to
 * deal with the complicated APIs and various encoder settings in FFmpeg.
 *
 * https://fburl.com/xprs
 *
 */

#pragma once

#include <cinttypes>
#include <string>
#include <string_view>
#include <vector>

namespace xprs {

/**
 * Possible result for XPRS API calls.
 */
enum class XprsResult : int {
  OK = 0,

  ERR_GENERIC = -1,
  ERR_INVALID_CONFIG = -2,
  ERR_OOM = -3,
  ERR_NO_FRAME = -4,
  ERR_SYSTEM = -5,
  ERR_FFMPEG = -6,
  ERR_NOT_INITIALIZED = -7,
  ERR_INVALID_FRAME = -8,
  ERR_CORRUPT_DATA = -9,
  ERR_INVALID_INPUT = -10,
  ERR_MUX_FAILURE = -11,
  ERR_NOT_IMPLEMENTED = -12,

  ERR_UNKNOWN = -999
};

/**
 * Supported pixel format.
 * Note that not all formats will be supported by all codecs.
 * YUV formats must come before RGB formats. RGB24 must be first RGB format.
 */
enum class PixelFormat : uint32_t {
  /**
   * Unknown format
   */
  UNKNOWN,

  /**
   * Y,  8bpp
   */
  GRAY8,

  /**
   * Y, 10bpp, little-endian
   */
  GRAY10LE,

  /**
   * Y, 12bpp, little-endian
   */
  GRAY12LE,

  /**
   * Planar YUV 4:2:0, 12bpp, (1 Cr & Cb sample per 2x2 Y samples)
   */
  YUV420P,

  /**
   * planar YUV 4:2:0, 12bpp, 1 plane for Y and 1 plane for the UV components, which are interleaved
   * (first byte U and the following byte V)
   */
  NV12,

  /**
   * Planar YUV 4:2:0, 15bpp, (1 Cr & Cb sample per 2x2 Y samples), little-endian
   */
  YUV420P10LE,

  /**
   * Planar YUV 4:2:0, 18bpp, (1 Cr & Cb sample per 2x2 Y samples), little-endian
   */
  YUV420P12LE,

  /**
   * Planar YUV 4:2:2, 16bpp, (1 Cr & Cb sample per 2x1 Y samples)
   */
  YUV422P,

  /**
   * Planar YUV 4:4:4, 24bpp, (1 Cr & Cb sample per 1x1 Y samples)
   */
  YUV444P,

  /**
   * Packed RGB 8:8:8, 24bpp, RGBRGB... (must be first RGB format)
   */
  RGB24,

  /**
   * Planer GBR 8 bit, 24bpp, GGG...G, BBB...B, RRR...R
   */
  GBRP,

  /**
   * Planer GBR 10 bit, 30bpp, GGG...G, BBB...B, RRR...R
   */
  GBRP10LE,

  /**
   * Planer GBR 12 bit, 36bpp, GGG...G, BBB...B, RRR...R
   */
  GBRP12LE,

  /**
   * NV 12, 10 bit, little-endian
   */
  NV1210LE,

  /**
   * NV 12, 12 bit, little-endian
   */
  NV1212LE,

  COUNT,
};

/**
 * Maximum number of data planes.
 */
constexpr int MAX_NUM_PLANES = 4;

/**
 * For PTS, etc.
 */
using TimeStamp = int64_t;

/**
 * Buffer pointers and meta data about a frame.
 */
struct Frame {
  /**
   * Presentation timestamp in milliseconds. Returned on output.
   */
  TimeStamp ptsMs = 0;

  /**
   * Pointer to pixel buffers of individual planes. The actual number of planes is decided by fmt.
   */
  uint8_t* planes[MAX_NUM_PLANES] = {};

  /**
   * Strides for individual planes. Stride is the number of bytes between row starts.
   */
  uint16_t stride[MAX_NUM_PLANES] = {};

  /**
   * Number of valid planes and strides for this frame (see above for 'MAX_NUM_PLANES'); unused
   * pointers to planes set to null.
   */
  int numPlanes = 0;

  /**
   * Pixel format of the frame. May be UNKNOWN, in which case encoder will use
   * EncoderConfig::inputFmt.
   */
  PixelFormat fmt = PixelFormat::UNKNOWN;

  /**
   * Width of the picture. May be 0, in which case encoder will use EncoderConfig::width..
   */
  uint16_t width = 0;

  /**
   * Height of the picture. May be 0, in which case encoder will use EncoderConfig::height.
   */
  uint16_t height = 0;

  /**
   * Custom frame encoding parameters for external rate control and loss handling. These
   * should be left to 0 or false for most applications.
   */

  /**
   * For encoding: true forces the current frame to be a key (IDR) frame, false indicates
   * the encoder should decide. For decoding: true indicates the current frame was decoded
   * from a key (IDR) frame, false indicates otherwise.
   */
  bool keyFrame = false;
};

/**
 * Used to set the video encoder configuration.
 */
struct EncoderConfig {
  /**
   * Width of encoded video frame. Incoming frames will be rejected if their dimensions differ.
   */
  uint16_t width = 0;

  /**
   * Height of encoded video frame. Incoming frames will be rejected if their dimensions differ.
   */
  uint16_t height = 0;

  /**
   * Encoded video format. Also the expected format of video frames to be passed to the encoder,
   * unless a format is specified for the frame.
   */
  PixelFormat encodeFmt = PixelFormat::YUV420P;

  /**
   * Key frame interval. Number of frames before a key frame is encoded. 0 or 1 means every frame is
   * a key frame.
   */
  int32_t keyDistance = 60;

  /**
   * Sets the quality of encoding 1-100 with higher number indicating better quality. Quality 60
   * maps to QP 20 in H.264 and H.265, which is already very good visual quality. Encoded video size
   * increases by 5~10% for every quality step. Internally this maps to Constant Rate Factor in
   * x264/x265 and libvpx. 100 = lossless, 0 = codec default.
   */
  uint8_t quality = 0;

  /**
   * Trade off performance for compression efficiency: slow, medium or fast. Predefined tuning for a
   * scenario. Slow takes longer to encode, but produces better results. Maps to codec specific
   * presets such that each tuning provides similar results across all codecs.
   */
  std::string preset = "medium";

  /**
   * Suppress verbose debugging message.
   */
  bool suppressNonFatalMessage = false;
};

/**
 * Codec format identifiers for encode and decode.
 */
enum class VideoCodecFormat : uint32_t {
  FIRST,
  H264 = FIRST,
  H265,
  VP9,
  AV1,

  LAST
};

using VideoCodecName = std::string;

/**
 * Identify encoders and decoders.
 */
struct VideoCodec {
  /**
   * Identifier of the codec format.
   */
  VideoCodecFormat format = VideoCodecFormat::H264;

  /**
   * Name of the codec implementation.
   */
  VideoCodecName implementationName;

  /**
   * Whether the codec is hardware accelearated.
   */
  bool hwAccel = false;
};

using CodecList = std::vector<VideoCodec>;
using PixelFormatList = std::vector<PixelFormat>;

/**
 * Enumerate all available encoders.
 * @param codecs Returns the list of available encoders.
 * @param hwCapabilityCheck If true, only list HW encoders if GPU has hardware
 * encoder. Some GPUs like A100, A30, H100 do not have HW encoder but still have HW decoder.
 * @return XprsResult
 */
XprsResult enumEncoders(CodecList& codecs, bool hwCapabilityCheck = true);

/**
 * Enumerate all available decoders.
 * @param codecs Returns the list of available decoders.
 * @param hwCapabilityCheck If true, only list HW decoders if GPU has one.
 * Some GPUs like A100, A30, H100 do not have HW AV1 decoder but have H264 and H265.
 * @return XprsResult
 */
XprsResult enumDecoders(CodecList& codecs, bool hwCapabilityCheck = true);

/**
 * Enumerate all available encoders for a give codec. In the beginning before we have HW support,
 * this would return just 1 codec.
 * @param codecs Returns the list of available encoders specified by \p standard.
 * @param standard Specifies the codec standard to enumerate.
 * @param hwCapabilityCheck If true, only list HW encoders if GPU has hardware
 * encoder. Some GPUs like A100, A30, H100 do not have HW encoder but still have HW decoder.
 * @return XprsResult
 */
XprsResult
enumEncodersByFormat(CodecList& codecs, VideoCodecFormat standard, bool hwCapabilityCheck = true);

/**
 * Enumerate all available decoders for a give codec. In the beginning before we have HW support,
 * this would return just 1 codec.
 * @param codecs Returns the list of available decoders specified by \p standard.
 * @param standard Specifies the codec standard to enumerate.
 * @param hwCapabilityCheck If true, only list HW decoders if GPU has one.
 * Some GPUs like A100, A30, H100 do not have HW AV1 decoder but have H264 and H265.
 * @return XprsResult
 */
XprsResult
enumDecodersByFormat(CodecList& codecs, VideoCodecFormat standard, bool hwCapabilityCheck = true);

extern const std::string_view SUPPORTED_CODEC_FORMATS[];

/**
 * Converts an enumerated video codec format \p standard to its generic string moniker.
 * @param videoCodecStr Returns the string moniker for a codec standard.
 * @param standard Specifies the codec standard.
 * @return XprsResult
 */
XprsResult getNameFromVideoCodecFormat(std::string& videoCodecStr, VideoCodecFormat standard);

/**
 * Converts an enumerated video codec format \p standard to its generic string moniker.
 *
 * @param standard Specifies the codec standard.
 * @return Name of the codec standard as a string. "Unknown" if \p standard is not valid.
 */
const char* getNameFromVideoCodecFormat(VideoCodecFormat standard);

/**
 * Converts a video codec format generic string moniker \p videoCodecStr to its enumerated video
 * codec format \p standard
 * @param standard Returns the codec standard specified by string \p videoCodecStr
 * @param videoCodecStr Specifies the string moniker.
 * @return XprsResult
 */
XprsResult getVideoCodecFormatFromName(
    VideoCodecFormat& standard,
    const VideoCodecName& videoCodecStr);

/**
 * Check if the indicated pixel format is supported by the indicated codec implementation.
 * @param result Indicates whether the format is supported.
 * @param implementation The name of the codec to check.
 * @param format The format to check.
 * @return XprsResult
 */
XprsResult
isValidPixelFormat(bool& result, const VideoCodecName& implementation, PixelFormat format);

/**
 * Create a list of pixel formats supported by the indicated codec implementation.
 * @param formats Returns the list of formats supported by a codec implementation.
 * @param implementation The codec implementation to check for.
 * @return XprsResult
 */
XprsResult enumPixelFormats(PixelFormatList& formats, const VideoCodecName& implementation);

/**
 * Return the number of planes used by \p pixelFmt or 0 on failure.
 * @param pixelFmt Pixel format.
 */
int getNumPlanes(xprs::PixelFormat pixelFmt);

/**
 * Convert a PixelFormat to text
 * @param pixelFormat The pixel format to convert.
 * @return Name of the pixel format as a string.
 */
const char* getPixelFormatName(PixelFormat pixelFormat);

/**
 * Convert a PixelFormat to text
 * @param pixelFormatName String name of a pixel format.
 * @return PixelFormat
 */
PixelFormat getPixelFormatFromName(const char* pixelFormatName);

/**
 * Get error message for an error code.
 * @param error Error code.
 * @return const char* A description of the error code.
 */
const char* getErrorMessage(XprsResult error);

/**
 * Compressed video buffer.
 */
struct Buffer {
  /**
   * Size in bytes
   */
  size_t size;

  /**
   * Pointer to encoded data
   */
  uint8_t* data;
};

/**
 * Stores the output data from encoder.
 */
struct EncoderOutput {
  /**
   * Compressed video output buffer.
   */
  Buffer buffer;

  /**
   * Presentation timestamp in milliseconds of the encoded frame.
   */
  TimeStamp ptsMs;

  /**
   * Whether the output contains a key frame.
   */
  bool isKey;
};

constexpr int INPUT_BUFFER_PADDING_SIZE = 64;

/**
 * Encoder interface
 */
class IVideoEncoder {
 public:
  virtual ~IVideoEncoder() {}

  /**
   * Invalidate the current encoder configuration, if any, and initialize a new encoder.
   * @param config Encoder configuration.
   * @return XprsResult
   */
  virtual XprsResult init(const EncoderConfig& config) = 0;

  /**
   * Encode a frame. This API mostly wraps the avcodec_encode_video2() API in libavcodec.
   * Buffers in out will be allocated by encoder. The buffer persists through encoder lifetime and
   * will be overwritten during the next call to encodeFrame(). Caller should ensure it finishes
   * using the buffers before calling encodeFrame() again. frameIn - input frame out - contains
   * compressed output bitstream. Buffer will be allocated by encoder. The returned buffer
   * belongs to the decoder and is valid only until the next call to this function or until
   * deleting the encoder. The caller may not write to it.
   * @param out Returns the compressed frame.
   * @param frameIn Input frame.
   * @return XprsResult::OK on success, XprsResult::ERR_NOT_INITIALIZED on bad initialization,
   * XprsResult::ERR_INVALID_FRAME when frameIn is invalid because decoder encountered errors,
   * XprsResult::ERR_NO_FRAME when no frame is available for decoding, XprsResult::ERR_FFMPEG on
   * FFmpeg error, XprsResult::ERR_SYSTEM on system error (e.g., insufficient memory)
   */
  virtual XprsResult encodeFrame(EncoderOutput& out, const Frame& frameIn) = 0;
};

/**
 * Decoder interface
 */
class IVideoDecoder {
 public:
  virtual ~IVideoDecoder() {}

  /**
   * Initialize the decoder.
   * @param disableHwAcceleration If true, disable automatic hardware acceleration on MacOS.
   * @return XprsResult
   */
  virtual XprsResult init(bool disableHwAcceleration = false) = 0;

  /**
   * Given a compressed frame buffer, decode the frame. This API mostly wraps the
   * avcodec_decode_video2() API in libavcodec. Here we assume the input buffer contains the data
   * for a whole frame, including the necessary headers if it's the first frame. The input buffer
   * must be INPUT_BUFFER_PADDING_SIZE larger than the actual read bytes because some optimized
   * bitstream readers read 32 or 64 bits at once and could read over the end. The end of the input
   * buffer should be set to 0.
   * @param frameOut Decoded frame. The codec will allocate memory for the actual bitmap. The
   * returned buffer belongs to the decoder and is valid only until the next call to this function
   * or until deleting the decoder. The caller may not write to it.
   * @param compressed Input buffer containing the compressed frame.
   * @return XprsResult
   */
  virtual XprsResult decodeFrame(Frame& frameOut, const Buffer& compressed) = 0;
};

/**
 * Video stream configuration for mp4 muxer.
 */
struct MuxerVideoStreamConfig {
  /**
   * Codec name
   */
  std::string codec;

  /**
   * Width of the video
   */
  uint16_t width;

  /**
   * Height of the video
   */
  uint16_t height;
};

/**
 * Video muxer interface
 */
class IVideoMuxer {
 public:
  virtual ~IVideoMuxer() {}

  /**
   * Specify the output media file path.
   * @param mediaFilePath The output media path.
   */
  virtual XprsResult open(const std::string& mediaFilePath) = 0;

  /**
   * Add a video stream to the media file. Currently it can only mux one single video stream, so
   * this method can only be called once.
   * @param config Configuration for the video stream to be muxed.
   * @return XprsResult
   */
  virtual XprsResult addVideoStream(const MuxerVideoStreamConfig& config) = 0;

  /**
   * Write a compressed video frame to the media file.
   * @param frame Compressed video frame. This can be an output frame obtained from the encoder
   * interface IVideoEncoder.
   * @return XprsResult
   */
  virtual XprsResult muxFrame(const EncoderOutput& frame) = 0;

  /**
   * Write the stream trailer to the output media file and free the IO context.
   * @return XprsResult
   */
  virtual XprsResult close() = 0;
};

/**
 * Creates an encoder instance.
 * @param codec Identifies type of codec to create.
 * @return Encoder instance on success or nullptr on failure. The returned pointer should be deleted
 * by the caller.
 */
IVideoEncoder* createEncoder(const VideoCodec& codec);

/**
 * Creates a decoder instance.
 * @param codec Identifies type of codec to create.
 * @return Decoder instance on success or nullptr on failure. The returned pointer should be deleted
 * by the caller.
 */
IVideoDecoder* createDecoder(const VideoCodec& codec);

/**
 * Create a video muxer.
 * @return Muxer instance on sucess or nullptr on failure. The returned pointer should be deleted by
 * the caller.
 */
IVideoMuxer* createMuxer();

} // namespace xprs
