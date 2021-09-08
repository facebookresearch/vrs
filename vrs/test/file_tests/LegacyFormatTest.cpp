// Facebook Technologies,  LLC Proprietary and Confidential.

#include <gtest/gtest.h> // IWYU pragma: keep

#include <TestDataDir/TestDataDir.h>
#include <vrs/DataLayoutConventions.h>
#include <vrs/LegacyFormatsProvider.h>
#include <vrs/RecordFileReader.h>

using namespace std;
using namespace vrs;

namespace {
struct LegacyFormatTester : testing::Test {
  std::string kTestFile = string(coretech::getTestDataDir()) + "/VRS_Files/ar_camera.vrs";
};

void confirmFormatsInFile(RecordFormatMap& formats) {
  ASSERT_TRUE(formats.find({Record::Type::CONFIGURATION, 1}) != formats.end());
  ASSERT_TRUE(formats.find({Record::Type::DATA, 1}) != formats.end());
  ASSERT_STREQ(
      formats.find({Record::Type::CONFIGURATION, 1})->second.asString().c_str(),
      "data_layout/size=29+data_layout");
  ASSERT_STREQ(
      formats.find({Record::Type::DATA, 1})->second.asString().c_str(),
      "data_layout/size=33+image/raw");
}

class LegacyMontereyCamera : public AutoDataLayout {
 public:
  enum : uint32_t { kDataVersion = 5 };
  // v2
  DataPieceValue<double> captureTimestamp{"capture_timestamp"};
  DataPieceValue<double> arrivalTimestamp{"arrival_timestamp"};
  DataPieceValue<uint64_t> frameCounter{"frame_counter"};
  DataPieceValue<uint32_t> cameraUniqueId{"camera_unique_id"};
  // v3
  DataPieceValue<int32_t> streamId{"stream_id", 0};
  DataPieceValue<uint32_t> gainHAL{"gain_hal", 0};
  // v4
  DataPieceValue<double> exposureDuration{"exposure_duration", 0};
  // v5
  DataPieceValue<float> gain{"gain", 0}; // complex default value: force calling a method

  AutoDataLayoutEnd endLayout;
};

} // namespace

TEST_F(LegacyFormatTester, LegacyFormatTest) {
  vrs::RecordFileReader file;
  ASSERT_EQ(file.openFile(kTestFile), 0);
  StreamId id =
      file.getStreamForTag("type", "service_pixel_buffer", RecordableTypeId::FacebookARCamera);
  ASSERT_TRUE(id.isValid());
  RecordFormatMap formats;

  // Get & verify the datalayouts from the file
  EXPECT_EQ(file.getRecordFormats(id, formats), 2);
  ASSERT_TRUE(formats.find({Record::Type::DATA, 2}) == formats.end()); // not present in the file
  confirmFormatsInFile(formats);

  // add legacy datalayout definitions for types defined in the file
  RecordFormatRegistrar& reg = RecordFormatRegistrar::getInstance();
  DataLayoutConventions::ImageSpec imageSpec;
  reg.addLegacyRecordFormat(
      RecordableTypeId::FacebookARCamera,
      Record::Type::CONFIGURATION,
      1,
      imageSpec.getContentBlock(),
      {&imageSpec});
  reg.addLegacyRecordFormat(
      RecordableTypeId::FacebookARCamera,
      Record::Type::DATA,
      1,
      imageSpec.getContentBlock(),
      {&imageSpec});
  // add legacy definition not present in the file
  reg.addLegacyRecordFormat(
      RecordableTypeId::FacebookARCamera,
      Record::Type::DATA,
      2, // different record format version
      imageSpec.getContentBlock(),
      {&imageSpec});

  EXPECT_EQ(file.getRecordFormats(id, formats), 3); // 2 "old" + 1 "new"
  confirmFormatsInFile(formats); // verify that the colisions don't matter

  // verify that the added legacy definition is found
  ASSERT_TRUE(formats.find({Record::Type::DATA, 2}) != formats.end());
  ASSERT_STREQ(formats.find({Record::Type::DATA, 2})->second.asString().c_str(), "data_layout");
}

