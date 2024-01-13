/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <iostream>
#include <limits>
#include <type_traits>

#include <gtest/gtest.h>

#include <vrs/DataPieces.h>

using namespace std;
using namespace vrs;

#define JSON_DUMP false

namespace {

template <typename T, size_t N>
void sequenceInit(MatrixND<T, N>& matrix, T start = 1) {
  T v = start;
  for (size_t x = 0; x < N; x++) {
    for (size_t y = 0; y < N; y++) {
      matrix[x][y] = v;
      v += 1;
    }
  }
}

template <typename T, size_t N>
void sequenceInit(DataPieceValue<MatrixND<T, N>>& field, T start = 1) {
  MatrixND<T, N> matrix;
  sequenceInit(matrix, start);
  field.set(matrix);
}

struct MyConfig : public AutoDataLayout {
  MyConfig() {
    bool_.setDefault(true);
    bools_.setDefault({false, true});
    char_.setRange(-128, 127);
    char_.setDefault('a');
    char_.setRequired();
    int8_.setRange(-128, 127);
    int8_.setDefault(8);
    uint8_.setRange(0, 255);
    int16_.setRange(INT16_MIN, INT16_MAX);
    int16_.setDefault(-16);
    uint16_.setRange(0, UINT16_MAX);
    uint16_.setDefault(16);
    int32_.setRange(INT32_MIN, INT32_MAX);
    uint32_.setRange(0, UINT32_MAX);
    int64_.setRange(INT64_MIN, INT64_MAX);
    uint64_.setRange(0, UINT64_MAX);
    uint64_.setDefault(42);
    float_.setRange(-1.5, 1.5);
    double_.setIncrement(0.025, 0.035);
    double_.setDefault(3.14);
    double_.setRequired(true);
    name_.setDefault("bye");
    calibration_.setDefault({1, 2, 3});
    calibration3_.setDefault({{1, 2}, {3, 4}});
    sequenceInit(calibrationM3Dd_);
    sequenceInit(calibrationM3Df_);
    sequenceInit(calibrationM3Di_);
    sequenceInit(calibrationM4Dd_);
    sequenceInit(calibrationM4Df_);
    sequenceInit(calibrationM4Di_);
    Matrix4Di v;
    for (size_t k = 0; k < calibrationArray_.getArraySize(); k++) {
      sequenceInit(v, static_cast<int32_t>(k));
      calibrationArray_.set(v, k);
    }
  }

  DataPieceValue<Bool> bool_{"bool"};
  DataPieceArray<Bool> bools_{"bools", 2};
  DataPieceValue<char> char_{"char"};
  DataPieceValue<int8_t> int8_{"int8"};
  DataPieceValue<uint8_t> uint8_{"uint8"};
  DataPieceValue<int16_t> int16_{"int16"};
  DataPieceValue<uint16_t> uint16_{"uint16"};
  DataPieceValue<int32_t> int32_{"int32"};
  DataPieceValue<uint32_t> uint32_{"uint32"};
  DataPieceValue<int64_t> int64_{"int64"};
  DataPieceValue<uint64_t> uint64_{"uint64"};
  DataPieceValue<float> float_{"float"};
  DataPieceValue<double> double_{"double"};

  DataPieceArray<char> name_{"my_name", 30};
  DataPieceArray<int32_t> calibration_{"my_calibration", 20};
  DataPieceArray<int32_t> calibration2_{"my_calibration_2", 20};
  DataPieceArray<Point2Di> calibration3_{"my_calibration_3", 2};
  DataPieceValue<Matrix3Dd> calibrationM3Dd_{"my_calibration_M3Dd"};
  DataPieceValue<Matrix3Df> calibrationM3Df_{"my_calibration_M3Df"};
  DataPieceValue<Matrix3Di> calibrationM3Di_{"my_calibration_M3Di"};
  DataPieceValue<Matrix4Dd> calibrationM4Dd_{"my_calibration_M4Dd"};
  DataPieceValue<Matrix4Df> calibrationM4Df_{"my_calibration_M4Df"};
  DataPieceValue<Matrix4Di> calibrationM4Di_{"my_calibration_M4Di"};
  DataPieceArray<Matrix4Di> calibrationArray_{"my_calibration_M4Di", 3};

  AutoDataLayoutEnd end;
};

struct OldConfig : public AutoDataLayout {
  DataPieceValue<int8_t> int8_{"int8"};
  DataPieceValue<uint8_t> uint8_{"uint8"};
  DataPieceValue<int16_t> int16_{"int16"};
  DataPieceValue<uint16_t> uint16_{"uint16"};
  DataPieceValue<uint32_t> uint32_{"uint32"};
  DataPieceValue<int64_t> int64_{"int64"};
  DataPieceValue<uint64_t> uint64_{"uint64"};
  DataPieceValue<double> double_{"double_renamed"};

  DataPieceArray<char> name_{"my_name", 30};
  DataPieceArray<int32_t> calibration_{"my_calibration", 20};
  DataPieceArray<int32_t> calibration2_{"my_calibration_2", 25};

  AutoDataLayoutEnd end;
};

struct DataLayoutTester : testing::Test {
  MyConfig testConfig;
};

} // namespace

TEST_F(DataLayoutTester, testDataLayout) {
#if JSON_DUMP
  testConfig.printLayout(cout);
#endif
  string json = testConfig.asJson(JsonFormatProfile::ExternalPretty);
  size_t lineCount = static_cast<size_t>(count(json.begin(), json.end(), '\n'));
  EXPECT_EQ(lineCount, 410);
#if JSON_DUMP
  cout << "Json: " << json << "\n";
#endif
}

