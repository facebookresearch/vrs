// Facebook Technologies, LLC Proprietary and Confidential.

#include "DataLayoutConventions.h"

#include "RecordFormat.h"

namespace vrs {
namespace DataLayoutConventions {

ContentBlock ImageSpec::getImageContentBlock(const ImageContentBlockSpec& base, size_t blockSize) {
  PixelFormat readPixelFormat{PixelFormat::UNDEFINED};
  uint32_t readWidth{};
  uint32_t readHeight{};
  uint32_t readBytesPerPixel{};
  uint8_t readBytesPerPixel8{};
  if (width.get(readWidth) && readWidth > 0 && height.get(readHeight) && readHeight > 0) {
    if (pixelFormat.get(readPixelFormat) && readPixelFormat != PixelFormat::UNDEFINED &&
        readPixelFormat < PixelFormat::COUNT) {
      // that's all we need
    } else {
      readPixelFormat = PixelFormat::UNDEFINED;
      if ((bytesPerPixels.get(readBytesPerPixel) && readBytesPerPixel > 0) ||
          (bytesPerPixels8.get(readBytesPerPixel8) &&
           (readBytesPerPixel = readBytesPerPixel8) > 0)) {
        // Legacy spec, without pixel format, so we have to make assumptions here
        switch (readBytesPerPixel) {
          case 1:
            readPixelFormat = PixelFormat::GREY8;
            break;
          case 3:
            readPixelFormat = PixelFormat::RGB8;
            break;
          case 4:
            readPixelFormat = PixelFormat::DEPTH32F;
            break;
          case 8:
            readPixelFormat = PixelFormat::SCALAR64F;
            break;
          default:
            break;
        }
      }
    }
  }
  if (readWidth != 0 && readHeight != 0 && readPixelFormat != PixelFormat::UNDEFINED) {
    uint32_t readStride = stride.get(); // get value or 0
    if (base.getImageFormat() == ImageFormat::RAW) {
      return ContentBlock(readPixelFormat, readWidth, readHeight, readStride);
    } else if (base.getImageFormat() == ImageFormat::VIDEO) {
      if (blockSize != ContentBlock::kSizeUnknown) {
        string aCodecName;
        if (!codecName.get(aCodecName) || aCodecName.empty()) {
          aCodecName = base.getCodecName();
        }
        uint32_t aCodecQuality;
        if (!codecQuality.get(aCodecQuality) ||
            !ImageContentBlockSpec::isQualityValid(aCodecQuality)) {
          aCodecQuality = base.getCodecQuality();
        }
        return {
            ImageContentBlockSpec(
                aCodecName, aCodecQuality, readPixelFormat, readWidth, readHeight, readStride),
            blockSize};
      }
    }
  }
  return {};
}

} // namespace DataLayoutConventions
} // namespace vrs
