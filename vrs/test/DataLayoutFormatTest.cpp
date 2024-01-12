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

#include <cmath>

#include <gtest/gtest.h>

#include <test_helpers/GTestMacros.h>
#include <vrs/DataLayoutConventions.h>
#include <vrs/RecordFileReader.h>
#include <vrs/RecordFileWriter.h>
#include <vrs/RecordFormatStreamPlayer.h>
#include <vrs/os/Utils.h>

using namespace vrs;
using namespace vrs::datalayout_conventions;
using namespace std;

namespace TestFormat {

using datalayout_conventions::ImageSpecType;

struct Configuration : public AutoDataLayout {
  enum : uint32_t { kVersion = 1 };

  Configuration() {
    // Testing special numeric values, to validate json generation & parsing
    double _doubleNaN = nan("");
    double _doubleInf = HUGE_VAL;
    float _floatNaN = nanf("");
    float _floatInf = HUGE_VALF;

    // Validate that math macros work the way we expect
    EXPECT_TRUE(isnan(_doubleNaN));
    EXPECT_TRUE(isinf(_doubleInf));
    EXPECT_TRUE(isnan(_floatNaN));
    EXPECT_TRUE(isinf(_floatInf));

    this->doubleNaN.setDefault(_doubleNaN);
    this->doubleInf.setDefault(_doubleInf);
    this->floatNaN.setDefault(_floatNaN);
    this->floatInf.setDefault(_floatInf);
  }

  DataPieceValue<double> doubleValue{"double_value"};
  DataPieceValue<int32_t> intValue{"int_value"};
  DataPieceValue<char> charValue{"char_value"};
  DataPieceArray<int32_t> arrayInts{"int_array", 5};
  DataPieceVector<int16_t> vectorInt16{"int_vector"};
  DataPieceString stringValue{"string_value"};

  DataPieceValue<double> doubleNaN{"double_nan"};
  DataPieceValue<double> doubleInf{"double_inf"};
  DataPieceValue<float> floatNaN{"float_nan"};
  DataPieceValue<float> floatInf{"float_inf"};

  AutoDataLayoutEnd endLayout;
};

struct Data : public AutoDataLayout {
  enum : uint32_t { kVersion = 1 };

  DataPieceValue<ImageSpecType> width{kImageWidth};
  DataPieceValue<ImageSpecType> height{kImageHeight};
  DataPieceValue<ImageSpecType> bytesPerPixels{kImageBytesPerPixel};
  DataPieceValue<ImageSpecType> format{kImagePixelFormat};
  DataPieceString stringData{"string_data"};
  DataPieceVector<string> vectorString{"string_vector"};
  DataPieceStringMap<int32_t> stringMapInt{"string_map_int"};
  DataPieceStringMap<string> stringMapString{"string_map_string"};

  AutoDataLayoutEnd endLayout;
};

struct FormatValues : public AutoDataLayout {
  DataPieceValue<int32_t> int32{"int32_t"};
  DataPieceValue<uint32_t> uint32{"uint32_t"};
  DataPieceValue<int64_t> int64{"int64_t"};
  DataPieceValue<uint64_t> uint64{"uint64_t"};
  DataPieceValue<float> floatv{"float"};
  DataPieceValue<double> doublev{"double"};
  DataPieceStringMap<double> stringMapDouble{"string_map_double"};

  AutoDataLayoutEnd endLayout;
};

} // namespace TestFormat

