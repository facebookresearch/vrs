// (c) Meta Platforms, Inc. and affiliates. Confidential and proprietary.

#include "nvDecoder.h"
#include "Codecs.h"
#include "FFmpegUtils.h"
#include "cudaContextProvider.h"
#include "cudaUtils.h"

#include <logging/Log.h>
#include <cmath>
#include <stdexcept>
#include <string>

#define DEFAULT_LOG_CHANNEL "XPRS"

namespace xprs {

// Doing a device capability check via NVENC API is more generic and consistent way
// but it turned out to be quite slow, > 600 ms
// This method identifies A100 GPU which does not support HW AV1 decoder but is widely used in our
// servers fleet. Filtering devices by device name is significantly faster.
bool deviceHasNoHwDecoder(std::string& decoderName, const char* deviceName) {
  if (decoderName != kNvAv1DecoderName) {
    return false;
  }
  return deviceName == nullptr || strstr(deviceName, "PG509") != nullptr ||
      strstr(deviceName, "A100") != nullptr;
}

// Currently we only support NV12 output format.
// To support other formats extend getChromaHeightFactor and getChromaPlaneCount functions below
static float getChromaHeightFactor(cudaVideoSurfaceFormat surface_format) {
  float factor = 0.5;
  switch (surface_format) {
    case cudaVideoSurfaceFormat_NV12:
    case cudaVideoSurfaceFormat_P016:
      factor = 0.5;
      break;
    case cudaVideoSurfaceFormat_YUV444:
    case cudaVideoSurfaceFormat_YUV444_16Bit:
      factor = 1.0;
      break;
  }
  return factor;
}

static int getChromaPlaneCount(cudaVideoSurfaceFormat surface_format) {
  int numPlane = 1;
  switch (surface_format) {
    case cudaVideoSurfaceFormat_NV12:
    case cudaVideoSurfaceFormat_P016:
      numPlane = 1;
      break;
    case cudaVideoSurfaceFormat_YUV444:
    case cudaVideoSurfaceFormat_YUV444_16Bit:
      numPlane = 2;
      break;
  }
  return numPlane;
}

cudaVideoCodec codecNameToCudaVideoCodecEnum(const std::string& codec_name) {
  if (codec_name == kNvH264DecoderName) {
    return cudaVideoCodec_H264;
  } else if (codec_name == kNvH265DecoderName) {
    return cudaVideoCodec_HEVC;
  } else if (codec_name == kNvAv1DecoderName) {
    return cudaVideoCodec_AV1;
  } else {
    std::string error_message = "Unsupported cudaVideoCodec requested: " + codec_name;
    XR_LOGE("{}", error_message.c_str());
    throw std::runtime_error(error_message);
  }
}

NvDecoder::NvDecoder(const NvCodecContext& nvcodec_context, cudaVideoCodec cuda_video_codec)
    : _nvcodec_context(nvcodec_context) {
  const CUDAContextScope scope(_nvcodec_context);
  CUVIDPARSERPARAMS videoParserParameters = {};
  videoParserParameters.CodecType = cuda_video_codec;
  videoParserParameters.ulMaxNumDecodeSurfaces = 1;
  videoParserParameters.ulClockRate = 1000;
  constexpr int low_latency_display_delay = 0;
  videoParserParameters.ulMaxDisplayDelay = low_latency_display_delay;
  videoParserParameters.pUserData = this;
  videoParserParameters.pfnSequenceCallback = HandleVideoSequenceCallback;
  videoParserParameters.pfnDecodePicture = HandlePictureDecodeCallback;
  videoParserParameters.pfnDisplayPicture = HandlePictureDisplayCallback;
  CUDA_API_CALL(
      _nvcodec_context._cuvid_functions->cuvidCreateVideoParser(&_parser, &videoParserParameters),
      _nvcodec_context._cuda_functions,
      THROW_IF_ERROR);
}

NvDecoder::~NvDecoder() {
  if (_parser) {
    CUDA_API_CALL(
        _nvcodec_context._cuvid_functions->cuvidDestroyVideoParser(_parser),
        _nvcodec_context._cuda_functions,
        DO_NOT_THROW);
  }
  if (_decoder) {
    CUDA_API_CALL(
        _nvcodec_context._cuvid_functions->cuvidDestroyDecoder(_decoder),
        _nvcodec_context._cuda_functions,
        DO_NOT_THROW);
  }
}

void NvDecoder::open() {}

// Return values:
// 0 :  fail
// 1 :  succeeded, but driver should not override CUVIDPARSERPARAMS::ulMaxNumDecodeSurfaces
// >1:  succeeded, and driver should override CUVIDPARSERPARAMS::ulMaxNumDecodeSurfaces with this
// return value
int NvDecoder::HandleVideoSequence(CUVIDEOFORMAT* vdeo_format) {
  CUVIDDECODECREATEINFO video_decode_create_info = {0};
  video_decode_create_info.CodecType = vdeo_format->codec;
  video_decode_create_info.ChromaFormat = vdeo_format->chroma_format;
  video_decode_create_info.OutputFormat = cudaVideoSurfaceFormat_NV12;
  video_decode_create_info.bitDepthMinus8 = vdeo_format->bit_depth_luma_minus8;
  video_decode_create_info.DeinterlaceMode = cudaVideoDeinterlaceMode_Weave;
  video_decode_create_info.ulNumOutputSurfaces = 2; // 2; mine 2, ffmpeg = 1 seems does not matter
  video_decode_create_info.ulCreationFlags = cudaVideoCreate_PreferCUVID;
  // This is how nvidia recommends calculating ulNumDecodeSurfaces here:
  // https://developer.nvidia.com/blog/optimizing-video-memory-usage-with-the-nvdecode-api-and-nvidia-video-codec-sdk/
  // video_decode_create_info.ulNumDecodeSurfaces = video_format->min_num_decode_surfaces + 3;
  video_decode_create_info.ulNumDecodeSurfaces = 25;
  video_decode_create_info.ulWidth = vdeo_format->coded_width;
  video_decode_create_info.ulHeight = vdeo_format->coded_height;
  video_decode_create_info.ulMaxWidth = video_decode_create_info.ulWidth;
  video_decode_create_info.ulMaxHeight = video_decode_create_info.ulHeight;
  video_decode_create_info.ulTargetWidth = video_decode_create_info.ulWidth;
  video_decode_create_info.ulTargetHeight = video_decode_create_info.ulHeight;

  _imageWidthInPixels = vdeo_format->display_area.right - vdeo_format->display_area.left;
  // NV12/P016 output format width is 2 byte aligned because of U and V interleave
  if (_output_format == cudaVideoSurfaceFormat_NV12 ||
      _output_format == cudaVideoSurfaceFormat_P016) {
    _imageWidthInPixels = (_imageWidthInPixels + 1) & ~1;
  }
  _lumaHeight = vdeo_format->display_area.bottom - vdeo_format->display_area.top;
  _bytesPerPixel = video_decode_create_info.bitDepthMinus8 > 0 ? 2 : 1;
  _chromaHeight =
      (int)(std::ceil(_lumaHeight * getChromaHeightFactor(video_decode_create_info.OutputFormat)));

  _numChromaPlanes = getChromaPlaneCount(video_decode_create_info.OutputFormat);
  _surfaceHeight = video_decode_create_info.ulTargetHeight;
  const int size_ofdecoded_image_yuv_format_in_bytes =
      _imageWidthInPixels * (_lumaHeight + (_chromaHeight * _numChromaPlanes)) * _bytesPerPixel;

  _decoded_image_yuv.resize(size_ofdecoded_image_yuv_format_in_bytes);

  CUDAContextScope cuda_context_scope(_nvcodec_context);
  CUDA_API_CALL(
      _nvcodec_context._cuvid_functions->cuvidCreateDecoder(&_decoder, &video_decode_create_info),
      _nvcodec_context._cuda_functions,
      THROW_IF_ERROR);
  return video_decode_create_info.ulNumDecodeSurfaces;
}

void NvDecoder::decode(
    uint8_t* encoded_image_ptr,
    size_t encoded_image_size,
    Picture& decoded_picture) {
  _decoded_frame_ptr = decoded_picture.avFrame();
  CUVIDSOURCEDATAPACKET packet = {0};
  packet.payload = encoded_image_ptr;
  packet.payload_size = encoded_image_size;
  packet.flags = CUVID_PKT_ENDOFPICTURE | CUVID_PKT_TIMESTAMP;
  packet.timestamp = 0;
  if (encoded_image_size == 0) {
    packet.flags |= CUVID_PKT_ENDOFSTREAM;
  }
  CUDA_API_CALL(
      _nvcodec_context._cuvid_functions->cuvidParseVideoData(_parser, &packet),
      _nvcodec_context._cuda_functions,
      THROW_IF_ERROR);
}

// 0: fail, >=1: succeeded
int NvDecoder::HandlePictureDecode(CUVIDPICPARAMS* pic_params) {
  CUDAContextScope cuda_context_scope(_nvcodec_context);
  _isKeyFrame = pic_params->intra_pic_flag;
  CUDA_API_CALL(
      _nvcodec_context._cuvid_functions->cuvidDecodePicture(_decoder, pic_params),
      _nvcodec_context._cuda_functions,
      THROW_IF_ERROR);
  return 1;
}

//  0: fail; >=1: succeeded
int NvDecoder::HandlePictureDisplay(CUVIDPARSERDISPINFO* parser_disp_info) {
  CUVIDPROCPARAMS video_processing_parameters = {};
  video_processing_parameters.progressive_frame = parser_disp_info->progressive_frame;
  video_processing_parameters.second_field = parser_disp_info->repeat_first_field + 1;
  video_processing_parameters.top_field_first = parser_disp_info->top_field_first;
  video_processing_parameters.unpaired_field = parser_disp_info->repeat_first_field < 0;
  video_processing_parameters.output_stream = _cuvidStream;

  CUdeviceptr src_frame_device_ptr = 0;
  unsigned int src_pitch = 0;
  CUDAContextScope cuda_context_scope(_nvcodec_context);

  // Create scope guard to ensure frame is unmapped on any exit path
  auto unmapGuard = [&](char* dummyPtr) -> void {
    if (src_frame_device_ptr) {
      CUDA_API_CALL(
          _nvcodec_context._cuvid_functions->cuvidUnmapVideoFrame(_decoder, src_frame_device_ptr),
          _nvcodec_context._cuda_functions,
          THROW_IF_ERROR);
    }
    delete dummyPtr;
  };
  std::unique_ptr<char, decltype(unmapGuard)> guard(new char(0), unmapGuard);

  CUDA_API_CALL(
      _nvcodec_context._cuvid_functions->cuvidMapVideoFrame(
          _decoder,
          parser_disp_info->picture_index,
          &src_frame_device_ptr,
          &src_pitch,
          &video_processing_parameters),
      _nvcodec_context._cuda_functions,
      THROW_IF_ERROR);

  CUVIDGETDECODESTATUS decode_status;
  memset(&decode_status, 0, sizeof(decode_status));
  CUresult result = _nvcodec_context._cuvid_functions->cuvidGetDecodeStatus(
      _decoder, parser_disp_info->picture_index, &decode_status);
  if (result == CUDA_SUCCESS &&
      (decode_status.decodeStatus == cuvidDecodeStatus_Error ||
       decode_status.decodeStatus == cuvidDecodeStatus_Error_Concealed)) {
    std::string error_message =
        "Image decoding failed with status: " + std::to_string(decode_status.decodeStatus);
    throw std::runtime_error(error_message);
  }
  uint8_t* decoded_image_yuv_ptr = _decoded_image_yuv.data();
  // Copy luma plane
  CUDA_MEMCPY2D mem_cpy_2d = {0};
  mem_cpy_2d.srcMemoryType = CU_MEMORYTYPE_DEVICE;
  mem_cpy_2d.srcDevice = src_frame_device_ptr;
  mem_cpy_2d.srcPitch = src_pitch;
  mem_cpy_2d.dstMemoryType = CU_MEMORYTYPE_HOST;
  mem_cpy_2d.dstDevice = (CUdeviceptr)(mem_cpy_2d.dstHost = decoded_image_yuv_ptr);
  mem_cpy_2d.dstPitch =
      _deviceFramePitch ? _deviceFramePitch : _imageWidthInPixels * _bytesPerPixel;
  mem_cpy_2d.WidthInBytes = _imageWidthInPixels * _bytesPerPixel;
  mem_cpy_2d.Height = _lumaHeight;
  CUDA_API_CALL(
      _nvcodec_context._cuda_functions->cuMemcpy2DAsync(&mem_cpy_2d, _cuvidStream),
      _nvcodec_context._cuda_functions,
      THROW_IF_ERROR);

  // Copy chroma plane
  // NVDEC output has luma height aligned by 2. Adjust chroma offset by aligning height
  mem_cpy_2d.srcDevice = (CUdeviceptr)((uint8_t*)src_frame_device_ptr +
                                       mem_cpy_2d.srcPitch * ((_surfaceHeight + 1) & ~1));
  mem_cpy_2d.dstDevice =
      (CUdeviceptr)(mem_cpy_2d.dstHost = decoded_image_yuv_ptr + mem_cpy_2d.dstPitch * _lumaHeight);
  mem_cpy_2d.Height = _chromaHeight;
  CUDA_API_CALL(
      _nvcodec_context._cuda_functions->cuMemcpy2DAsync(&mem_cpy_2d, _cuvidStream),
      _nvcodec_context._cuda_functions,
      THROW_IF_ERROR);

  if (_numChromaPlanes == 2) {
    mem_cpy_2d.srcDevice = (CUdeviceptr)((uint8_t*)src_frame_device_ptr +
                                         mem_cpy_2d.srcPitch * ((_surfaceHeight + 1) & ~1) * 2);
    mem_cpy_2d.dstDevice = (CUdeviceptr)(mem_cpy_2d.dstHost = decoded_image_yuv_ptr +
                                             mem_cpy_2d.dstPitch * _lumaHeight * 2);
    mem_cpy_2d.Height = _chromaHeight;
    CUDA_API_CALL(
        _nvcodec_context._cuda_functions->cuMemcpy2DAsync(&mem_cpy_2d, _cuvidStream),
        _nvcodec_context._cuda_functions,
        THROW_IF_ERROR);
  }
  CUDA_API_CALL(
      _nvcodec_context._cuda_functions->cuStreamSynchronize(_cuvidStream),
      _nvcodec_context._cuda_functions,
      THROW_IF_ERROR);

  _decoded_frame_ptr->format = AV_PIX_FMT_NV12;
  _decoded_frame_ptr->width = _imageWidthInPixels;
  _decoded_frame_ptr->height = _lumaHeight;
  _decoded_frame_ptr->data[0] = _decoded_image_yuv.data();
  _decoded_frame_ptr->linesize[0] = _imageWidthInPixels * _bytesPerPixel;
  _decoded_frame_ptr->data[1] =
      _decoded_image_yuv.data() + (_decoded_frame_ptr->width * _decoded_frame_ptr->height);
  _decoded_frame_ptr->linesize[1] = _decoded_frame_ptr->linesize[0];
  _decoded_frame_ptr->key_frame = _isKeyFrame;
  if (_isKeyFrame) {
    _decoded_frame_ptr->pict_type = AV_PICTURE_TYPE_I;
  } else {
    _decoded_frame_ptr->pict_type = AV_PICTURE_TYPE_P;
  }
  return 1;
}

} // namespace xprs