TEST_F(DataLayoutTester, testDefault) {
  MyConfig refConfig;
  memset(refConfig.getFixedData().data(), 0, refConfig.getFixedData().size());
  EXPECT_EQ(refConfig.bool_.get(), false);
  EXPECT_TRUE(refConfig.bool_.set(true));
  EXPECT_EQ(refConfig.bool_.get(), true);

  Bool boolsInit[2]{true, false};
  EXPECT_TRUE(refConfig.bools_.set(boolsInit));
  vector<Bool> bools;
  EXPECT_TRUE(refConfig.bools_.get(bools));
  EXPECT_EQ(bools.size(), 2);
  EXPECT_EQ(bools[0], true);
  EXPECT_EQ(bools[1], false);

  EXPECT_EQ(refConfig.char_.get(), 0);
  EXPECT_EQ(refConfig.int8_.get(), 0);
  EXPECT_EQ(refConfig.uint8_.get(), 0);
  EXPECT_EQ(refConfig.int16_.get(), 0);
  EXPECT_EQ(refConfig.uint16_.get(), 0);
  EXPECT_EQ(refConfig.int32_.get(), 0);
  EXPECT_EQ(refConfig.uint32_.get(), 0);
  EXPECT_EQ(refConfig.int64_.get(), 0);
  EXPECT_EQ(refConfig.uint64_.get(), 0);
  EXPECT_EQ(refConfig.float_.get(), 0);
  EXPECT_EQ(refConfig.double_.get(), 0);

  int32_t calibration[20];
  EXPECT_TRUE(refConfig.calibration_.get(calibration, 20));
  for (int32_t cal : calibration) {
    EXPECT_EQ(cal, 0);
  }
  for (size_t k = 0; k < 20; k++) {
    calibration[k] = static_cast<int32_t>(k);
  }
  refConfig.calibration_.set(calibration, 20);
  vector<int32_t> calibrationVector;
  EXPECT_TRUE(refConfig.calibration_.get(calibrationVector));
  EXPECT_EQ(calibrationVector.size(), 20);
  for (size_t k = 0; k < 20; k++) {
    EXPECT_EQ(calibration[k], calibrationVector[k]);
    calibrationVector[k] = static_cast<int32_t>(100 + k);
  }
  EXPECT_TRUE(refConfig.calibration_.set(calibrationVector));
  EXPECT_TRUE(refConfig.calibration_.get(calibration, 20));
  for (size_t k = 0; k < 20; k++) {
    EXPECT_EQ(calibration[k], static_cast<int32_t>(100 + k));
  }

  EXPECT_TRUE(refConfig.name_.set("hello"));
  vector<char> name;
  EXPECT_TRUE(refConfig.name_.get(name));
  EXPECT_EQ(strcmp(name.data(), "hello"), 0);
  name = refConfig.name_.getDefault();
  EXPECT_EQ(strcmp(name.data(), "bye"), 0);

  MyConfig defaultConfig;
  defaultConfig.getFixedData()
      .clear(); // remove underlying data, forcing all values to their default

  EXPECT_EQ(defaultConfig.bool_.get(), true);
  bools = refConfig.bools_.getDefault();
  EXPECT_EQ(bools.size(), 2);
  EXPECT_EQ(bools[0], false);
  EXPECT_EQ(bools[1], true);

  EXPECT_EQ(defaultConfig.char_.get(), 'a');
  EXPECT_EQ(defaultConfig.int8_.get(), 8);
  EXPECT_EQ(defaultConfig.uint8_.get(), 0);
  EXPECT_EQ(defaultConfig.int16_.get(), -16);
  EXPECT_EQ(defaultConfig.uint16_.get(), 16);
  EXPECT_EQ(defaultConfig.int32_.get(), 0);
  EXPECT_EQ(defaultConfig.uint32_.get(), 0);
  EXPECT_EQ(defaultConfig.int64_.get(), 0);
  EXPECT_EQ(defaultConfig.uint64_.get(), 42);
  EXPECT_EQ(defaultConfig.float_.get(), 0);
  EXPECT_EQ(defaultConfig.double_.get(), 3.14);
}

