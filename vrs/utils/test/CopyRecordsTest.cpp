// Copyright (c) Facebook, Inc. and its affiliates. Confidential and proprietary.

#include <gtest/gtest.h>

#include <TestDataDir/TestDataDir.h>

#include <vrs/RecordFileReader.h>
#include <vrs/os/Utils.h>
#include <vrs/utils/CopyRecords.h>
#include <vrs/utils/Validation.h>

using namespace std;
using namespace vrs;
using namespace vrs::utils;

namespace {

class MyFilter : public RecordFilterCopier {
 public:
  MyFilter(
      vrs::RecordFileReader& fileReader,
      vrs::RecordFileWriter& fileWriter,
      vrs::StreamId id,
      const CopyOptions& copyOptions,
      const string& calibration)
      : RecordFilterCopier(fileReader, fileWriter, id, copyOptions), calibration_{calibration} {}
  bool shouldCopyVerbatim(const CurrentRecord& record) override {
    return record.recordType != Record::Type::CONFIGURATION;
  }
  void doDataLayoutEdits(const CurrentRecord&, size_t, DataLayout& datalayout) override {
    DataPieceString* calibration = datalayout.findDataPieceString("factory_calibration");
    EXPECT_NE(calibration, nullptr);
    calibration->stage("Patched calibration for " + calibration_);
  }

 private:
  string calibration_;
};

std::unique_ptr<StreamPlayer> makeDataLayoutFilter(
    RecordFileReader& fileReader,
    RecordFileWriter& fileWriter,
    StreamId streamId,
    const CopyOptions& copyOptions) {
  if (streamId.getTypeId() == RecordableTypeId::SlamCameraData ||
      streamId.getTypeId() == RecordableTypeId::ConstellationCameraData ||
      streamId.getTypeId() == RecordableTypeId::SlamImuData ||
      streamId.getTypeId() == RecordableTypeId::SlamMagnetometerData) {
    return make_unique<MyFilter>(
        fileReader, fileWriter, streamId, copyOptions, streamId.getTypeName());
  } else {
    return make_unique<Copier>(fileReader, fileWriter, streamId, copyOptions);
  }
}

} // namespace

struct CopyRecordsTest : testing::Test {};

TEST_F(CopyRecordsTest, VerbatimCopy) {
  Recordable::resetNewInstanceIds();

  const std::string kSourceFile =
      string(coretech::getTestDataDir()) + "/VRS_Files/InsideOutMonterey.vrs";
  const std::string outputFile = os::getTempFolder() + "InsideOutMonterey-copy.vrs";

  CopyOptions options(false); // override compression setting, threadings, chunking, etc.
  options.setCompressionPreset(CompressionPreset::Lz4Fast);

  FilteredVRSFileReader filteredReader(kSourceFile);
  ASSERT_EQ(filteredReader.openFile(), 0);
  string originalChecksum = checkRecords(filteredReader, options, CheckType::Checksum);

  // Copy the file, as is
  int statusCode = copyRecords(filteredReader, outputFile, options);
  EXPECT_EQ(statusCode, 0);

  FilteredVRSFileReader outputReader(outputFile);
  ASSERT_EQ(outputReader.openFile(), 0);

  // Verify that the original & the copy have the same logicial checksum
  EXPECT_EQ(checkRecords(outputReader, options, CheckType::Checksum), originalChecksum);

  remove(outputFile.c_str());
}

TEST_F(CopyRecordsTest, DataLayoutFilter) {
  Recordable::resetNewInstanceIds();

  const std::string kSourceFile =
      string(coretech::getTestDataDir()) + "/VRS_Files/InsideOutMonterey.vrs";
  const std::string outputFile = os::getTempFolder() + "datalayoutfiltered.vrs";

  CopyOptions options(false); // override compression setting, threadings, chunking, etc.
  options.setCompressionPreset(CompressionPreset::Lz4Fast);

  FilteredVRSFileReader filteredReader(kSourceFile);
  ASSERT_EQ(filteredReader.openFile(), 0);

  // Run the filter
  int statusCode = copyRecords(filteredReader, outputFile, options, {}, makeDataLayoutFilter);
  EXPECT_EQ(statusCode, 0);

  FilteredVRSFileReader outputReader(outputFile);
  ASSERT_EQ(outputReader.openFile(), 0);
  EXPECT_EQ(outputReader.reader.getStreams().size(), 10);
  EXPECT_EQ(outputReader.reader.getIndex().size(), 243);
  EXPECT_EQ(outputReader.reader.getTags().size(), 6);
  EXPECT_EQ(outputReader.reader.getStreams().size(), filteredReader.reader.getStreams().size());
  EXPECT_EQ(outputReader.reader.getIndex().size(), filteredReader.reader.getIndex().size());
  EXPECT_EQ(outputReader.reader.getTags().size(), filteredReader.reader.getTags().size());

  // We have verified that the processed file was modified as expected, and we're just capturing
  // the checksum of that verified file...
  EXPECT_EQ(checkRecords(outputReader, options, CheckType::Checksum), "a8337099f2139304");

  remove(outputFile.c_str());
}

