// Facebook Technologies, LLC Proprietary and Confidential.

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
  string vrsFilesDir = coretech::getTestDataDir() + "/VRS_Files/";
  string singleFile = vrsFilesDir + "chunks.vrs";
  string singleFileJson = FileSpec({vrsFilesDir + "VRSTestRecording.vrs"}).toJson();
  string chunkedFile = FileSpec({singleFile, singleFile + "_1", singleFile + "_2"}).toJson();
  string gaiaFile = "gaia:123456";
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

  EXPECT_EQ(openVRSFile(singleFile, file), 0);
  EXPECT_EQ(
      file->getTotalSize(),
      13594); // auto-detection of chunks means the size includes all the chunks
  EXPECT_EQ(file->getFileHandlerName(), DiskFile::staticName());
  file.reset();

  EXPECT_EQ(openVRSFile(singleFileJson, file), 0);
  EXPECT_EQ(file->getTotalSize(), 137551082);
  EXPECT_EQ(file->getFileHandlerName(), DiskFile::staticName());
  file.reset();

  EXPECT_EQ(openVRSFile(chunkedFile, file), 0);
  EXPECT_EQ(file->getTotalSize(), 13594);
  EXPECT_EQ(file->getFileHandlerName(), DiskFile::staticName());
  file.reset();
}

TEST_F(FileHandlerFactoryTest, testBadFileHandler) {
  RecordFileReader reader;
  EXPECT_NE(reader.openFile("{\"chunks\":[\"somepath\"],\"storage\":\"bad_oil\"}"), 0);
}

TEST_F(FileHandlerFactoryTest, OpenGaiaUris) {
  FileHandlerFactory& factory = FileHandlerFactory::getInstance();
  std::unique_ptr<FileHandler> file;

  EXPECT_NE(factory.delegateOpen(gaiaFile, file), 0); // fails: no handler for "gaia"
  EXPECT_FALSE((file));

  // Verify that gaia URI are handled by our fake gaia handler
  factory.registerFileHandler(make_unique<FakeHandler>("gaia"));
  EXPECT_EQ(factory.delegateOpen(gaiaFile, file), 0);
  EXPECT_STREQ(file->getFileHandlerName().c_str(), "gaia");
  factory.unregisterFileHandler("gaia");
  file.reset();
}
