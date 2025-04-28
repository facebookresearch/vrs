// (c) Meta Platforms, Inc. and affiliates. Confidential and proprietary.

//
// See FFmpegDecode.h for details.

#include "FFmpegDecode.h"

#include <logging/Log.h>
#define DEFAULT_LOG_CHANNEL "XPRS"

namespace xprs {

VideoDecode::VideoDecode(const char* avcodecName, [[maybe_unused]] bool disableHwAcceleration) {
  if ((_avCodec = avcodec_find_decoder_by_name(avcodecName)) == nullptr) {
    throw INVOKE_CODEC_EXCEPTION_MESSAGE("avcodec_find_decoder failed to find codec");
  }

  if ((_avContext = avcodec_alloc_context3(_avCodec)) == nullptr) {
    throw INVOKE_CODEC_EXCEPTION_MESSAGE("avcodec_alloc_context3 failed");
  }

  // Use HW if available on Mac.
#ifdef __APPLE__
  if (!disableHwAcceleration) {
    // Set up hardware acceleration
    AVBufferRef* hwDeviceCtx = nullptr;
    if (av_hwdevice_ctx_create(&hwDeviceCtx, AV_HWDEVICE_TYPE_VIDEOTOOLBOX, nullptr, nullptr, 0) <
        0) {
      XR_LOGW("Failed to create VideoToolbox device. Falling back to SW");
    } else {
      XR_LOGI("Using VideoToolbox HW acceleration");
      _avContext->hw_device_ctx = av_buffer_ref(hwDeviceCtx);
      av_buffer_unref(&hwDeviceCtx);
      _hwEnabled = true;
    }
  }
#endif

  if ((_avPkt = av_packet_alloc()) == nullptr) {
    throw INVOKE_CODEC_EXCEPTION_MESSAGE("av_packet_alloc failed");
  }
}

VideoDecode::~VideoDecode() {
  if (_avPkt != nullptr) {
    av_packet_unref(_avPkt);
    av_packet_free(&_avPkt);
  }
  avcodec_free_context(&_avContext);
}

void VideoDecode::open() {
  int ret = avcodec_open2(_avContext, _avCodec, nullptr);
  if (ret < 0)
    throw INVOKE_CODEC_EXCEPTION_CODE(ret);
}

void VideoDecode::decode(uint8_t* buffer, size_t size, Picture& pix) {
  _avPkt->data = buffer;
  _avPkt->size = (int)size;
  int ret = avcodec_send_packet(_avContext, _avPkt);
  if (ret < 0) {
    avcodec_flush_buffers(_avContext);
    throw INVOKE_CODEC_EXCEPTION_CODE(ret);
  }
  // At this point the input data is in the decoder, and could have been decoded to a frame.
  ret = avcodec_receive_frame(_avContext, pix.avFrame());
  if (ret == AVERROR(EAGAIN)) {
    // Handle special case where there's no B frame but num_reorder_frames is non-zero. Drain the
    // decoder and force out the decoded frame.

    // Send an empty packet signalling end of stream to make the decoder drain the queue.
    ret = avcodec_send_packet(_avContext, nullptr);
    if (ret < 0) {
      // An error happened, reset the decoder before returning to prepare it for future frames.
      avcodec_flush_buffers(_avContext);
      throw INVOKE_CODEC_EXCEPTION_CODE(ret);
    }
    // Try to receive the frame again.
    ret = avcodec_receive_frame(_avContext, pix.avFrame());
    // After the end of stream has been signalled above, the buffers need to be flushed before any
    // other frame is decoded.
    avcodec_flush_buffers(_avContext);
  }
  if (ret < 0) {
    // On any error, flush the buffers before decoding another frame, so that anything decoded in
    // this call is removed from the output queue.
    avcodec_flush_buffers(_avContext);
    throw INVOKE_CODEC_EXCEPTION_CODE(ret);
  }
}

} // namespace xprs
