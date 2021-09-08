// Facebook Technologies, LLC Proprietary and Confidential.

#pragma once

#include <vrs/DataLayoutConventions.h>
#include <vrs/DataPieces.h>
#include <vrs/FileFormat.h>
#include <vrs/StreamPlayer.h>

namespace OVR {
namespace Vision {
namespace DepthSensingCamera {

using ::vrs::AutoDataLayout;
using ::vrs::AutoDataLayoutEnd;
using ::vrs::DataPieceValue;
using ::vrs::DataLayoutConventions::ImageSpecType;
using ::vrs::DataLayoutConventions::kImageBytesPerPixel;
using ::vrs::DataLayoutConventions::kImageHeight;
using ::vrs::DataLayoutConventions::kImageWidth;

class LegacyConfiguration : public AutoDataLayout {
 public:
  enum : uint32_t { kVersion = 1 };

  DataPieceValue<ImageSpecType> width{kImageWidth};
  DataPieceValue<ImageSpecType> height{kImageHeight};
  DataPieceValue<uint8_t> bytesPerPixels{kImageBytesPerPixel};

  AutoDataLayoutEnd endLayout;
};

class LegacyData : public AutoDataLayout {
 public:
  enum : uint32_t { kVersion = 1 };

  DataPieceValue<uint32_t> frameNumber{"frame_number"};
  DataPieceValue<uint64_t> time_stamp{"time_stamp"};
  DataPieceValue<float> exposure_time{"exposure_time"};
  DataPieceValue<double> gain{"gain"};
  DataPieceValue<double> black_level{"black_level"};
  DataPieceValue<double> arrivalTime{"arrival_time"};

  AutoDataLayoutEnd endLayout;
};

} // namespace DepthSensingCamera
} // namespace Vision
} // namespace OVR
