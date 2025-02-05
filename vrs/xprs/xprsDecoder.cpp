// (c) Meta Platforms, Inc. and affiliates. Confidential and proprietary.

//
// CVideoDecoder class is an implementation of the IVideoDecoder interface class.
// See xprsDecoder.h for details.

#include "xprsDecoder.h"
#ifdef WITH_NVCODEC
#include "cudaContextProvider.h"
#endif
#include "Codecs.h"
#include "xprsUtils.h"

namespace xprs {

CVideoDecoder::CVideoDecoder(const VideoCodec& codec) : _timeStamp{0} {
  *((VideoCodec*)this) = codec;
  _decoder = nullptr;
}

CVideoDecoder::~CVideoDecoder() {}

XprsResult CVideoDecoder::init(const std::string& logFolderPath) {
  XprsResult result = XprsResult::OK;
  try {
#ifdef WITH_NVCODEC
    if (implementationName == kNvH264DecoderName || implementationName == kNvH265DecoderName ||
        implementationName == kNvAv1DecoderName) {
      const NvCodecContext nvcodec_context = NvCodecContextProvider::getNvCodecContext();
      const cudaVideoCodec cuda_video_codec = codecNameToCudaVideoCodecEnum(implementationName);
      _decoder.reset(new NvDecoder(nvcodec_context, cuda_video_codec));
    } else
#endif
    {
      _decoder.reset(new VideoDecode(implementationName.c_str()));
    }
    _decoder->open();
  } catch (std::exception& e) {
    ERR_LOG(convertExceptionToError(e, result));
    _decoder.reset();
  }

  return result;
}

XprsResult CVideoDecoder::decodeFrame(Frame& frameOut, const Buffer& compressed) {
  if (!_decoder) {
    return XprsResult::ERR_NOT_INITIALIZED;
  }

  XprsResult result = XprsResult::OK;

  try {
    _decoder->decode(compressed.data, compressed.size, _pix);
    if (_pix.avFrame()->flags & AV_FRAME_FLAG_CORRUPT) {
      result = XprsResult::ERR_CORRUPT_DATA;
    } else if (_pix.avFrame()->flags & AV_FRAME_FLAG_DISCARD) {
      result = XprsResult::ERR_NO_FRAME;
    } else {
      _pix.avFrame()->pts = _timeStamp++; // we create an artificial pts because we are dealing with
                                          // raw bitstreams, which don't have this information
      convertAVFrameToFrame(_pix.avFrame(), frameOut);
    }
  } catch (std::exception& e) {
    ERR_LOG(convertExceptionToError(e, result));
  }

  return result;
}

} // namespace xprs