TEST_F(DataLayoutTester, testDataLayoutMatcher) {
  MyConfig testConfig2;
  EXPECT_TRUE(testConfig2.mapLayout(testConfig));

  OldConfig otherConfig;
  EXPECT_TRUE(otherConfig.mapLayout(testConfig));
  // make a missing required field fail the mapping
  otherConfig.double_.setRequired();
  EXPECT_FALSE(otherConfig.mapLayout(testConfig));
  double v = 0;
  EXPECT_FALSE(otherConfig.double_.get(v));
  EXPECT_FALSE(otherConfig.double_.getDefault(v));

  MyConfig newConfig;
  OldConfig oldConfig;
  EXPECT_FALSE(newConfig.mapLayout(oldConfig)); // chart, which is requried, is missing
  EXPECT_FALSE(newConfig.char_.isAvailable());
  EXPECT_TRUE(newConfig.int8_.isAvailable());
  EXPECT_TRUE(newConfig.uint8_.isAvailable());
  EXPECT_TRUE(newConfig.int16_.isAvailable());
  EXPECT_TRUE(newConfig.uint16_.isAvailable());
  EXPECT_FALSE(newConfig.int32_.isAvailable());
  EXPECT_TRUE(newConfig.uint32_.isAvailable());
  EXPECT_TRUE(newConfig.int64_.isAvailable());
  EXPECT_TRUE(newConfig.uint64_.isAvailable());
  EXPECT_FALSE(newConfig.float_.isAvailable());
  EXPECT_FALSE(newConfig.double_.isAvailable());
  EXPECT_TRUE(newConfig.name_.isAvailable());
  EXPECT_TRUE(newConfig.calibration_.isAvailable());
  EXPECT_TRUE(newConfig.calibration_.getDefault().size() == 20);
  EXPECT_FALSE(newConfig.calibration2_.isAvailable()); // different array size
  EXPECT_FALSE(newConfig.calibration3_.isAvailable());

  // Set the fixed data to random values
  srand(12345);
  auto& fixedData = oldConfig.getFixedData();
  for (int8_t& data : fixedData) {
    data = static_cast<int8_t>(rand());
  }
  // See that we find the same values when using the new and the old layout,
  // when data fields match between the two.

  char chart = 'z';
  EXPECT_FALSE(newConfig.char_.isAvailable());
  EXPECT_FALSE(newConfig.char_.get(chart));
  EXPECT_EQ(chart, 'a');

  int8_t int8 = 0;
  EXPECT_EQ(newConfig.int8_.get(), oldConfig.int8_.get());
  EXPECT_TRUE(newConfig.int8_.get(int8));
  EXPECT_EQ(int8, oldConfig.int8_.get());

  EXPECT_EQ(newConfig.uint8_.get(), oldConfig.uint8_.get());
  EXPECT_EQ(newConfig.int16_.get(), oldConfig.int16_.get());
  EXPECT_EQ(newConfig.uint16_.get(), oldConfig.uint16_.get());
  EXPECT_FALSE(newConfig.int32_.isAvailable());
  EXPECT_EQ(newConfig.uint32_.get(), oldConfig.uint32_.get());
  EXPECT_EQ(newConfig.int64_.get(), oldConfig.int64_.get());
  EXPECT_EQ(newConfig.uint64_.get(), oldConfig.uint64_.get());
  EXPECT_FALSE(newConfig.float_.isAvailable());

  // check missing data, with no default
  float floatt = 0;
  EXPECT_FALSE(newConfig.float_.getDefault(floatt));
  EXPECT_EQ(newConfig.float_.get(), 0);
  floatt = -1;
  EXPECT_FALSE(newConfig.float_.get(floatt));
  EXPECT_EQ(floatt, 0);

  // check missing data, with default
  EXPECT_FALSE(newConfig.double_.isAvailable());
  double doublet_default = 0;
  EXPECT_TRUE(newConfig.double_.getDefault(doublet_default));
  EXPECT_EQ(doublet_default, 3.14);
  EXPECT_EQ(newConfig.double_.get(), doublet_default);
  double doublet = -1;
  EXPECT_FALSE(newConfig.double_.get(doublet));
  EXPECT_EQ(doublet_default, 3.14); // get(value) returns false when a default value is returned

  char name[30];
  EXPECT_TRUE(oldConfig.name_.get(name, sizeof(name)));
  vector<char> nameVector;
  EXPECT_TRUE(newConfig.name_.get(nameVector));
  EXPECT_EQ(nameVector.size(), sizeof(name));
  EXPECT_EQ(memcmp(name, nameVector.data(), sizeof(name)), 0);

  int32_t calibration[20];
  EXPECT_TRUE(oldConfig.calibration_.get(calibration, 20));
  vector<int32_t> calibrationVector;
  EXPECT_TRUE(newConfig.calibration_.get(calibrationVector));
  EXPECT_EQ(calibrationVector.size(), 20);
  EXPECT_EQ(memcmp(calibration, calibrationVector.data(), sizeof(calibration)), 0);

  vector<Point2Di> calibration3;
  EXPECT_FALSE(newConfig.calibration3_.get(calibration3));
  ASSERT_EQ(calibration3.size(), 2);
  EXPECT_EQ(calibration3[0], Point2Di({1, 2}));
  EXPECT_EQ(calibration3[1], Point2Di({3, 4}));

  Point2Di calibration3c[2];
  EXPECT_FALSE(newConfig.calibration3_.get(calibration3c, 2));
  vector<Point2Di> calibration3d;
  for (size_t k = 0; k < 2; k++) {
    calibration3d.push_back(calibration3c[k]);
  }
  EXPECT_EQ(calibration3, calibration3d);
}

namespace {

class VarSizeLayout : public AutoDataLayout {
 public:
  VarSizeLayout() {
    intsWithDefault_.setDefault({1, 2, 3});
    intsWithDefault_.setRequired(true);
    name_.setDefault("default_name");
    label_.setDefault("default_label");
    mapString_.stagedValues() = {{"one", "1"}, {"two", "2"}, {"three", "3"}};
    mapPoint2dd_.stagedValues() = {{"one", {1, 2}}, {"two", {2, 3}}, {"three", {4, 5}}};
  }

  DataPieceValue<int32_t> int32{"an_int32"};
  DataPieceValue<float> afloat{"a_float"};

  DataPieceVector<int32_t> ints_{"ints"};
  DataPieceVector<int32_t> intsWithDefault_{"intsDefault"};
  DataPieceVector<double> doubles_{"doubles"};
  DataPieceVector<string> strings_{"strings"};
  DataPieceVector<string> moreStrings_{"more_strings"};
  DataPieceValue<int32_t> int_{"int"};
  DataPieceString name_{"name"};
  DataPieceString label_{"label"};
  DataPieceString emptyString_{"empty_string"};

  DataPieceStringMap<string> mapString_{"map_string"};
  DataPieceStringMap<Bool> mapBool_{"map_Bool"};
  DataPieceStringMap<char> mapChar_{"map_char"};
  DataPieceStringMap<double> mapDouble_{"map_double"};
  DataPieceStringMap<float> mapFloat_{"map_float"};
  DataPieceStringMap<int64_t> mapInt64_t_{"map_int64_t"};
  DataPieceStringMap<uint64_t> mapUint64_t_{"map_uint64_t"};
  DataPieceStringMap<int32_t> mapInt32_{"map_int32"};
  DataPieceStringMap<uint32_t> mapUint32_t_{"map_uint32_t"};
  DataPieceStringMap<int16_t> mapInt16_t_{"map_int16_t"};
  DataPieceStringMap<uint16_t> mapUint16_t_{"map_uint16_t"};
  DataPieceStringMap<int8_t> mapInt8_t_{"map_int8_t"};
  DataPieceStringMap<uint8_t> mapUint8_t_{"map_uint8_t"};

