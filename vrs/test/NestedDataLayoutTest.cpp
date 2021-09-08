// Facebook Technologies, LLC Proprietary and Confidential.

#include <iostream>
#include <limits>

#include <gtest/gtest.h>

#include <vrs/DataPieces.h>
#include <vrs/DataSource.h>

using namespace std;
using namespace vrs;

namespace {
// These definition come from a real use case, captured here for testing, with some added fake data.
struct RenderPose : public DataLayoutStruct {
  DATA_LAYOUT_STRUCT_WITH_INIT(RenderPose)

  void init() {
    text.setDefault("hello");
  }

  vrs::DataPieceValue<int64_t> timestamp{"timestamp"};
  vrs::DataPieceVector<vrs::Matrix4Dd> orientation{"orientation"};
  vrs::DataPieceVector<vrs::Matrix3Dd> translation{"translation"};
  // fake data
  vrs::DataPieceVector<int32_t> values{"values"};
  vrs::DataPieceString text{"text"};
};

struct RigidBodyPose : public DataLayoutStruct {
  DATA_LAYOUT_STRUCT(RigidBodyPose)

  vrs::DataPieceVector<vrs::Matrix3Dd> angularVelocity{"angular_velocity"};
  vrs::DataPieceVector<vrs::Matrix3Dd> linearVelocity{"linear_velocity"};
  vrs::DataPieceVector<vrs::Matrix3Dd> angularAcceleration{"angular_acceleration"};
  vrs::DataPieceVector<vrs::Matrix3Dd> linearAcceleration{"linear_acceleration"};
  RenderPose pose{"pose"};
};

struct InputEntry : public DataLayoutStruct {
  DATA_LAYOUT_STRUCT(InputEntry)

  vrs::DataPieceValue<uint8_t> type{"type"};
  vrs::DataPieceValue<uint32_t> buttonState{"button_state"};
  vrs::DataPieceValue<uint32_t> capsenseState{"capsense_state"};
  vrs::DataPieceValue<float> indexTrigger{"index_trigger"};
  vrs::DataPieceValue<float> middleFingerTrigger{"middle_finger_trigger"};
  vrs::DataPieceVector<vrs::Matrix2Df> thumbstick{"thumbstick"};
  RigidBodyPose rigidBody{"rigid_body"};
};

struct Tracking : public AutoDataLayout {
  RigidBodyPose headPose{"head_pose"};
  InputEntry leftController{"left"};
  InputEntry rightController{"right"};
  // fake data
  vrs::DataPieceString removedString{"removed_string"};

  vrs::AutoDataLayoutEnd endLayout;
};

struct ShakenTracking : public AutoDataLayout {
  // add a fake fixed size piece and a fake var size piece
  vrs::DataPieceValue<uint8_t> extra{"extra"};
  vrs::DataPieceString string{"string"};

  // shuffle the order of the other fields
  InputEntry rightController{"right"};
  InputEntry leftController{"left"};
  RigidBodyPose headPose{"head_pose"};

  vrs::AutoDataLayoutEnd endLayout;
};

struct NestedDataLayoutTester : testing::Test {};

TEST_F(NestedDataLayoutTester, nestedTest) {
  Tracking tracking;
  const int64_t timestamp = 12345678;
  const vector<int32_t> values{1, 2, 3};
  const string text = "something to say";
  tracking.rightController.rigidBody.pose.timestamp.set(timestamp);
  tracking.headPose.pose.values.stagedValues() = values;
  tracking.leftController.rigidBody.pose.text.stage(text);
  const string removedString = "to be removed";
  tracking.removedString.stage(removedString);
  // tracking.printLayout(cout);

  // Let's clone the datalayout definition via json, to ensure correct definition transcoding
  string js = tracking.asJson(JsonFormatProfile::VrsFormat);
  unique_ptr<DataLayout> dl = DataLayout::makeFromJson(js);
  ASSERT_NE(dl, nullptr);
  EXPECT_TRUE(tracking.isSame(*dl));

  // Let's save the datalayout into a byte buffer
  DataLayoutChunk chunk(tracking);
  vector<uint8_t> buffer(chunk.size());
  uint8_t* bptr = buffer.data();
  chunk.fillAndAdvanceBuffer(bptr);

  // Inject the collected data into the rebuilt datalayout
  vector<int8_t>& fixedData = dl->getFixedData();
  fixedData.resize(dl->getFixedDataSizeNeeded());
  vector<int8_t>& varData = dl->getVarData();
  ASSERT_GE(buffer.size(), fixedData.size());
  memcpy(fixedData.data(), buffer.data(), fixedData.size());
  size_t varLength = dl->getVarDataSizeFromIndex();
  ASSERT_EQ(buffer.size(), fixedData.size() + varLength);
  varData.resize(varLength);
  if (varLength > 0) {
    memcpy(varData.data(), buffer.data() + fixedData.size(), varLength);
  }

  // test some values by hand
  DataPieceValue<int64_t>* ts = dl->findDataPieceValue<int64_t>("right/rigid_body/pose/timestamp");
  ASSERT_NE(ts, nullptr);
  EXPECT_EQ(ts->get(), timestamp);
  DataPieceString* sp = dl->findDataPieceString("removed_string");
  ASSERT_NE(sp, nullptr);
  EXPECT_EQ(sp->get(), removedString);
  sp = dl->findDataPieceString("string");
  ASSERT_EQ(sp, nullptr);

  // map the layout using a modified version
  ShakenTracking readTracking;
  readTracking.requireAllPieces();
  readTracking.extra.setRequired(false);
  readTracking.string.setRequired(false);
  EXPECT_TRUE(readTracking.mapLayout(*dl));
  EXPECT_FALSE(readTracking.extra.isAvailable());
  EXPECT_FALSE(readTracking.string.isAvailable());

  // Check values via mapping
  EXPECT_EQ(readTracking.rightController.rigidBody.pose.timestamp.get(), timestamp);
  vector<int32_t> readValues;
  EXPECT_TRUE(readTracking.headPose.pose.values.get(readValues));
  EXPECT_EQ(readValues, values);
  const string readText = readTracking.leftController.rigidBody.pose.text.get();
  EXPECT_EQ(text, readText);
  EXPECT_FALSE(readTracking.string.isAvailable());
}

} // namespace
