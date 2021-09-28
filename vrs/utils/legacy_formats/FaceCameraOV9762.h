// Copyright (c) Facebook, Inc. and its affiliates. Confidential and proprietary.

#pragma once

#include <vrs/DataLayoutConventions.h>
#include <vrs/DataReference.h>
#include <vrs/FileFormat.h>
#include <vrs/StreamPlayer.h>

namespace OVR {
namespace Vision {

namespace FaceCameraOV9762 {

using ::vrs::AutoDataLayout;
using ::vrs::AutoDataLayoutEnd;
using ::vrs::CurrentRecord;
using ::vrs::DataPieceString;
using ::vrs::DataPieceValue;
using ::vrs::DataPieceVector;
using ::vrs::DataReference;
using ::vrs::Point2Dd;
using ::vrs::DataLayoutConventions::ImageSpecType;
using ::vrs::FileFormat::LittleEndian;

#pragma pack(push, 1)

enum : uint32_t { kStateVersion = 1 }; // No state data

struct VRSConfiguration {
  enum : uint32_t { kVersion = 2 };

  VRSConfiguration() {}

  LittleEndian<ImageSpecType> width;
  LittleEndian<ImageSpecType> height;
  LittleEndian<ImageSpecType> bytesPerPixel;
  LittleEndian<ImageSpecType> format;
};

struct VRSData {
  enum : uint32_t { kVersion = 1 };

  VRSData() {}

  bool canHandle(
      const CurrentRecord& record,
      void* imageData,
      uint32_t imageSize,
      DataReference& outDataReference) {
    uint32_t formatVersion = record.formatVersion;
    uint32_t payloadSize = record.recordSize;
    if ((formatVersion == kVersion) && (sizeof(VRSData) + imageSize == payloadSize)) {
      outDataReference.useRawData(this, payloadSize - imageSize, imageData, imageSize);
      return true;
    }
    return false;
  }

  void upgradeFrom(uint32_t formatVersion) {
    // We do nothing here, since only one version
  }

  LittleEndian<double> exposureTime;
  LittleEndian<double> arrivalTime;
  LittleEndian<uint64_t> frameCounter;
  LittleEndian<uint32_t> cameraUniqueId;
};

#pragma pack(pop)

// The types & names of some of these fields are using the new DataLayout conventions
// for ImageContentBlocks. See VRS/DataLayoutConventions.h
class DataLayoutConfigurationLegacy : public AutoDataLayout {
 public:
  enum : uint32_t { kVersion = FaceCameraOV9762::VRSConfiguration::kVersion };

  DataPieceValue<ImageSpecType> width{::vrs::DataLayoutConventions::kImageWidth};
  DataPieceValue<ImageSpecType> height{::vrs::DataLayoutConventions::kImageHeight};
  DataPieceValue<ImageSpecType> bytesPerPixels{::vrs::DataLayoutConventions::kImageBytesPerPixel};
  DataPieceValue<ImageSpecType> format{::vrs::DataLayoutConventions::kImagePixelFormat};

  AutoDataLayoutEnd endLayout;
};

class DataLayoutDataLegacy : public AutoDataLayout {
 public:
  enum : uint32_t { kVersion = FaceCameraOV9762::VRSData::kVersion };

  DataPieceValue<double> exposureTime{"exposure_time"};
  DataPieceValue<double> arrivalTime{"arrival_time"};
  DataPieceValue<uint64_t> frameCounter{"frame_counter"};
  DataPieceValue<uint32_t> cameraUniqueId{"camera_unique_id"};

  AutoDataLayoutEnd endLayout;
};

class DataLayoutConfiguration : public AutoDataLayout {
 public:
  enum : uint32_t { kVersion = 3 };

  DataLayoutConfiguration() {
    labels_ = {
        &label1,
        &label2,
        &label3,
        &label4,
        &label5,
        &label6,
        &label7,
        &label8,
        &label9,
        &label10,
        &label11,
        &label12};
  }