namespace {

static const ImageSpecType kWidth = 640;
static const ImageSpecType kHeight = 480;
static const ImageSpecType kBytesPerPixel = 1;

template <typename T, size_t S>
void check(
    const vrs::DataPieceValue<vrs::PointND<T, S>>& v,
    const vrs::DataPieceValue<vrs::PointND<T, S>>& r) {
  vrs::PointND<T, S> value, ref;
  v.get(value);
  r.get(ref);
  EXPECT_EQ(value, ref);
}

template <typename T, size_t S>
void check(
    const vrs::DataPieceValue<vrs::MatrixND<T, S>>& v,
    const vrs::DataPieceValue<vrs::MatrixND<T, S>>& r) {
  vrs::MatrixND<T, S> value, ref;
  v.get(value);
  r.get(ref);
  EXPECT_EQ(value, ref);
}

template <typename T, size_t S>
void check(const vrs::DataPieceArray<T>& v, const vrs::DataPieceArray<T>& r) {
  vector<T> value, ref;
  v.get(value);
  r.get(ref);
  EXPECT_EQ(value, ref);
}

template <typename T>
void checkVector(const vrs::DataPieceVector<T>& v, const vrs::DataPieceVector<T>& r) {
  vector<T> values, refValues;
  v.get(values);
  r.get(refValues);
  EXPECT_EQ(values, refValues);
}

template <typename T>
void checkStringMap(const vrs::DataPieceStringMap<T>& v, const vrs::DataPieceStringMap<T>& r) {
  map<string, T> values, refValues;
  v.get(values);
  r.get(refValues);
  EXPECT_EQ(values, refValues);
}

void checkStagedString(const vrs::DataPieceString& v, const vrs::DataPieceString& r) {
  EXPECT_GE(r.stagedValue().size(), 0); // guards against swapping the 'v' and 'r' args
  EXPECT_STREQ(v.get().c_str(), r.stagedValue().c_str());
}

void setConfig(TestFormat::Configuration& config) {
  config.doubleValue.set(123.12);
  config.intValue.set(123);
  config.charValue.set(67);
  int32_t arrayInts[] = {123, 456, 789, 101112, 131415};
  config.arrayInts.set(arrayInts);
  int16_t arrayInt16[] = {98, 587, 67, 587, 5476, 57};
  config.vectorInt16.stage(arrayInt16);
  config.stringValue.stage("San Francisco");
}

void checkConfig(const TestFormat::Configuration& config) {
  TestFormat::Configuration ref;
  setConfig(ref);
  ref.collectVariableDataAndUpdateIndex();
  EXPECT_EQ(config.doubleValue.get(), ref.doubleValue.get());
  EXPECT_EQ(config.intValue.get(), ref.intValue.get());
  EXPECT_EQ(config.charValue.get(), ref.charValue.get());
  check<int32_t, 5>(config.arrayInts, ref.arrayInts);
  checkVector<int16_t>(config.vectorInt16, ref.vectorInt16);
  EXPECT_STREQ(config.stringValue.get().c_str(), ref.stringValue.get().c_str());

  // Default values are stored in the datalayout description, which is what we want to test here,
  // because by default, json doesn't support writing/reading nan & inf values in json.
  EXPECT_TRUE(isnan(config.doubleNaN.getDefault()));
  EXPECT_TRUE(isinf(config.doubleInf.getDefault()));
  EXPECT_TRUE(isnan(config.floatNaN.getDefault()));
  EXPECT_TRUE(isinf(config.floatInf.getDefault()));
}

void setData(TestFormat::Data& data) {
  data.width.set(kWidth);
  data.height.set(kHeight);
  data.bytesPerPixels.set(kBytesPerPixel);
  data.format.set(1);
  data.stringData.stage("hola");
  vector<string> stringVector = {"hi", "bonjour", "allo"};
  data.vectorString.stage(stringVector);
  map<string, int32_t> stringMapInt = {{"first", 1}, {"second", 2}, {"third", 3}};
  data.stringMapInt.stage(stringMapInt);
  map<string, string> stringMapString = {{"first", "un"}, {"second", "deux"}, {"third", "trois"}};
  data.stringMapString.stage(stringMapString);
}

void checkData(const TestFormat::Data& data) {
  TestFormat::Data ref;
  setData(ref);
  checkStagedString(data.stringData, ref.stringData);
  ref.collectVariableDataAndUpdateIndex();
  EXPECT_EQ(data.width.get(), ref.width.get());
  EXPECT_EQ(data.height.get(), ref.height.get());
  EXPECT_EQ(data.bytesPerPixels.get(), ref.bytesPerPixels.get());
  EXPECT_EQ(data.format.get(), ref.format.get());
  EXPECT_STREQ(data.stringData.get().c_str(), ref.stringData.get().c_str());
  checkVector<string>(data.vectorString, ref.vectorString);
  checkStringMap<int32_t>(data.stringMapInt, ref.stringMapInt);
  checkStringMap<string>(data.stringMapString, ref.stringMapString);
}

class RecordableDevice : public Recordable {
 public:
  RecordableDevice() : Recordable(RecordableTypeId::UnitTest1) {
    setCompression(CompressionPreset::None);
    addRecordFormat(
        vrs::Record::Type::CONFIGURATION,
        TestFormat::Configuration::kVersion,
        config_.getContentBlock(),
        {&config_});
    addRecordFormat(
        vrs::Record::Type::DATA,
        TestFormat::Data::kVersion,
        config_.getContentBlock() + ContentBlock(ImageFormat::RAW),
        {&data_});
  }

  double getTimeStamp() {
    static double sTimeStamp = 0;
    return sTimeStamp += 1;
  }

  const Record* createStateRecord() override {
    return nullptr;
  }

  const Record* createConfigurationRecord() override {
    return nullptr;
  }

  void createRecords() {
    setConfig(config_);
    createRecord(
        getTimeStamp(),
        Record::Type::CONFIGURATION,
        TestFormat::Configuration::kVersion,
        DataSource(config_));

    setData(data_);

    // Don't allocate on the stack as e.g. the default stack size on XROS is 64K
    ImageSpecType imageBufferSize = kHeight * kWidth * kBytesPerPixel;
    auto image = make_unique<uint8_t[]>(imageBufferSize);
    createRecord(
        getTimeStamp(),
        Record::Type::DATA,
        TestFormat::Data::kVersion,
        DataSource(data_, {image.get(), imageBufferSize}));
  }

