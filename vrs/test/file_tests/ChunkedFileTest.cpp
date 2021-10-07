// Copyright (c) Facebook, Inc. and its affiliates. Confidential and proprietary.

#include <utility>

#include <gtest/gtest.h>

#include <TestDataDir/TestDataDir.h>
#include <vrs/DiskFile.h>
#include <vrs/ErrorCode.h>
#include <vrs/RecordFileReader.h>
#include <vrs/os/Utils.h>
#include <vrs/utils/RecordFileInfo.h>

#if !IS_WINDOWS_PLATFORM()
#include <unistd.h>
#endif

#include <vrs/test/helpers/VRSTestsHelpers.h>

using namespace std;
using namespace vrs;

using coretech::getTestDataDir;

namespace {
int addPies(DiskFile& file, string path) {
  int status = file.create(path);
  EXPECT_EQ(status, 0);
  if (status != 0) {
    return status;
  }
  double pi = 3.14;
  EXPECT_EQ(file.write(pi), 0);
  EXPECT_EQ(file.addChunk(), 0);
  EXPECT_EQ(file.write(pi), 0);
  EXPECT_EQ(file.write(pi), 0);
  EXPECT_EQ(file.addChunk(), 0);
  EXPECT_EQ(file.write(pi), 0);
  EXPECT_EQ(file.write(pi), 0);
  EXPECT_EQ(file.write(pi), 0);
  EXPECT_EQ(file.close(), 0);
  return 0;
}

} // namespace

struct ChunkedFileTester : testing::Test {
  std::string kChunkedFile = getTestDataDir() + "/VRS_Files/chunks.vrs";
  std::string kChunkedFile2 = getTestDataDir() + "/VRS_Files/chunks.vrs_1";
  std::string kChunkedFile3 = getTestDataDir() + "/VRS_Files/chunks.vrs_2";
  std::string kMissingFile = getTestDataDir() + "/VRS_Files/does_not_exist.vrs";
};

TEST_F(ChunkedFileTester, ChunkedFileTest) {
  vrs::RecordFileReader file;
  EXPECT_EQ(file.openFile(kChunkedFile), 0);
  EXPECT_EQ(file.getRecordCount(), 166); // number of records if all chunks are found
  EXPECT_EQ(file.getFileChunks().size(), 3);
}

TEST_F(ChunkedFileTester, OpenChunkedFileTest) {
  vrs::RecordFileReader file;
  std::string jsonPath = FileSpec({kChunkedFile, kChunkedFile2, kChunkedFile3}).toJson();
  EXPECT_EQ(file.openFile(jsonPath), 0);
  EXPECT_EQ(file.getRecordCount(), 166); // number of records if all chunks are found
  EXPECT_EQ(file.getFileChunks().size(), 3);
}

TEST_F(ChunkedFileTester, MissingChunkChunkedFileTest) {
  vrs::RecordFileReader file;
  std::string jsonPath = FileSpec({kChunkedFile, kMissingFile, kChunkedFile3}).toJson();
  EXPECT_EQ(file.openFile(jsonPath), DISKFILE_FILE_NOT_FOUND);
}

#if !IS_WINDOWS_PLATFORM() && !IS_XROS_PLATFORM()