  DataPieceStringMap<Point2Dd> mapPoint2dd_{"map_point2dd"};
  DataPieceStringMap<Point2Df> mapPoint2df_{"map_point2df"};
  DataPieceStringMap<Point2Di> mapPoint2di_{"map_point2di"};
  DataPieceStringMap<Point3Dd> mapPoint3dd_{"map_point3dd"};
  DataPieceStringMap<Point3Df> mapPoint3df_{"map_point3df"};
  DataPieceStringMap<Point3Di> mapPoint3di_{"map_point3di"};
  DataPieceStringMap<Point4Dd> mapPoint4dd_{"map_point4dd"};
  DataPieceStringMap<Point4Df> mapPoint4df_{"map_point4df"};
  DataPieceStringMap<Point4Di> mapPoint4di_{"map_point4di"};

  DataPieceStringMap<Matrix3Dd> mapMatrix3dd_{"map_matrix3dd"};
  DataPieceStringMap<Matrix3Df> mapMatrix3df_{"map_matrix3df"};
  DataPieceStringMap<Matrix3Di> mapMatrix3di_{"map_matrix3di"};
  DataPieceStringMap<Matrix4Dd> mapMatrix4dd_{"map_matrix4dd"};
  DataPieceStringMap<Matrix4Df> mapMatrix4df_{"map_matrix4df"};
  DataPieceStringMap<Matrix4Di> mapMatrix4di_{"map_matrix4di"};

  AutoDataLayoutEnd end;
};

class OldVarSizeLayout : public AutoDataLayout {
 public:
  OldVarSizeLayout() {
    ints_.stagedValues() = {4, 3, 2, 1};
    moreInts_.stagedValues() = {1, 2, 3, 4, 5, 6};
    doubles_.stagedValues() = {1, 2};
    strings_.stagedValues() = {"Eline", "Marlene", ""};
    mapInt32_.stagedValues() = {{"first", 1}, {"second", 2}, {"third", 3}};
    name_.stage("old_name");
  }

  DataPieceValue<int32_t> int32{"an_int32"};
  DataPieceValue<float> afloat{"a_different_float"};

  DataPieceVector<int32_t> ints_{"ints"};
  DataPieceVector<int32_t> moreInts_{"intsDefaultDifferentName"};
  DataPieceVector<double> doubles_{"doubles"};
  DataPieceVector<string> strings_{"strings"};
  DataPieceStringMap<int32_t> mapInt32_{"map_int32"};
  DataPieceString name_{"name"};
  DataPieceString emptyString_{"empty_string"};

  AutoDataLayoutEnd end;
};

} // namespace

TEST_F(DataLayoutTester, testVarSizeFields) {
  VarSizeLayout varSizeLayout;
  EXPECT_TRUE(varSizeLayout.isVarDataIndexValid());
  varSizeLayout.printLayout(cout);
#if JSON_DUMP
  cout << "Json: " << varSizeLayout.asJson(JsonFormatProfile::EXTERNAL_PRETTY) << "\n";
#endif
  OldVarSizeLayout oldVarSizeLayout;
  EXPECT_TRUE(oldVarSizeLayout.isVarDataIndexValid());
  oldVarSizeLayout.collectVariableDataAndUpdateIndex();

  EXPECT_FALSE(varSizeLayout.mapLayout(oldVarSizeLayout));
  varSizeLayout.printLayout(cout);
#if JSON_DUMP
  cout << "Json: " << varSizeLayout.asJson(JsonFormatProfile::EXTERNAL_PRETTY) << "\n";
#endif

  EXPECT_EQ(varSizeLayout.getDeclaredFixedDataPiecesCount(), 3);
  EXPECT_EQ(varSizeLayout.getAvailableFixedDataPiecesCount(), 1);
  EXPECT_EQ(varSizeLayout.getDeclaredVarDataPiecesCount(), 36);
  EXPECT_EQ(varSizeLayout.getAvailableVarDataPiecesCount(), 6);
  EXPECT_EQ(oldVarSizeLayout.getDeclaredFixedDataPiecesCount(), 2);
  EXPECT_EQ(oldVarSizeLayout.getAvailableFixedDataPiecesCount(), 2);
  EXPECT_EQ(oldVarSizeLayout.getDeclaredVarDataPiecesCount(), 7);
  EXPECT_EQ(oldVarSizeLayout.getAvailableVarDataPiecesCount(), 7);

  EXPECT_TRUE(varSizeLayout.isVarDataIndexValid());
  EXPECT_TRUE(oldVarSizeLayout.isVarDataIndexValid());

  vector<int32_t> values;
  EXPECT_TRUE(varSizeLayout.ints_.get(values));
  EXPECT_TRUE(varSizeLayout.ints_.isAvailable());
  EXPECT_EQ(values.size(), 4);
  EXPECT_EQ(values[0], 4);
  EXPECT_EQ(values[1], 3);
  EXPECT_EQ(values[2], 2);
  EXPECT_EQ(values[3], 1);

  EXPECT_FALSE(varSizeLayout.intsWithDefault_.isAvailable());
  varSizeLayout.intsWithDefault_.get(values);
  EXPECT_EQ(values.size(), 3);
  EXPECT_EQ(values[0], 1);
  EXPECT_EQ(values[1], 2);
  EXPECT_EQ(values[2], 3);

  EXPECT_TRUE(varSizeLayout.doubles_.isAvailable());
  vector<double> dvalues;
  varSizeLayout.doubles_.get(dvalues);
  EXPECT_EQ(dvalues.size(), 2);
  EXPECT_EQ(dvalues[0], 1);
  EXPECT_EQ(dvalues[1], 2);

  EXPECT_TRUE(varSizeLayout.strings_.isAvailable());
  vector<string> strings;
  varSizeLayout.strings_.get(strings);
  EXPECT_EQ(strings.size(), 3);
  EXPECT_TRUE(strings[0] == "Eline");
  EXPECT_TRUE(strings[1] == "Marlene");
  EXPECT_TRUE(strings[2].empty());

  EXPECT_FALSE(varSizeLayout.int_.isAvailable());

  string str;
  EXPECT_TRUE(varSizeLayout.name_.get(str));
  EXPECT_TRUE(str == "old_name");
  EXPECT_TRUE(varSizeLayout.name_.isAvailable());
  EXPECT_TRUE(varSizeLayout.name_.get() == "old_name");

  EXPECT_FALSE(varSizeLayout.label_.get(str));
  EXPECT_TRUE(str == "default_label");
  EXPECT_FALSE(varSizeLayout.label_.isAvailable());
  EXPECT_TRUE(varSizeLayout.label_.get() == "default_label");

  EXPECT_TRUE(varSizeLayout.emptyString_.isAvailable());
  EXPECT_EQ(varSizeLayout.emptyString_.get().size(), 0);
}