 private:
  TestFormat::Configuration config_;
  TestFormat::Data data_;
};

struct DataLayoutFormatStreamPlayer : public RecordFormatStreamPlayer {
  bool onDataLayoutRead(const CurrentRecord& record, size_t blockIndex, DataLayout& layout)
      override {
    if (record.recordType == Record::Type::CONFIGURATION) {
      if (record.formatVersion == TestFormat::Configuration::kVersion) {
        configCount++;
        const TestFormat::Configuration& config =
            getExpectedLayout<TestFormat::Configuration>(layout, blockIndex);
        checkConfig(config);
      } else {
        ADD_FAILURE();
      }
    } else if (record.recordType == Record::Type::DATA) {
      if (record.formatVersion == TestFormat::Data::kVersion) {
        dataCount++;
        const TestFormat::Data& data = getExpectedLayout<TestFormat::Data>(layout, blockIndex);
        checkData(data);
      } else {
        ADD_FAILURE();
      }
    } else {
      ADD_FAILURE();
    }
    return true;
  }

  bool onUnsupportedBlock(const CurrentRecord& r, size_t index, const ContentBlock& cb) override {
    ADD_FAILURE();
    return RecordFormatStreamPlayer::onUnsupportedBlock(r, index, cb);
  }

  bool onImageRead(const CurrentRecord&, size_t, const ContentBlock& content) override {
    imageCount++;
    EXPECT_EQ(content.getBlockSize(), kHeight * kWidth * kBytesPerPixel);
    return true;
  }

  int configCount = 0;
  int dataCount = 0;
  int imageCount = 0;
};

struct DataLayoutFormatTester : testing::Test {
  int createFile() {
    RecordFileWriter fileWriter;
    RecordableDevice device;
    fileWriter.addRecordable(&device);
    device.createRecords();
    return fileWriter.writeToFile(fileName);
  }

  int checkFile() {
    RecordFileReader fileReader;
    RETURN_ON_FAILURE(fileReader.openFile(fileName));
    EXPECT_TRUE(fileReader.hasIndex());

    const set<StreamId>& streamIds = fileReader.getStreams();
    EXPECT_EQ(streamIds.size(), 1);
    StreamId id = *streamIds.begin();
    EXPECT_EQ(id.getTypeId(), RecordableTypeId::UnitTest1);

    DataLayoutFormatStreamPlayer streamPlayer;
    fileReader.setStreamPlayer(id, &streamPlayer);
    RETURN_ON_FAILURE(fileReader.readAllRecords());

    EXPECT_EQ(streamPlayer.configCount, 1);
    EXPECT_EQ(streamPlayer.dataCount, 1);
    EXPECT_EQ(streamPlayer.imageCount, 1);

    return fileReader.closeFile();
  }

  string fileName = os::getTempFolder() + "DataLayoutFormatTester.vrs";
};

} // namespace

TEST_F(DataLayoutFormatTester, DataLayoutFormatTest) {
  ASSERT_EQ(createFile(), 0);
  EXPECT_EQ(checkFile(), 0);
  os::remove(fileName);
}

#define SAMPLE_EPOCH_TIME 2000000000

TEST_F(DataLayoutFormatTester, FormatValuesTest) {
  using namespace TestFormat;
  FormatValues valuesdl;
  valuesdl.int32.set(SAMPLE_EPOCH_TIME);
  valuesdl.uint32.set(SAMPLE_EPOCH_TIME);
  valuesdl.int64.set(SAMPLE_EPOCH_TIME);
  valuesdl.uint64.set(SAMPLE_EPOCH_TIME);
  valuesdl.doublev.set(1.7044e9);
  valuesdl.floatv.set(SAMPLE_EPOCH_TIME);
  valuesdl.stringMapDouble.stagedValues()["walltime"] = SAMPLE_EPOCH_TIME;
  valuesdl.stringMapDouble.stagedValues()["arrival"] = 1.7044e9;

  valuesdl.collectVariableDataAndUpdateIndex();

  stringstream ss;
  valuesdl.printLayoutCompact(ss);
  EXPECT_EQ(
      ss.str(),
      "  int32_t: 2000000000\n"
      "  uint32_t: 2000000000\n"
      "  int64_t: 2000000000\n"
      "  uint64_t: 2000000000\n"
      "  float: 2e+09\n"
      "  double: 1704400000.000\n"
      "  string_map_double, 2 values:\n"
      "      \"arrival\": 1704400000.000\n"
      "      \"walltime\": 2000000000.000\n");
}
