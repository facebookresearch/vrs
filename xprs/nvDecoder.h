// (c) Meta Platforms, Inc. and affiliates. Confidential and proprietary.

#pragma once

#include <vector>

#include <ffnvcodec/dynlink_cuda.h>
#include <ffnvcodec/dynlink_loader.h> // CudaFunctions
#include <ffnvcodec/dynlink_nvcuvid.h>

#include "cudaContextProvider.h"

#include "InternalDecoder.h"

struct AVFrame;

namespace xprs {

class Picture;

cudaVideoCodec codecNameToCudaVideoCodecEnum(const std::string& codec_name);

/*
 * Returns true if a GPU has no hardware decoder provided in the decoderName.
 */
bool deviceHasNoHwDecoder(std::string& decoderName, const char* deviceName);

class NvDecoder : public InternalDecoder {
 public:
  NvDecoder(const NvCodecContext& nvcodec_context, cudaVideoCodec cuda_video_codec);
  ~NvDecoder() override;
  NvDecoder(const NvDecoder&) = delete;
  NvDecoder& operator=(const NvDecoder&) = delete;
  NvDecoder(NvDecoder&&) = delete;
  NvDecoder& operator=(NvDecoder&&) = delete;

  void open() override;
  // Copies decoded image from GPU/CUDA memory to host/cpu memory and sets Picture fields.
  void decode(uint8_t* encoded_image_ptr, size_t encoded_image_size, Picture& decoded_picture)
      override;

 private:
  const NvCodecContext _nvcodec_context;

  CUvideoparser _parser = nullptr;
  CUvideodecoder _decoder = nullptr;
  CUstream _cuvidStream = nullptr;

  unsigned int _imageWidthInPixels = 0;
  unsigned int _lumaHeight = 0;
  unsigned int _chromaHeight = 0;
  unsigned int _numChromaPlanes = 0;
  int _bytesPerPixel = 1;
  size_t _deviceFramePitch = 0;
  int _surfaceHeight = 0;
  bool _isKeyFrame = false;
  cudaVideoSurfaceFormat _output_format = cudaVideoSurfaceFormat_NV12;
  std::vector<uint8_t> _decoded_image_yuv;
  AVFrame* _decoded_frame_ptr;

  // Callback function to be registered for getting a callback when decoding of sequence starts
  static int CUDAAPI HandleVideoSequenceCallback(void* pUserData, CUVIDEOFORMAT* pVideoFormat) {
    return ((NvDecoder*)pUserData)->HandleVideoSequence(pVideoFormat);
  }

  // Callback function to be registered for getting a callback when a decoded frame is ready to be
  // decoded
  static int CUDAAPI HandlePictureDecodeCallback(void* pUserData, CUVIDPICPARAMS* pPicParams) {
    return ((NvDecoder*)pUserData)->HandlePictureDecode(pPicParams);
  }

  // Callback function to be registered for getting a callback when a decoded frame is available for
  // display
  static int CUDAAPI HandlePictureDisplayCallback(void* pUserData, CUVIDPARSERDISPINFO* pDispInfo) {
    return ((NvDecoder*)pUserData)->HandlePictureDisplay(pDispInfo);
  }

  // This function gets called when a sequence is ready to be decoded. The function also gets called
  // when there is format change. It inits video decoder
  int HandleVideoSequence(CUVIDEOFORMAT* pVideoFormat);

  // This function gets called when a picture is ready to be decoded. cuvidDecodePicture is called
  // from this function to decode the picture
  int HandlePictureDecode(CUVIDPICPARAMS* pPicParams);

  // This function gets called after a picture is decoded and available for display. Frames are
  // fetched and stored in internal buffer
  int HandlePictureDisplay(CUVIDPARSERDISPINFO* pDispInfo);
};

} // namespace xprs