namespace {

struct OptionalFields {
  DataPieceString optionalFieldName{"optional_field"};
};

struct LayoutWithOptionalFields : public AutoDataLayout {
  explicit LayoutWithOptionalFields(bool allocateOptionalFields = false)
      : optionalFields(allocateOptionalFields) {}

  DataPieceString normalField{"normal_field"};

  const OptionalDataPieces<OptionalFields> optionalFields;

  AutoDataLayoutEnd end;
};
} // namespace

TEST_F(DataLayoutTester, testOptionalFields) {
  LayoutWithOptionalFields noOptionalField;
  ASSERT_FALSE(noOptionalField.optionalFields);

  LayoutWithOptionalFields hasOptionalField(true);
  ASSERT_TRUE(hasOptionalField.optionalFields);

  ASSERT_EQ(
      hasOptionalField.getDeclaredVarDataPiecesCount(),
      noOptionalField.getDeclaredVarDataPiecesCount() + 1);
}

/// For testing only.
///
/// Definitions, so that we can use numeric_limits<Bool>::lowest(), etc in our tests.
namespace std {
template <>
class numeric_limits<Bool> {
 public:
  static Bool lowest() {
    return false;
  }
  static Bool max() {
    return true;
  }
  static Bool min() {
    return false;
  }
};
} // namespace std

