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

#include <gtest/gtest.h>

#include <vrs/os/FileList.h>
#include <vrs/os/Utils.h>

using namespace std;
using namespace vrs;

static void createFile(const string& path, size_t size) {
  FILE* newFile = os::fileOpen(path, "wb");
  EXPECT_NE(newFile, nullptr);
  string s;
  s.assign(size, 'a');
  os::fileWrite(s.c_str(), size, 1, newFile);
  os::fileClose(newFile);
}

struct FileListTest : testing::Test {
  FileListTest() {
    for (const string& folder : kTestFolders) {
      EXPECT_EQ(os::makeDirectories(os::pathJoin(kSubTestFolder, folder)), 0);
    }
    for (const auto& files : kTestFiles) {
      createFile(os::pathJoin(kSubTestFolder, files.first), files.second);
    }
  }

  const string kSubTestFolder = os::pathJoin(os::getTempFolder(), "file_list_test");
  const string kVrsFilesFolder{"vrs_files"};
  const vector<string> kTestFolders{kVrsFilesFolder};
  const vector<pair<string, size_t>> kTestFiles = {
      {"BUCK", 1},
      {"test1", 2},
      {"test2.bin", 3},
      {"test3.dat", 4},
      {"vrs_files/test_sub1.vrs", 42},
      {"vrs_files/test_sub2.vrs", 43},
      {"vrs_files/test_sub3.vrs", 44},
  };
};

TEST_F(FileListTest, getFilesAndFoldersTest) {
  vector<string> files, folders;
  EXPECT_EQ(os::getFilesAndFolders(kSubTestFolder, files, &folders), 0);
  ASSERT_EQ(files.size(), 4);
  EXPECT_EQ(os::getFilename(files[0]), kTestFiles[0].first);
  EXPECT_EQ(os::getFilename(files[1]), kTestFiles[1].first);
  EXPECT_EQ(os::getFilename(files[2]), kTestFiles[2].first);
  ASSERT_EQ(folders.size(), 1);
  EXPECT_EQ(string(folders[0].data() + folders[0].size() - 9), kVrsFilesFolder);

  vector<string> fullfiles, fullfolders;
  EXPECT_EQ(os::getFilesAndFolders(kSubTestFolder, fullfiles, &fullfolders), 0);
  ASSERT_EQ(fullfiles.size(), files.size());
  ASSERT_EQ(folders, fullfolders);

  vector<string> files2;
  EXPECT_EQ(os::getFilesAndFolders(kSubTestFolder, files2), 0);
  ASSERT_EQ(fullfiles, files2);
}

TEST_F(FileListTest, getFileListTest) {
  vector<string> files;
  EXPECT_EQ(os::getFileList(kSubTestFolder, files, 1), 0);
  ASSERT_EQ(files.size(), 7);
  EXPECT_EQ(os::getFilename(files[0]), kTestFiles[0].first);
  EXPECT_EQ(os::getFilename(files[1]), kTestFiles[1].first);
  EXPECT_EQ(os::getFilename(files[2]), kTestFiles[2].first);
  size_t testVrsFileIndex = 4;
  string testVrsFileName = os::getParentFolder(kTestFiles[testVrsFileIndex].first);
  size_t testVrsFileSize = kTestFiles[testVrsFileIndex].second;
  int VRS_FilesCount = 0;
  for (const string& file : files) {
    if (os::getFilename(os::getParentFolder(file)) == kVrsFilesFolder) {
      VRS_FilesCount++;
    }
    if (os::getFilename(file) == testVrsFileName) {
      EXPECT_EQ(os::getFileSize(file), testVrsFileSize);
    }
  }
  EXPECT_EQ(VRS_FilesCount, 3);
}