TEST_F(ChunkedFileTester, LinkedFileTest) {
  vrs::RecordFileReader file;
  // Test that if we link to the first chunk, even from a remote folder,
  // all the chunks are found & indexed
  string linkedFile = os::getTempFolder() + "chunks_link.vrs";
  EXPECT_EQ(::symlink(kChunkedFile.c_str(), linkedFile.c_str()), 0); // create a link
  EXPECT_EQ(file.openFile(linkedFile), 0);
  EXPECT_EQ(file.getRecordCount(), 166);
  EXPECT_EQ(file.getFileChunks().size(), 3);

  FileSpec foundSpec;
  ASSERT_EQ(RecordFileReader::vrsFilePathToFileSpec(linkedFile, foundSpec), 0);
  ASSERT_EQ(foundSpec.chunks.size(), 3);
  EXPECT_EQ(foundSpec.fileHandlerName, DiskFile::staticName());

  EXPECT_EQ(os::getFilename(foundSpec.chunks[0]), os::getFilename(kChunkedFile));
  EXPECT_EQ(os::getFilename(foundSpec.chunks[1]), os::getFilename(kChunkedFile2));
  EXPECT_EQ(os::getFilename(foundSpec.chunks[2]), os::getFilename(kChunkedFile3));
  EXPECT_EQ(os::getFileSize(foundSpec.chunks[0]), os::getFileSize(kChunkedFile));
  EXPECT_EQ(os::getFileSize(foundSpec.chunks[1]), os::getFileSize(kChunkedFile2));
  EXPECT_EQ(os::getFileSize(foundSpec.chunks[2]), os::getFileSize(kChunkedFile3));

  EXPECT_EQ(::unlink(linkedFile.c_str()), 0); // delete that link

  EXPECT_EQ(
      RecordFileReader::vrsFilePathToFileSpec(linkedFile, foundSpec), DISKFILE_FILE_NOT_FOUND);
}
#endif // !IS_WINDOWS_PLATFORM && !IS_XROS_PLATFORM()

