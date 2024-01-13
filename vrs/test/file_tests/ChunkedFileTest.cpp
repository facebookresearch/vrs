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

#include <utility>

#include <gtest/gtest.h>

#include <TestDataDir/TestDataDir.h>
#include <vrs/os/Utils.h>
#include <vrs/test/helpers/VRSTestsHelpers.h>

#if !IS_WINDOWS_PLATFORM()
#include <unistd.h>
#endif

using namespace std;
using namespace vrs;

using coretech::getTestDataDir;

namespace {
int addPies(DiskFile& file, const string& path) {
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
  string kChunkedFile = getTestDataDir() + "/VRS_Files/chunks.vrs";
  string kChunkedFile2 = getTestDataDir() + "/VRS_Files/chunks.vrs_1";
  string kChunkedFile3 = getTestDataDir() + "/VRS_Files/chunks.vrs_2";
  string kMissingFile = getTestDataDir() + "/VRS_Files/does_not_exist.vrs";
};

static const char* kChunkedFileStreamSignature =
    "101-462fa095330f6ac4-1-1-100,200-51b98ee8be872906-1-1-100,372-1b24bc705850ccad-1-1-100";

TEST_F(ChunkedFileTester, ChunkedFileTest) {
  vrs::RecordFileReader file;
  EXPECT_EQ(file.openFile(kChunkedFile), 0);
  EXPECT_EQ(file.getRecordCount(), 306); // number of records if all chunks are found
  EXPECT_EQ(file.getFileChunks().size(), 3);
  EXPECT_TRUE(file.mightContainImages(StreamId(RecordableTypeId::ForwardCameraRecordableClass, 1)));
  EXPECT_FALSE(file.mightContainImages(StreamId(RecordableTypeId::AudioStream, 1)));
  EXPECT_FALSE(file.mightContainImages(StreamId(RecordableTypeId::MotionRecordableClass, 1)));
  EXPECT_FALSE(file.mightContainAudio(StreamId(RecordableTypeId::ForwardCameraRecordableClass, 1)));
  EXPECT_TRUE(file.mightContainAudio(StreamId(RecordableTypeId::AudioStream, 1)));
  EXPECT_FALSE(file.mightContainAudio(StreamId(RecordableTypeId::MotionRecordableClass, 1)));
  EXPECT_EQ(file.getStreamsSignature(), kChunkedFileStreamSignature);
}

TEST_F(ChunkedFileTester, OpenChunkedFileTest) {
  vrs::RecordFileReader file;
  string jsonPath = FileSpec({kChunkedFile, kChunkedFile2, kChunkedFile3}).toJson();
  EXPECT_EQ(file.openFile(jsonPath), 0);
  EXPECT_EQ(file.getRecordCount(), 306); // number of records if all chunks are found
  EXPECT_EQ(file.getFileChunks().size(), 3);
  EXPECT_EQ(file.getStreamsSignature(), kChunkedFileStreamSignature);
}

TEST_F(ChunkedFileTester, MissingChunkChunkedFileTest) {
  vrs::RecordFileReader file;
  string jsonPath = FileSpec({kChunkedFile, kMissingFile, kChunkedFile3}).toJson();
  EXPECT_EQ(file.openFile(jsonPath), DISKFILE_FILE_NOT_FOUND);
}

#if !IS_WINDOWS_PLATFORM()

TEST_F(ChunkedFileTester, LinkedFileTest) {
  vrs::RecordFileReader file;
  // Test that if we link to the first chunk, even from a remote folder,
  // all the chunks are found & indexed
  string linkedFile = os::getTempFolder() + "chunks_link.vrs";
  EXPECT_EQ(::symlink(kChunkedFile.c_str(), linkedFile.c_str()), 0); // create a link
  EXPECT_EQ(file.openFile(linkedFile), 0);
  EXPECT_EQ(file.getRecordCount(), 306);
  EXPECT_EQ(file.getFileChunks().size(), 3);
  EXPECT_EQ(file.getStreamsSignature(), kChunkedFileStreamSignature);

  for (bool checkSignature : {false, true}) {
    FileSpec foundSpec;
    ASSERT_EQ(RecordFileReader::vrsFilePathToFileSpec(linkedFile, foundSpec, checkSignature), 0);
    ASSERT_EQ(foundSpec.chunks.size(), 3);
    EXPECT_EQ(foundSpec.fileHandlerName, DiskFile::staticName());

    EXPECT_EQ(os::getFilename(foundSpec.chunks[0]), os::getFilename(kChunkedFile));
    EXPECT_EQ(os::getFilename(foundSpec.chunks[1]), os::getFilename(kChunkedFile2));
    EXPECT_EQ(os::getFilename(foundSpec.chunks[2]), os::getFilename(kChunkedFile3));
    EXPECT_EQ(os::getFileSize(foundSpec.chunks[0]), os::getFileSize(kChunkedFile));
    EXPECT_EQ(os::getFileSize(foundSpec.chunks[1]), os::getFileSize(kChunkedFile2));
    EXPECT_EQ(os::getFileSize(foundSpec.chunks[2]), os::getFileSize(kChunkedFile3));
  }

  EXPECT_EQ(::unlink(linkedFile.c_str()), 0); // delete that link

  FileSpec spec;
  EXPECT_EQ(RecordFileReader::vrsFilePathToFileSpec(linkedFile, spec), DISKFILE_FILE_NOT_FOUND);
}
#endif // !IS_WINDOWS_PLATFORM

TEST_F(ChunkedFileTester, newChunks) {
  const string testPath = os::getTempFolder() + "chunking.vrs";

  // test regular chunking: path not ending with "_1", path + "_1", path + "_2", etc
  DiskFile file;
  ASSERT_EQ(addPies(file, testPath), 0);
  FileSpec fileSpec;
  ASSERT_EQ(RecordFileReader::vrsFilePathToFileSpec(testPath, fileSpec), 0);
  file.openSpec(fileSpec);
  vector<pair<string, int64_t>> chunks = file.getFileChunks();
  ASSERT_EQ(chunks.size(), 3);
  EXPECT_EQ(chunks[0], make_pair(testPath, static_cast<int64_t>(sizeof(double))));
  EXPECT_EQ(chunks[1], make_pair(testPath + "_1", static_cast<int64_t>(2 * sizeof(double))));
  EXPECT_EQ(chunks[2], make_pair(testPath + "_2", static_cast<int64_t>(3 * sizeof(double))));

  // test regular split-head chunking: path ending with "_1", path + "_2", path + "_3", etc
  ASSERT_EQ(addPies(file, testPath + "_1"), 0);
  // we open testPath, and find it + 3 chunks
  ASSERT_EQ(RecordFileReader::vrsFilePathToFileSpec(testPath, fileSpec), 0);
  file.openSpec(fileSpec);
  chunks = file.getFileChunks();
  ASSERT_EQ(chunks.size(), 4);
  EXPECT_EQ(chunks[0], make_pair(testPath, static_cast<int64_t>(sizeof(double))));
  EXPECT_EQ(chunks[1], make_pair(testPath + "_1", static_cast<int64_t>(sizeof(double))));
  EXPECT_EQ(chunks[2], make_pair(testPath + "_2", static_cast<int64_t>(2 * sizeof(double))));
  EXPECT_EQ(chunks[3], make_pair(testPath + "_3", static_cast<int64_t>(3 * sizeof(double))));
  vrs::test::deleteChunkedFile(file);
}