namespace {

struct Counters {
  int pieceCounter = 0;
  size_t arraySize = 10; // shared by all types of array
};

template <typename T>
inline void addTemplatePiece(ManualDataLayout& layout, Counters& c, const T& defaultValue) {
  string valuePieceName = to_string(++c.pieceCounter) + "_value";
  DataPieceValue<T>* newValue = new DataPieceValue<T>(valuePieceName, defaultValue);
  layout.add(unique_ptr<DataPiece>(newValue));
  newValue->setMin(numeric_limits<T>::lowest());
  newValue->setMax(numeric_limits<T>::max());
  if constexpr (std::is_arithmetic_v<T>) {
    newValue->setMinIncrement(numeric_limits<T>::lowest() / 10);
    newValue->setMaxIncrement(numeric_limits<T>::max() / 10);
  }
  newValue->setTag("description", string("this is ") + valuePieceName);
  newValue->setTag("units", "metric");

  string arrayPieceName = to_string(++c.pieceCounter) + "_array";
  DataPieceArray<T>* newArray = new DataPieceArray<T>(arrayPieceName, c.arraySize++);
  layout.add(unique_ptr<DataPiece>(newArray));
  vector<T> values;
  for (size_t k = 0; k < newArray->getArraySize(); ++k) {
    values.push_back(static_cast<T>(c.arraySize + k));
  }
  newArray->setDefault(values);
  newArray->setMin(numeric_limits<T>::lowest());
  newArray->setMax(numeric_limits<T>::max());
  newArray->setTag(arrayPieceName, valuePieceName); // make something variable...

  string vectorPieceName = to_string(++c.pieceCounter) + "_vector";
  DataPieceVector<T>* newVector = new DataPieceVector<T>(vectorPieceName);
  layout.add(unique_ptr<DataPiece>(newVector));
  newVector->setDefault(values);
  newArray->setTag(vectorPieceName, arrayPieceName); // make something variable...

  string stringMapPieceName = to_string(++c.pieceCounter) + "_stringMap";
  DataPieceStringMap<T>* newStringMap = new DataPieceStringMap<T>(stringMapPieceName);
  layout.add(unique_ptr<DataPiece>(newStringMap));
  map<string, T> stringMap;
  stringMap["lowest"] = numeric_limits<T>::lowest();
  stringMap["max"] = numeric_limits<T>::max();
  stringMap["min"] = numeric_limits<T>::min();
  newStringMap->setDefault(stringMap);
  newStringMap->setTag(stringMapPieceName, vectorPieceName); // make something variable...
}

template <typename T>
T makePoint(size_t baseValue) {
  T point;
  for (size_t n = 0; n < T::kSize; ++n) {
    point.dim[n] = static_cast<typename T::type>(baseValue + n);
  }
  return point;
}

template <typename T>
inline void addTemplatePiecePoint(ManualDataLayout& layout, Counters& c) {
  string valuePieceName = to_string(++c.pieceCounter) + "_value";
  DataPieceValue<T>* newValue = new DataPieceValue<T>(valuePieceName);
  layout.add(unique_ptr<DataPiece>(newValue));
  newValue->setTag("description", string("this is ") + valuePieceName);
  newValue->setTag("units", "metric");

  string arrayPieceName = to_string(++c.pieceCounter) + "_array";
  DataPieceArray<T>* newArray = new DataPieceArray<T>(arrayPieceName, c.arraySize++);
  layout.add(unique_ptr<DataPiece>(newArray));
  vector<T> values;
  for (size_t k = 0; k < newArray->getArraySize(); ++k) {
    values.push_back(makePoint<T>(c.arraySize + k));
  }
  newArray->setDefault(values);
  newArray->setMin(numeric_limits<T>::lowest());
  newArray->setMax(numeric_limits<T>::max());
  newArray->setTag(arrayPieceName, valuePieceName); // make something variable...

  string vectorPieceName = to_string(++c.pieceCounter) + "_vector";
  DataPieceVector<T>* newVector = new DataPieceVector<T>(vectorPieceName);
  layout.add(unique_ptr<DataPiece>(newVector));
  newVector->setDefault(values);
  newArray->setTag(vectorPieceName, arrayPieceName); // make something variable...

  string stringMapPieceName = to_string(++c.pieceCounter) + "_stringMap";
  DataPieceStringMap<T>* newStringMap = new DataPieceStringMap<T>(stringMapPieceName);
  layout.add(unique_ptr<DataPiece>(newStringMap));
  map<string, T> stringMap;
  stringMap["one"] = makePoint<T>(c.arraySize + 0);
  stringMap["two"] = makePoint<T>(c.arraySize + 1);
  stringMap["three"] = makePoint<T>(c.arraySize + 2);
  newStringMap->setDefault(stringMap);
  newStringMap->setTag(stringMapPieceName, vectorPieceName); // make something variable...
}

template <typename T>
T makeMatrix(size_t baseValue) {
  T matrix;
  for (size_t n = 0; n < T::kMatrixSize; ++n) {
    matrix[n] = makePoint<PointND<typename T::type, T::kMatrixSize>>(baseValue + n);
  }
  return matrix;
}

template <typename T>
inline void addTemplatePieceMatrix(ManualDataLayout& layout, Counters& c) {
  string valuePieceName = to_string(++c.pieceCounter) + "_value";
  DataPieceValue<T>* newValue = new DataPieceValue<T>(valuePieceName);
  layout.add(unique_ptr<DataPiece>(newValue));
  newValue->setTag("description", string("this is ") + valuePieceName);
  newValue->setTag("units", "metric");

  string arrayPieceName = to_string(++c.pieceCounter) + "_array";
  DataPieceArray<T>* newArray = new DataPieceArray<T>(arrayPieceName, c.arraySize++);
  layout.add(unique_ptr<DataPiece>(newArray));
  vector<T> values;
  for (size_t k = 0; k < newArray->getArraySize(); ++k) {
    values.push_back(makeMatrix<T>(c.arraySize + k));
  }
  newArray->setDefault(values);
  newArray->setMin(numeric_limits<T>::lowest());
  newArray->setMax(numeric_limits<T>::max());
  newArray->setTag(arrayPieceName, valuePieceName); // make something variable...

  string vectorPieceName = to_string(++c.pieceCounter) + "_vector";
  DataPieceVector<T>* newVector = new DataPieceVector<T>(vectorPieceName);
  layout.add(unique_ptr<DataPiece>(newVector));
  newVector->setDefault(values);
  newArray->setTag(vectorPieceName, arrayPieceName); // make something variable...

  string stringMapPieceName = to_string(++c.pieceCounter) + "_stringMap";
  DataPieceStringMap<T>* newStringMap = new DataPieceStringMap<T>(stringMapPieceName);
  layout.add(unique_ptr<DataPiece>(newStringMap));
  map<string, T> stringMap;
  stringMap["one"] = makeMatrix<T>(c.arraySize + 0);
  stringMap["two"] = makeMatrix<T>(c.arraySize + 1);
  stringMap["three"] = makeMatrix<T>(c.arraySize + 2);
  newStringMap->setDefault(stringMap);
  newStringMap->setTag(stringMapPieceName, vectorPieceName); // make something variable...
}

} // namespace