namespace {

class RecordFilter : public RecordFilterCopier {
 public:
  RecordFilter(
      vrs::RecordFileReader& fileReader,
      vrs::RecordFileWriter& fileWriter,
      vrs::StreamId id,
      const CopyOptions& copyOptions)
      : RecordFilterCopier(fileReader, fileWriter, id, copyOptions) {}
  bool shouldCopyVerbatim(const CurrentRecord& record) override {
    return false;
  }
  void doDataLayoutEdits(const CurrentRecord& record, size_t blockIndex, DataLayout& datalayout)
      override {
    DataPieceString* deviceType = datalayout.findDataPieceString("device_type");
    if (deviceType != nullptr) {
      deviceType->stage(deviceType->get() + "_modified");
    }
  }
  static void flipBuffer(vector<uint8_t>& buffer) {
    if (buffer.empty()) {
      return;
    }
    uint8_t* left = buffer.data();
    uint8_t* right = left + buffer.size() - 1;
    while (left < right) {
      uint8_t temp = *left;
      *left++ = *right;
      *right-- = temp;
    }
  }
  void filterImage(const CurrentRecord&, size_t, const ContentBlock&, vector<uint8_t>& p) override {
    flipBuffer(p);
  }
  void filterAudio(const CurrentRecord&, size_t, const ContentBlock&, vector<uint8_t>& s) override {
    flipBuffer(s);
  }
};

std::unique_ptr<StreamPlayer> makeRecordFilter(
    RecordFileReader& fileReader,
    RecordFileWriter& fileWriter,
    StreamId streamId,
    const CopyOptions& copyOptions) {
  if (streamId.getTypeId() == RecordableTypeId::PolarisCamera ||
      streamId.getTypeId() == RecordableTypeId::PolarisAudio) {
    return make_unique<RecordFilter>(fileReader, fileWriter, streamId, copyOptions);
  } else {
    return make_unique<Copier>(fileReader, fileWriter, streamId, copyOptions);
  }
}

} // namespace

TEST_F(CopyRecordsTest, RecordFilter) {
  Recordable::resetNewInstanceIds();

  const std::string kSourceFile = string(coretech::getTestDataDir()) + "/VRS_Files/short_audio.vrs";
  const std::string outputFile = os::getTempFolder() + "recordfiltered.vrs";

  CopyOptions options(false); // override compression setting, threadings, chunking, etc.
  options.setCompressionPreset(CompressionPreset::Lz4Fast);

  FilteredVRSFileReader filteredReader(kSourceFile);
  ASSERT_EQ(filteredReader.openFile(), 0);

  // Run the filter
  int statusCode = copyRecords(filteredReader, outputFile, options, {}, makeRecordFilter);
  EXPECT_EQ(statusCode, 0);

  FilteredVRSFileReader outputReader(outputFile);
  ASSERT_EQ(outputReader.openFile(), 0);
  EXPECT_EQ(outputReader.reader.getStreams().size(), 5);
  EXPECT_EQ(outputReader.reader.getIndex().size(), 147);
  EXPECT_EQ(outputReader.reader.getTags().size(), 2);
  EXPECT_EQ(outputReader.reader.getStreams().size(), filteredReader.reader.getStreams().size());
  EXPECT_EQ(outputReader.reader.getIndex().size(), filteredReader.reader.getIndex().size());
  EXPECT_EQ(outputReader.reader.getTags().size(), filteredReader.reader.getTags().size());

  // We have verified that the processed file was modified as expected, and we're just capturing
  // the checksum of that verified file...
  EXPECT_EQ(checkRecords(outputReader, options, CheckType::Checksum), "80bf145b1f109db0");

  remove(outputFile.c_str());
}