  DataPieceValue<ImageSpecType> width{::vrs::DataLayoutConventions::kImageWidth};
  DataPieceValue<ImageSpecType> height{::vrs::DataLayoutConventions::kImageHeight};
  DataPieceValue<ImageSpecType> bytesPerPixel{::vrs::DataLayoutConventions::kImageBytesPerPixel};
  DataPieceValue<ImageSpecType> format{::vrs::DataLayoutConventions::kImagePixelFormat};

  DataPieceString streamName{"stream_name"}; // either "left-right" or "cyclo-mouth"
  DataPieceValue<uint16_t> leftImageLabelCount{"left_image_label_count"}; // left/cyclo
  DataPieceValue<uint16_t> rightImageLabelCount{"right_image_label_count"}; // right/mouth
  DataPieceString label1{"label_1"};
  DataPieceString label2{"label_2"};
  DataPieceString label3{"label_3"};
  DataPieceString label4{"label_4"};
  DataPieceString label5{"label_5"};
  DataPieceString label6{"label_6"};
  DataPieceString label7{"label_7"};
  DataPieceString label8{"label_8"};
  DataPieceString label9{"label_9"};
  DataPieceString label10{"label_10"};
  DataPieceString label11{"label_11"};
  DataPieceString label12{"label_12"};

  DataPieceString& getLabel(size_t index) {
    return *labels_[index];
  }
  size_t getMaxLabelCount() {
    return labels_.size();
  }

  DataPieceString& getLeftImageLabel(size_t index) {
    return getLabel(index);
  }
  DataPieceString& getRightImageLabel(size_t index) {
    return getLabel(leftImageLabelCount.get() + index);
  }

  AutoDataLayoutEnd endLayout;

 private:
  std::vector<DataPieceString*> labels_;
};

class DataLayoutData : public AutoDataLayout {
 public:
  enum : uint32_t { kVersion = 2 };

  DataLayoutData() {
    labelPoints_ = {
        &labelPoints1,
        &labelPoints2,
        &labelPoints3,
        &labelPoints4,
        &labelPoints5,
        &labelPoints6,
        &labelPoints7,
        &labelPoints8,
        &labelPoints9,
        &labelPoints10,
        &labelPoints11,
        &labelPoints12};
  }

  DataPieceValue<double> exposureTime{"exposure_time"};
  DataPieceValue<double> arrivalTime{"arrival_time"};
  DataPieceValue<uint64_t> frameCounter{"frame_counter"};
  DataPieceValue<uint32_t> cameraUniqueId{"camera_unique_id"};

  DataPieceVector<Point2Dd> labelPoints1{"label_points_1"};
  DataPieceVector<Point2Dd> labelPoints2{"label_points_2"};
  DataPieceVector<Point2Dd> labelPoints3{"label_points_3"};
  DataPieceVector<Point2Dd> labelPoints4{"label_points_4"};
  DataPieceVector<Point2Dd> labelPoints5{"label_points_5"};
  DataPieceVector<Point2Dd> labelPoints6{"label_points_6"};
  DataPieceVector<Point2Dd> labelPoints7{"label_points_7"};
  DataPieceVector<Point2Dd> labelPoints8{"label_points_8"};
  DataPieceVector<Point2Dd> labelPoints9{"label_points_9"};
  DataPieceVector<Point2Dd> labelPoints10{"label_points_10"};
  DataPieceVector<Point2Dd> labelPoints11{"label_points_11"};
  DataPieceVector<Point2Dd> labelPoints12{"label_points_12"};

  DataPieceVector<Point2Dd>& getPoints(size_t index) {
    return *labelPoints_[index];
  }

  AutoDataLayoutEnd endLayout;

 private:
  std::vector<DataPieceVector<Point2Dd>*> labelPoints_;
};

} // namespace FaceCameraOV9762
} // namespace Vision
} // namespace OVR