TEST_F(DataLayoutTester, testSerialization) {
  Counters counters;
  ManualDataLayout manualLayout;
  addTemplatePiece<Bool>(manualLayout, counters, true);
  addTemplatePiece<int8_t>(manualLayout, counters, 1);
  addTemplatePiece<uint8_t>(manualLayout, counters, 1);
  addTemplatePiece<int16_t>(manualLayout, counters, 1);
  addTemplatePiece<uint16_t>(manualLayout, counters, 1);
  addTemplatePiece<int32_t>(manualLayout, counters, 1);
  addTemplatePiece<uint32_t>(manualLayout, counters, 1);
  addTemplatePiece<int64_t>(manualLayout, counters, 1);
  addTemplatePiece<uint64_t>(manualLayout, counters, 1);
  addTemplatePiece<float>(manualLayout, counters, 1);
  addTemplatePiece<double>(manualLayout, counters, 1);
  addTemplatePiecePoint<Point2Dd>(manualLayout, counters);
  addTemplatePiecePoint<Point2Df>(manualLayout, counters);
  addTemplatePiecePoint<Point3Dd>(manualLayout, counters);
  addTemplatePiecePoint<Point3Df>(manualLayout, counters);
  addTemplatePiecePoint<Point4Dd>(manualLayout, counters);
  addTemplatePiecePoint<Point4Df>(manualLayout, counters);
  addTemplatePieceMatrix<Matrix3Dd>(manualLayout, counters);
  addTemplatePieceMatrix<Matrix3Df>(manualLayout, counters);
  addTemplatePieceMatrix<Matrix3Di>(manualLayout, counters);
  addTemplatePieceMatrix<Matrix4Dd>(manualLayout, counters);
  addTemplatePieceMatrix<Matrix4Df>(manualLayout, counters);
  addTemplatePieceMatrix<Matrix4Di>(manualLayout, counters);

  DataPieceString* stringPiece =
      new DataPieceString(to_string(++counters.pieceCounter) + "_string");
  manualLayout.add(unique_ptr<DataPiece>(stringPiece));
  stringPiece->setDefault("a default string");

  string vectorPieceName = to_string(++counters.pieceCounter) + "_stringVector";
  DataPieceVector<string>* stringVector = new DataPieceVector<string>(vectorPieceName);
  manualLayout.add(unique_ptr<DataPiece>(stringVector));
  stringVector->stage({"Paris", "New York", "Zurich"});
  stringVector->setDefault({"Marseille", "Tokyo"});

  manualLayout.endLayout();
  manualLayout.requireAllPieces();

  unique_ptr<DataLayout> newManualLayout = DataLayout::makeFromJson(manualLayout.asJson());
#if JSON_DUMP
  cout << "Json: " << manualLayout.asJson(JsonFormatProfile::EXTERNAL_PRETTY) << "\n";
  cout << "New Json: " << newManualLayout.get()->asJson(JsonFormatProfile::EXTERNAL_PRETTY) << "\n";
#endif

  newManualLayout->printLayout(cout);
  EXPECT_TRUE(manualLayout.isSame(*newManualLayout.get()));
  DataPieceValue<Matrix4Dd>* m4d = manualLayout.findDataPieceValue<Matrix4Dd>("81_value");
  ASSERT_NE(m4d, nullptr);
  Matrix4Dd m = m4d->getDefault();
  m[2][3] += 1;
  m4d->setDefault(m);
  EXPECT_FALSE(manualLayout.isSame(*newManualLayout.get()));

  DataPieceArray<Point3Df>* arr = manualLayout.findDataPieceArray<Point3Df>("58_array", 24);
  ASSERT_NE(arr, nullptr);

  DataPieceVector<int64_t>* vec = manualLayout.findDataPieceVector<int64_t>("31_vector");
  ASSERT_NE(vec, nullptr);

  DataPieceString* str = manualLayout.findDataPieceString("93_string");
  ASSERT_NE(str, nullptr);

  DataPieceVector<string>* strings =
      newManualLayout->findDataPieceVector<string>("94_stringVector");
  ASSERT_NE(strings, nullptr);
  vector<string> stringValues;
  strings->get(stringValues);
  EXPECT_EQ(stringValues.size(), 2);
  EXPECT_TRUE(stringValues[0] == "Marseille");
  EXPECT_TRUE(stringValues[1] == "Tokyo");

  manualLayout.collectVariableDataAndUpdateIndex();
  newManualLayout->mapLayout(manualLayout);
  strings->get(stringValues);
  EXPECT_EQ(stringValues.size(), 3);
  EXPECT_TRUE(stringValues[0] == "Paris");
  EXPECT_TRUE(stringValues[1] == "New York");
  EXPECT_TRUE(stringValues[2] == "Zurich");
}

namespace {
struct MetadataTest : public AutoDataLayout {
  MetadataTest() {
    intValue.setRange(10, 20);
    intValue.setMinIncrement(1);
    intValue.setMaxIncrement(3);
    intValue.setDescription("some int");
    intValue.setUnit("meter");
    floatValue.setRange(-10, +100);
  }
  DataPieceValue<int32_t> intValue{"int"};
  DataPieceValue<float> floatValue{"float"};
  DataPieceArray<float> floatArrayValue{"float_array", 2};
  DataPieceVector<float> floatVectorValue{"float_vector"};
  DataPieceStringMap<uint8_t> uintStringMapValue{"uint_string_map"};
  DataPieceString stringValue{"string"};

  AutoDataLayoutEnd end;
};
} // namespace

TEST_F(DataLayoutTester, testMetaData) {
  const MetadataTest data;
  const string js = data.asJson(JsonFormatProfile::VrsFormat);
  unique_ptr<DataLayout> dl = DataLayout::makeFromJson(js);
  ASSERT_NE(dl, nullptr);
  EXPECT_TRUE(data.isSame(*dl));

  const DataPieceValue<int32_t>* intValue = dl->findDataPieceValue<int32_t>("int");
  ASSERT_NE(intValue, nullptr);
  int32_t v = 0;
  EXPECT_TRUE(intValue->getMin(v));
  EXPECT_EQ(v, 10);
  EXPECT_TRUE(intValue->getMax(v));
  EXPECT_EQ(v, 20);
  EXPECT_TRUE(intValue->getMinIncrement(v));
  EXPECT_EQ(v, 1);
  EXPECT_TRUE(intValue->getMaxIncrement(v));
  EXPECT_EQ(v, 3);
  string s;
  EXPECT_TRUE(intValue->getUnit(s));
  EXPECT_STREQ(s.c_str(), "meter");
  EXPECT_TRUE(intValue->getDescription(s));
  EXPECT_STREQ(s.c_str(), "some int");

  const DataPieceValue<float>* floatValue = dl->findDataPieceValue<float>("float");
  ASSERT_NE(floatValue, nullptr);
  float f = 0;
  EXPECT_TRUE(floatValue->getMin(f));
  EXPECT_NEAR(f, -10.f, 0.0001f);
  EXPECT_TRUE(floatValue->getMax(f));
  EXPECT_NEAR(f, 100.f, 0.0001f);

  const DataPieceArray<float>* floatArrayValue = dl->findDataPieceArray<float>("float_array", 2);
  ASSERT_NE(floatArrayValue, nullptr);

  const DataPieceVector<float>* floatVectorValue = dl->findDataPieceVector<float>("float_vector");
  ASSERT_NE(floatVectorValue, nullptr);

  const DataPieceStringMap<uint8_t>* stringMapValue =
      dl->findDataPieceStringMap<uint8_t>("uint_string_map");
  ASSERT_NE(stringMapValue, nullptr);

  const DataPieceString* stringValue = dl->findDataPieceString("string");
  ASSERT_NE(stringValue, nullptr);
}

