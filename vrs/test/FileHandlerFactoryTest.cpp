// Copyright (c) Facebook, Inc. and its affiliates. Confidential and proprietary.

#include <gtest/gtest.h>

#include <TestDataDir/TestDataDir.h>
#include <test_helpers/GTestMacros.h>

#include <vrs/DiskFile.h>
#include <vrs/FileHandlerFactory.h>
#include <vrs/RecordFileReader.h>
#include <vrs/helpers/FileMacros.h>

using namespace std;
using namespace vrs;

struct FileHandlerFactoryTest : testing::Test {
  const string kVrsFilesDir = coretech::getTestDataDir() + "/VRS_Files/";
  const string kFirstChunk = kVrsFilesDir + "chunks.vrs";
  const string kSingleFileJson = FileSpec({kVrsFilesDir + "sample_file.vrs"}).toJson();
  const string kMultiChunksJson =
      FileSpec({kFirstChunk, kFirstChunk + "_1", kFirstChunk + "_2"}).toJson();
  const string kUriSchemeFile = "myscheme:123456";
};

// Fake FileHandler class that just pretends to open any path
class FakeHandler : public DiskFile {
 public:
  FakeHandler(const string& name) {
    fileHandlerName_ = name;
  }
  std::unique_ptr<FileHandler> makeNew() const override {
    return make_unique<FakeHandler>(fileHandlerName_);
  }
  int open(const string& filePath) override {
    return 0;
  }
  int openSpec(const FileSpec& fileSpec) override {
    return 0;
  }
  int delegateOpen(const string& path, std::unique_ptr<FileHandler>& outNewDelegate) override {
    outNewDelegate.reset();
    return 0;
  }
  virtual int delegateOpenSpec(
      const FileSpec& fileSpec,
      std::unique_ptr<FileHandler>& outNewDelegate) override {
    outNewDelegate.reset();
    return 0;
  }
};

static int openVRSFile(const string& path, std::unique_ptr<FileHandler>& outFile) {
  FileSpec fileSpec;
  IF_ERROR_RETURN(RecordFileReader::vrsFilePathToFileSpec(path, fileSpec));
  return FileHandlerFactory::getInstance().delegateOpen(fileSpec, outFile);
}

TEST_F(FileHandlerFactoryTest, ANDROID_DISABLED(OpenSomeRealVRSFiles)) {
  std::unique_ptr<FileHandler> file;

  EXPECT_EQ(openVRSFile(kFirstChunk, file), 0);
  EXPECT_EQ(
      file->getTotalSize(),
      82677); // auto-detection of chunks means the size includes all the chunks
  EXPECT_EQ(file->getFileHandlerName(), DiskFile::staticName());
  file.reset();

  EXPECT_EQ(openVRSFile(kSingleFileJson, file), 0);
  EXPECT_EQ(file->getTotalSize(), 83038);
  EXPECT_EQ(file->getFileHandlerName(), DiskFile::staticName());
  file.reset();

  EXPECT_EQ(openVRSFile(kMultiChunksJson, file), 0);
  EXPECT_EQ(file->getTotalSize(), 82677);
  EXPECT_EQ(file->getFileHandlerName(), DiskFile::staticName());
  file.reset();
}

TEST_F(FileHandlerFactoryTest, testBadFileHandler) {
  RecordFileReader reader;
  EXPECT_NE(reader.openFile("{\"chunks\":[\"somepath\"],\"storage\":\"bad_oil\"}"), 0);
}

TEST_F(FileHandlerFactoryTest, openCustomSchemeUri) {
  FileHandlerFactory& factory = FileHandlerFactory::getInstance();
  std::unique_ptr<FileHandler> file;

  EXPECT_NE(factory.delegateOpen(kUriSchemeFile, file), 0); // fails: no handler for "myscheme"
  EXPECT_FALSE((file));

  // Verify that myscheme URI are handled by our "myscheme" handler
  factory.registerFileHandler(make_unique<FakeHandler>("myscheme"));
  EXPECT_EQ(factory.delegateOpen(kUriSchemeFile, file), 0);
  EXPECT_STREQ(file->getFileHandlerName().c_str(), "myscheme");
  factory.unregisterFileHandler("myscheme");
  file.reset();
}