TEST_F(ChunkedFileTester, DescriptionTest) {
  string filePath = getTestDataDir() + "/VRS_Files/arcata_raw10.vrs";
  vrs::RecordFileReader fileReader;
  ASSERT_EQ(fileReader.openFile(filePath), 0);

  string description =
      vrs::RecordFileInfo::jsonOverview(fileReader, vrs::RecordFileInfo::Details::Everything);

  // Need to substitute the file path with a fixed name that is the same on every platform
  // Note the path appears twice
  for (int i = 0; i < 2; ++i) {
    auto pos = description.find(filePath);
    EXPECT_NE(description.npos, pos);
    description = description.replace(pos, filePath.length(), "arcata_raw10.vrs");
  }

  EXPECT_STREQ(
      description.c_str(),
      "{\"file_name\":\"arcata_raw10.vrs\",\"file_chunks\":[\"arcata_raw10.vrs\"],"
      "\"file_size_short\":\"4.99 MB\",\"file_size\":5235728,"
      "\"tags\":{\"capture_time_epoch\":\"1601144265\",\"device_serial\":\"1WMJA030160226\","
      "\"device_type\":\"Seacliff\",\"device_version\":\"Proto1\",\"os_build_version\":"
      "\"10941900000000000\",\"os_fingerprint\":"
      "\"oculus/qc_seacliff/seacliff:10/QP1A.190711.020/10941900000000000:userdebug/test-keys\","
      "\"recorder_startup_timestamp_ms\":\"204504.989399\",\"recording_duration_sec\":\"0.000000\","
      "\"session_id\":\"5cf09ab7-5aa0-4151-9909-c89336229d89\",\"slam_camera_decimation_factor\":"
      "\"1\"},\"number_of_devices\":2,\"number_of_records\":10,\"start_time\":0.0,\"end_time\":"
      "214.61512226000003,\"devices\":[{\"recordable_name\":\"Device-independent 10-bit Monochrome "
      "Images (hand & body)\",\"recordable_id\":8010,\"instance_id\":1,\"tags\":{\"StreamID\":"
      "\"14925994218970939392\",\"camera_id\":\"0\",\"device_serial\":\"1WMJA030160226\","
      "\"device_type\":\"Seacliff\",\"device_version\":\"Proto1\",\"session_id\":\"5cf09ab7-5aa0-"
      "4151-9909-c89336229d89\"},\"vrs_tag\":{\"DL:Configuration:1:0\":\"{\\\"data_layout\\\":"
      "[{\\\"name\\\":\\\"image_width\\\",\\\"type\\\":\\\"DataPieceValue<uint32_t>\\\",\\\"offset"
      "\\\":0},{\\\"name\\\":\\\"image_height\\\",\\\"type\\\":\\\"DataPieceValue<uint32_t>\\\","
      "\\\"offset\\\":4},{\\\"name\\\":\\\"image_stride\\\",\\\"type\\\":\\\"DataPieceValue<uint32_"
      "t>\\\",\\\"offset\\\":8},{\\\"name\\\":\\\"image_pixel_format\\\",\\\"type\\\":\\\"DataPiece"
      "Value<uint32_t>\\\",\\\"offset\\\":12},{\\\"name\\\":\\\"meta_msgpack\\\",\\\"type\\\":\\\"D"
      "ataPieceVector<uint8_t>\\\",\\\"index\\\":0}]}\",\"DL:Data:4:0\":\"{\\\"data_layout\\\":[{\\"
      "\"name\\\":\\\"ns_from_epoch\\\",\\\"type\\\":\\\"DataPieceValue<int64_t>\\\",\\\"offset\\\""
      ":0,\\\"default\\\":0},{\\\"name\\\":\\\"image_size_bytes\\\",\\\"type\\\":\\\"DataPieceValue"
      "<uint64_t>\\\",\\\"offset\\\":8,\\\"default\\\":0},{\\\"name\\\":\\\"frame_meta\\\",\\\"type"
      "\\\":\\\"DataPieceVector<uint8_t>\\\",\\\"index\\\":0}]}\",\"RF:Configuration:1\":\"data_lay"
      "out\",\"RF:Data:4\":\"data_layout+image/raw\",\"VRS_Original_Recordable_Name\":\"Device-inde"
      "pendent 10-bit Monochrome Images (hand & body)\"},\"configuration\":{\"number_of_records\":1"
      ",\"start_time\":0.0,\"end_time\":0.0},\"state\":{\"number_of_records\":1,\"start_time\":0.0,"
      "\"end_time\":0.0},\"data\":{\"number_of_records\":3,\"start_time\":214.581788302,\"end_time"
      "\":214.61512226000003}},{\"recordable_name\":\"Device-independent 10-bit Monochrome Images ("
      "hand & body)\",\"recordable_id\":8010,\"instance_id\":2,\"tags\":{\"StreamID\":\"14925994218"
      "970939394\",\"camera_id\":\"2\",\"device_serial\":\"1WMJA030160226\",\"device_type\":\"Seacl"
      "iff\",\"device_version\":\"Proto1\",\"session_id\":\"5cf09ab7-5aa0-4151-9909-c89336229d89\"}"
      ",\"vrs_tag\":{\"DL:Configuration:1:0\":\"{\\\"data_layout\\\":[{\\\"name\\\":\\\"image_width"
      "\\\",\\\"type\\\":\\\"DataPieceValue<uint32_t>\\\",\\\"offset\\\":0},{\\\"name\\\":\\\"image"
      "_height\\\",\\\"type\\\":\\\"DataPieceValue<uint32_t>\\\",\\\"offset\\\":4},{\\\"name\\\":"
      "\\\"image_stride\\\",\\\"type\\\":\\\"DataPieceValue<uint32_t>\\\",\\\"offset\\\":8},{\\\"na"
      "me\\\":\\\"image_pixel_format\\\",\\\"type\\\":\\\"DataPieceValue<uint32_t>\\\",\\\"offset\\"
      "\":12},{\\\"name\\\":\\\"meta_msgpack\\\",\\\"type\\\":\\\"DataPieceVector<uint8_t>\\\",\\\""
      "index\\\":0}]}\",\"DL:Data:4:0\":\"{\\\"data_layout\\\":[{\\\"name\\\":\\\"ns_from_epoch\\\""
      ",\\\"type\\\":\\\"DataPieceValue<int64_t>\\\",\\\"offset\\\":0,\\\"default\\\":0},{\\\"name"
      "\\\":\\\"image_size_bytes\\\",\\\"type\\\":\\\"DataPieceValue<uint64_t>\\\",\\\"offset\\\":8"
      ",\\\"default\\\":0},{\\\"name\\\":\\\"frame_meta\\\",\\\"type\\\":\\\"DataPieceVector<uint8_"
      "t>\\\",\\\"index\\\":0}]}\",\"RF:Configuration:1\":\"data_layout\",\"RF:Data:4\":\"data_layo"
      "ut+image/raw\",\"VRS_Original_Recordable_Name\":\"Device-independent 10-bit Monochrome Image"
      "s (hand & body)\"},\"configuration\":{\"number_of_records\":1,\"start_time\":0.0,\"end_time"
      "\":0.0},\"state\":{\"number_of_records\":1,\"start_time\":0.0,\"end_time\":0.0},\"data\":{\""
      "number_of_records\":3,\"start_time\":214.581788302,\"end_time\":214.61512226000003}}]}");

  description = vrs::RecordFileInfo::jsonOverview(fileReader, vrs::RecordFileInfo::Details::Basics);

  { // Need to substitute the file path with a fixed name that is the same on every platform
    auto pos = description.find(filePath);
    EXPECT_NE(description.npos, pos);
    description = description.replace(pos, filePath.length(), "arcata_raw10.vrs");
  }
  EXPECT_STREQ(
      description.c_str(),
      "{\"file_name\":\"arcata_raw10.vrs\",\"file_size_short\":\"4.99 MB\",\"file_size\":5235728}");

  description =
      vrs::RecordFileInfo::jsonOverview(fileReader, vrs::RecordFileInfo::Details::ChunkList);
  { // Need to substitute the file path with a fixed name that is the same on every platform
    auto pos = description.find(filePath);
    EXPECT_NE(description.npos, pos);
    description = description.replace(pos, filePath.length(), "arcata_raw10.vrs");
  }

  EXPECT_STREQ(description.c_str(), "{\"file_chunks\":[\"arcata_raw10.vrs\"]}");

  description =
      vrs::RecordFileInfo::jsonOverview(fileReader, vrs::RecordFileInfo::Details::ListFileTags);
  EXPECT_STREQ(
      description.c_str(),
      "{\"tags\":{\"capture_time_epoch\":\"1601144265\",\"device_serial\":\"1WMJA030160226\",\"devi"
      "ce_type\":\"Seacliff\",\"device_version\":\"Proto1\",\"os_build_version\":\"1094190000000000"
      "0\",\"os_fingerprint\":\"oculus/qc_seacliff/seacliff:10/QP1A.190711.020/10941900000000000:us"
      "erdebug/test-keys\",\"recorder_startup_timestamp_ms\":\"204504.989399\",\"recording_duration"
      "_sec\":\"0.000000\",\"session_id\":\"5cf09ab7-5aa0-4151-9909-c89336229d89\",\"slam_camera_de"
      "cimation_factor\":\"1\"}}");

  // Test other API, with a file that has a single chunk, but a bunch of file tags!
  description = vrs::RecordFileInfo::jsonOverview(
      getTestDataDir() + "/VRS_Files/InsideOutCameraSync.vrs",
      vrs::RecordFileInfo::Details::ListFileTags);
  EXPECT_STREQ(
      description.c_str(),
      "{\"tags\":{\"Cmd_COMPILE_DATE\":\"Aug 21 2017 09:50:48\","
      "\"Cmd_REVISION\":\"9e6daa55fd692cad58f7568cafa0c05f8d80d2fe (* tsfix6)\","
      "\"FW_SYNCBOSS_FINGERPRINT\":\"HMD\",\"HMDSerial\":\"1PASH040A47144\","
      "\"INSIDE_OUT_TRACKER_RECORDING\":\"INSIDE_OUT_TRACKER_RECORDING\","
      "\"OS_FINGERPRINT\":\"oculus/monterey_proto1/proto1:7.1.1/N9F27F/518:userdev/test-keys\","
      "\"device_type\":\"Proto1\"}}");

  description =
      vrs::RecordFileInfo::jsonOverview(fileReader, vrs::RecordFileInfo::Details::MainCounters);
  EXPECT_STREQ(
      description.c_str(),
      "{\"number_of_devices\":2,\"number_of_records\":10,\"start_time\":0.0,"
      "\"end_time\":214.61512226000003}");

  description =
      vrs::RecordFileInfo::jsonOverview(fileReader, vrs::RecordFileInfo::Details::StreamNames);
  EXPECT_STREQ(
      description.c_str(),
      "{\"devices\":[{\"recordable_name\":\"Device-independent 10-bit Monochrome Images (hand & "
      "body)\",\"recordable_id\":8010,\"instance_id\":1},{\"recordable_name\":\"Device-independent "
      "10-bit Monochrome Images (hand & body)\",\"recordable_id\":8010,\"instance_id\":2}]}");

  description =
      vrs::RecordFileInfo::jsonOverview(fileReader, vrs::RecordFileInfo::Details::StreamTags);
  EXPECT_STREQ(
      description.c_str(),
      "{\"devices\":[{\"tags\":{\"StreamID\":\"14925994218970939392\",\"camera_id\":\"0\",\"device_"
      "serial\":\"1WMJA030160226\",\"device_type\":\"Seacliff\",\"device_version\":\"Proto1\",\"ses"
      "sion_id\":\"5cf09ab7-5aa0-4151-9909-c89336229d89\"},\"vrs_tag\":{\"DL:Configuration:1:0\":\""
      "{\\\"data_layout\\\":[{\\\"name\\\":\\\"image_width\\\",\\\"type\\\":\\\"DataPieceValue<uint"
      "32_t>\\\",\\\"offset\\\":0},{\\\"name\\\":\\\"image_height\\\",\\\"type\\\":\\\"DataPieceVal"
      "ue<uint32_t>\\\",\\\"offset\\\":4},{\\\"name\\\":\\\"image_stride\\\",\\\"type\\\":\\\"DataP"
      "ieceValue<uint32_t>\\\",\\\"offset\\\":8},{\\\"name\\\":\\\"image_pixel_format\\\",\\\"type"
      "\\\":\\\"DataPieceValue<uint32_t>\\\",\\\"offset\\\":12},{\\\"name\\\":\\\"meta_msgpack\\\","
      "\\\"type\\\":\\\"DataPieceVector<uint8_t>\\\",\\\"index\\\":0}]}\",\"DL:Data:4:0\":\"{\\\"da"
      "ta_layout\\\":[{\\\"name\\\":\\\"ns_from_epoch\\\",\\\"type\\\":\\\"DataPieceValue<int64_t>"
      "\\\",\\\"offset\\\":0,\\\"default\\\":0},{\\\"name\\\":\\\"image_size_bytes\\\",\\\"type\\\""
      ":\\\"DataPieceValue<uint64_t>\\\",\\\"offset\\\":8,\\\"default\\\":0},{\\\"name\\\":\\\"fram"
      "e_meta\\\",\\\"type\\\":\\\"DataPieceVector<uint8_t>\\\",\\\"index\\\":0}]}\",\"RF:Configura"
      "tion:1\":\"data_layout\",\"RF:Data:4\":\"data_layout+image/raw\",\"VRS_Original_Recordable_N"
      "ame\":\"Device-independent 10-bit Monochrome Images (hand & body)\"}},{\"tags\":{\"StreamID"
      "\":\"14925994218970939394\",\"camera_id\":\"2\",\"device_serial\":\"1WMJA030160226\",\"devic"
      "e_type\":\"Seacliff\",\"device_version\":\"Proto1\",\"session_id\":\"5cf09ab7-5aa0-4151-9909"
      "-c89336229d89\"},\"vrs_tag\":{\"DL:Configuration:1:0\":\"{\\\"data_layout\\\":[{\\\"name\\\""
      ":\\\"image_width\\\",\\\"type\\\":\\\"DataPieceValue<uint32_t>\\\",\\\"offset\\\":0},{\\\"na"
      "me\\\":\\\"image_height\\\",\\\"type\\\":\\\"DataPieceValue<uint32_t>\\\",\\\"offset\\\":4},"
      "{\\\"name\\\":\\\"image_stride\\\",\\\"type\\\":\\\"DataPieceValue<uint32_t>\\\",\\\"offset"
      "\\\":8},{\\\"name\\\":\\\"image_pixel_format\\\",\\\"type\\\":\\\"DataPieceValue<uint32_t>"
      "\\\",\\\"offset\\\":12},{\\\"name\\\":\\\"meta_msgpack\\\",\\\"type\\\":\\\"DataPieceVector<"
      "uint8_t>\\\",\\\"index\\\":0}]}\",\"DL:Data:4:0\":\"{\\\"data_layout\\\":[{\\\"name\\\":\\\""
      "ns_from_epoch\\\",\\\"type\\\":\\\"DataPieceValue<int64_t>\\\",\\\"offset\\\":0,\\\"default"
      "\\\":0},{\\\"name\\\":\\\"image_size_bytes\\\",\\\"type\\\":\\\"DataPieceValue<uint64_t>\\\""
      ",\\\"offset\\\":8,\\\"default\\\":0},{\\\"name\\\":\\\"frame_meta\\\",\\\"type\\\":\\\"DataP"
      "ieceVector<uint8_t>\\\",\\\"index\\\":0}]}\",\"RF:Configuration:1\":\"data_layout\",\"RF:Dat"
      "a:4\":\"data_layout+image/raw\",\"VRS_Original_Recordable_Name\":\"Device-independent 10-bit"
      " Monochrome Images (hand & body)\"}}]}");

  description = vrs::RecordFileInfo::jsonOverview(
      fileReader, vrs::RecordFileInfo::Details::StreamRecordCounts);
  EXPECT_STREQ(
      description.c_str(),
      "{\"devices\":[{\"configuration\":{\"number_of_records\":1,\"start_time\":0.0,\"end_time\":0."
      "0},\"state\":{\"number_of_records\":1,\"start_time\":0.0,\"end_time\":0.0},\"data\":{\"numbe"
      "r_of_records\":3,\"start_time\":214.581788302,\"end_time\":214.61512226000003}},{\"configura"
      "tion\":{\"number_of_records\":1,\"start_time\":0.0,\"end_time\":0.0},\"state\":{\"number_of_"
      "records\":1,\"start_time\":0.0,\"end_time\":0.0},\"data\":{\"number_of_records\":3,\"start_t"
      "ime\":214.581788302,\"end_time\":214.61512226000003}}]}");

  // Missing file: Don't test the actual error code & message, which are platform specific
  description = vrs::RecordFileInfo::jsonOverview("not_an_existing_vrs_file.vrs");
  string resultStart = R"({"file_name":"not_an_existing_vrs_file.vrs","error_code":)" +
      std::to_string(DISKFILE_FILE_NOT_FOUND) + R"(,"error_message":")";
  EXPECT_STREQ(description.substr(0, resultStart.length()).c_str(), resultStart.c_str());
}