TEST_F(DataLayoutTester, testStaging) {
  VarSizeLayout layout;
  string name, expectedName;
  EXPECT_FALSE(layout.name_.get(name));
  expectedName = "default_name";
  EXPECT_EQ(name, expectedName);
  vector<int32_t> ints, expectedInts;
  EXPECT_FALSE(layout.ints_.get(ints));
  EXPECT_EQ(ints, expectedInts);
  map<string, string> stringMap, stringMapExpected;
  EXPECT_FALSE(layout.mapString_.get(stringMap));
  EXPECT_EQ(stringMap, stringMapExpected);

  // stage values, collect them, verify reads
  expectedName = "new name";
  layout.name_.stage(expectedName);
  expectedInts = {5, 4, 3, 2, 1};
  layout.ints_.stage(expectedInts);
  stringMapExpected = {{"greeting", "hello"}, {"salutation", "bonjour"}, {"grusse", "moin"}};
  layout.mapString_.stage(stringMapExpected);
  layout.collectVariableDataAndUpdateIndex();
  EXPECT_TRUE(layout.name_.get(name));
  EXPECT_EQ(name, expectedName);
  EXPECT_TRUE(layout.ints_.get(ints));
  EXPECT_EQ(ints, expectedInts);
  EXPECT_TRUE(layout.mapString_.get(stringMap));
  EXPECT_EQ(stringMap, stringMapExpected);

  // change staged values
  layout.name_.stage("some name");
  ints = {1, 2};
  layout.ints_.stage(ints);
  stringMap = {{"answer", "yes"}};
  layout.mapString_.stage(stringMap);

  // overwrite the staged changes
  layout.stageCurrentValues();

  // verify the values
  EXPECT_TRUE(layout.name_.get(name));
  EXPECT_EQ(name, expectedName);
  EXPECT_TRUE(layout.ints_.get(ints));
  EXPECT_EQ(ints, expectedInts);
  EXPECT_TRUE(layout.mapString_.get(stringMap));
  EXPECT_EQ(stringMap, stringMapExpected);
}

static void cloneLayout(ManualDataLayout& copy, DataLayout& original) {
  copy.endLayout();
  // map layouts in each direction, to verify they both have the same fields
  copy.requireAllPieces();
  original.requireAllPieces();
  EXPECT_TRUE(original.mapLayout(copy));
  EXPECT_TRUE(copy.mapLayout(original));
  EXPECT_TRUE(original.isSame(copy));
  EXPECT_TRUE(copy.isSame(original));
}

TEST_F(DataLayoutTester, testCloning) {
  {
    MyConfig original;
    ManualDataLayout copy(original);
    cloneLayout(copy, original);
  }
  {
    VarSizeLayout original;
    ManualDataLayout copy(original);
    cloneLayout(copy, original);
  }
}

namespace {
const uint8_t kInt8{23};
const uint8_t kUint8{200};
const vector<uint32_t> kUint32values{1, 2, 3, 4, 5};
const string kName{"Eline"};
const vector<int8_t> kCharVectorValues{-1, '2', 5};
const map<string, string> kStringStringMap{{"ainee", "Eline"}, {"cadette", "Marlene"}};

struct ALayout : public AutoDataLayout {
  DataPieceValue<int8_t> int8_{"int8"};
  DataPieceValue<uint8_t> uint8_{"uint8"};
  DataPieceArray<uint32_t> uint32Array{"uint32_array", 5};

  DataPieceString name{"string_name"};
  DataPieceVector<int8_t> vectorChar{"vector_char"};
  DataPieceStringMap<string> stringStringMap{"string_string_map"};

  void setValues() {
    int8_.set(kInt8);
    uint8_.set(kUint8);
    uint32Array.set(kUint32values);
    name.stage(kName);
    vectorChar.stage(kCharVectorValues);
    stringStringMap.stage(kStringStringMap);
    collectVariableDataAndUpdateIndex();
  }

  AutoDataLayoutEnd end;
};

void checkValues(DataLayout& layout) {
  ALayout alayout;
  alayout.mapLayout(layout);
  EXPECT_EQ(alayout.int8_.get(), kInt8);
  EXPECT_EQ(alayout.uint8_.get(), kUint8);
  vector<uint32_t> uint32values;
  EXPECT_TRUE(alayout.uint32Array.get(uint32values));
  EXPECT_EQ(uint32values, kUint32values);

  EXPECT_EQ(alayout.name.get(), kName);

  vector<int8_t> charVector;
  EXPECT_TRUE(alayout.vectorChar.get(charVector));
  EXPECT_EQ(charVector, kCharVectorValues);

  map<string, string> stringStringMap;
  EXPECT_TRUE(alayout.stringStringMap.get(stringStringMap));
  EXPECT_EQ(stringStringMap, kStringStringMap);
}
} // namespace

TEST_F(DataLayoutTester, testCopyClonedDataPieceValues) {
  {
    ALayout base;
    ManualDataLayout clone(base);
    clone.endLayout();

    base.setValues();
    ASSERT_TRUE(clone.copyClonedDataPieceValues(base));
    clone.collectVariableDataAndUpdateIndex();
    checkValues(clone);
  }
  {
    ALayout base;
    ManualDataLayout clone(base);
    clone.add(make_unique<DataPieceString>("other_name"));
    clone.add(make_unique<DataPieceValue<double>>("double_value"));
    clone.endLayout();

    base.setValues();
    ASSERT_TRUE(clone.copyClonedDataPieceValues(base));
    clone.collectVariableDataAndUpdateIndex();
    checkValues(clone);
  }
}