TEST_F(LegacyFormatTester, UnitMinMaxTest) {
  RecordFormatRegistrar& reg = RecordFormatRegistrar::getInstance();
  unique_ptr<DataLayout> dl =
      reg.getLatestDataLayout(RecordableTypeId::Proto0CameraHALSlam, Record::Type::DATA);
  EXPECT_EQ(dl, nullptr);

  LegacyMontereyCamera legacyMontereyCamera;
  ContentBlock rawImage(ImageFormat::RAW);
  reg.addLegacyRecordFormat(
      RecordableTypeId::Proto0CameraHALSlam,
      Record::Type::DATA,
      LegacyMontereyCamera::kDataVersion,
      ContentBlock(ContentType::DATA_LAYOUT) + rawImage,
      {&legacyMontereyCamera});
  dl = reg.getLatestDataLayout(RecordableTypeId::Proto0CameraHALSlam, Record::Type::CONFIGURATION);
  EXPECT_EQ(dl, nullptr);
  dl = reg.getLatestDataLayout(RecordableTypeId::Proto0IMUDML, Record::Type::CONFIGURATION);
  EXPECT_EQ(dl, nullptr);
  dl = reg.getLatestDataLayout(RecordableTypeId::Proto0CameraHALSlam, Record::Type::DATA);
  ASSERT_NE(dl, nullptr);
  DataPieceValue<float>* gain = dl->findDataPieceValue<float>("gain");
  // we haven't declared a unit yet, so we should find the field, without any unit or min-max
  ASSERT_NE(gain, nullptr);
  string unit, description;
  EXPECT_FALSE(gain->getUnit(unit));
  EXPECT_FALSE(gain->getDescription(description));
  float min, max;
  EXPECT_FALSE(gain->getMin(min));
  EXPECT_FALSE(gain->getMax(max));

  legacyMontereyCamera.gain.setUnit("m/s");
  legacyMontereyCamera.gain.setRange(0, 10);
  legacyMontereyCamera.gain.setDescription("some gain");
  reg.addLegacyRecordFormat(
      RecordableTypeId::Proto0CameraHALSlam,
      Record::Type::DATA,
      LegacyMontereyCamera::kDataVersion + 1,
      ContentBlock(ContentType::DATA_LAYOUT) + rawImage,
      {&legacyMontereyCamera});
  dl = reg.getLatestDataLayout(RecordableTypeId::Proto0CameraHALSlam, Record::Type::DATA);
  ASSERT_NE(dl, nullptr);
  gain = dl->findDataPieceValue<float>("gain");
  ASSERT_NE(gain, nullptr);
  EXPECT_TRUE(gain->getUnit(unit));
  EXPECT_STREQ(unit.c_str(), "m/s");
  EXPECT_TRUE(gain->getDescription(description));
  EXPECT_STREQ(description.c_str(), "some gain");
  EXPECT_TRUE(gain->getMin(min));
  EXPECT_EQ(min, 0);
  EXPECT_TRUE(gain->getMax(max));
  EXPECT_EQ(max, 10);

  LegacyMontereyCamera virginLegacyMontereyCamera;
  reg.addLegacyRecordFormat(
      RecordableTypeId::Proto0CameraHALSlam,
      Record::Type::DATA,
      LegacyMontereyCamera::kDataVersion + 2,
      ContentBlock(ContentType::DATA_LAYOUT) + rawImage,
      {&virginLegacyMontereyCamera});
  // we should find the latest definition, which doesn't include the unit & min/max definitions
  dl = reg.getLatestDataLayout(RecordableTypeId::Proto0CameraHALSlam, Record::Type::DATA);
  ASSERT_NE(dl, nullptr);
  gain = dl->findDataPieceValue<float>("gain");
  ASSERT_NE(gain, nullptr);
  EXPECT_FALSE(gain->getUnit(unit));
  EXPECT_FALSE(gain->getMin(min));
  EXPECT_FALSE(gain->getMax(max));
}
