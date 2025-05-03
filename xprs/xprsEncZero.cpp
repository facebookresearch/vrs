// (c) Meta Platforms, Inc. and affiliates. Confidential and proprietary.

//
// See xprs.h for details.
// This implementation is for unsupported platforms, for which we don't have ffmpeg.
// It provides no codec of any kind, but satisfies the APIs, allowing for client
// apps to be easily built for non-xprs supported platforms without having to
// explicitly handle that xprs doesn't work in that environment.

#include "xprs.h"

namespace xprs {

///
/// Enumerate all available encoders: none...
///
XprsResult enumEncoders(CodecList& codecs, [[maybe_unused]] bool hwCapabilityCheck) {
  codecs.clear();
  return XprsResult::ERR_NOT_IMPLEMENTED;
}

///
/// Enumerate all available encoders for a give codec. In the beginning before we have HW support,
/// this would return just 1 codec.
///
XprsResult enumEncodersByFormat(
    CodecList& codecs,
    [[maybe_unused]] VideoCodecFormat standard,
    [[maybe_unused]] bool hwCapabilityCheck) {
  codecs.clear();
  return XprsResult::ERR_NOT_IMPLEMENTED;
}

///
/// Check if the indicated pixel 'format' is supported by the indicated codec 'implementation' and
/// set 'result' to true if so.
///
XprsResult isValidPixelFormat(
    [[maybe_unused]] bool& result,
    [[maybe_unused]] const VideoCodecName& implementation,
    [[maybe_unused]] PixelFormat format) {
  return XprsResult::ERR_NOT_IMPLEMENTED;
}

///
/// Create a list of pixel 'formats' supported by the indicated codec 'implementation'.
///
XprsResult enumPixelFormats(
    [[maybe_unused]] PixelFormatList& formats,
    [[maybe_unused]] const VideoCodecName& implementation) {
  return XprsResult::ERR_NOT_IMPLEMENTED;
}

IVideoEncoder* createEncoder([[maybe_unused]] const VideoCodec& codec) {
  return nullptr;
}

IVideoMuxer* createMuxer() {
  return nullptr;
}

} // namespace xprs