TEST_F(ChunkedFileTester, newChunks) {
  const std::string testPath = os::getTempFolder() + "chunking.vrs";

  // test regular chunking: path not ending with "_1", path + "_1", path + "_2", etc
  DiskFile file;
  ASSERT_EQ(addPies(file, testPath), 0);
  FileSpec fileSpec;
  ASSERT_EQ(RecordFileReader::vrsFilePathToFileSpec(testPath, fileSpec), 0);
  file.openSpec(fileSpec);
  vector<std::pair<string, int64_t>> chunks = file.getFileChunks();
  ASSERT_EQ(chunks.size(), 3);
  EXPECT_EQ(chunks[0], std::make_pair(testPath, static_cast<int64_t>(sizeof(double))));
  EXPECT_EQ(chunks[1], std::make_pair(testPath + "_1", static_cast<int64_t>(2 * sizeof(double))));
  EXPECT_EQ(chunks[2], std::make_pair(testPath + "_2", static_cast<int64_t>(3 * sizeof(double))));

  // test regular split-head chunking: path ending with "_1", path + "_2", path + "_3", etc
  ASSERT_EQ(addPies(file, testPath + "_1"), 0);
  // we open testPath, and find it + 3 chunks
  ASSERT_EQ(RecordFileReader::vrsFilePathToFileSpec(testPath, fileSpec), 0);
  file.openSpec(fileSpec);
  chunks = file.getFileChunks();
  ASSERT_EQ(chunks.size(), 4);
  EXPECT_EQ(chunks[0], std::make_pair(testPath, static_cast<int64_t>(sizeof(double))));
  EXPECT_EQ(chunks[1], std::make_pair(testPath + "_1", static_cast<int64_t>(sizeof(double))));
  EXPECT_EQ(chunks[2], std::make_pair(testPath + "_2", static_cast<int64_t>(2 * sizeof(double))));
  EXPECT_EQ(chunks[3], std::make_pair(testPath + "_3", static_cast<int64_t>(3 * sizeof(double))));
  vrs::test::deleteChunkedFile(file);
}
