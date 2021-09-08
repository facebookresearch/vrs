// Facebook Technologies, LLC Proprietary and Confidential.

#include "ChessCodec.h"

#define DEFAULT_LOG_CHANNEL "ChessBoardDecoder"
#include <logging/Checks.h>
#include <logging/Log.h>

#include <vrs/FileMacros.h>

using namespace vrs;
using namespace vrs::utils::test;

class ChessBoardDecoder : public vrs::utils::DecoderI {
 public:
  int decode(
      RecordReader* reader,
      const uint32_t sizeBytes,
      void* outBuffer,
      const ImageContentBlockSpec& inputImageSpec) override {
    frame_.resize(inputImageSpec.getRawImageSize(), 0);
    width_ = inputImageSpec.getWidth();
    height_ = inputImageSpec.getHeight();
    IF_ERROR_RETURN(decode(reader, sizeBytes));
    memcpy(outBuffer, frame_.data(), frame_.size());
    return SUCCESS;
  }
  int decode(RecordReader* reader, const uint32_t sizeBytes) override {
    vector<uint8_t> buffer(sizeBytes);
    IF_ERROR_RETURN(reader->read(buffer));
    if (frame_.empty()) {
      return INVALID_REQUEST;
    }
    if (sizeBytes == sizeof(IFrameData)) {
      // this is an i-frame: set the whole image to a color
      IFrameData* iFrame = reinterpret_cast<IFrameData*>(buffer.data());
      uint8_t value = iFrame->value;
      memset(frame_.data(), value, frame_.size());
    } else if (sizeBytes == sizeof(PFrameData)) {
      // this is a p-frame: set a square to a color, leave the rest unmodified
      PFrameData* pFrame = reinterpret_cast<PFrameData*>(buffer.data());
      uint8_t* data = frame_.data();
      uint32_t squareWidth = width_ / pFrame->xMax;
      uint32_t squareHeight = height_ / pFrame->yMax;
      for (uint32_t x = squareWidth * pFrame->x; x < squareWidth * (pFrame->x + 1); ++x) {
        for (uint32_t y = squareHeight * pFrame->y; y < squareHeight * (pFrame->y + 1); ++y) {
          uint8_t& v = data[x + y * width_];
          v += pFrame->incrementValue;
          XR_CHECK(v == pFrame->expectedValue);
        }
      }
    } else {
      return ErrorCode::INVALID_DISK_DATA;
    }
    return SUCCESS;
  }

 private:
  vector<uint8_t> frame_;
  uint32_t width_ = 0;
  uint32_t height_ = 0;
};

std::unique_ptr<vrs::utils::DecoderI> vrs::utils::test::chessDecoderMaker(const std::string& name) {
  if (name == kChessCodecName) {
    return std::make_unique<ChessBoardDecoder>();
  }
  return nullptr;
}
