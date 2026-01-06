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

#include <memory>

#include <gtest/gtest.h>

#define DEFAULT_LOG_CHANNEL "UserRecordsFileHandler"
#include <logging/Log.h>

#include <vrs/FileHandlerFactory.h>
#include <vrs/RecordFileReader.h>
#include <vrs/os/Utils.h>

#include <vrs/test/helpers/VRSTestsHelpers.h>

using namespace std;
using namespace vrs;
using namespace vrs::test;

namespace {

struct UserRecordsFileHandlerTest : testing::Test {};

string kUserFileRecordsFileHandler = "UserRecordsFileHandler";

#define METHOD_NOT_SUPPORTED() \
  throw std::runtime_error(    \
      fmt::format("Unsupported method: {}, {}:{}", __FUNCTION__, __FILE__, __LINE__))

class UserRecordsFileHandler : public WriteFileHandler {
 public:
  UserRecordsFileHandler() = default;
  std::unique_ptr<FileHandler> makeNew() const override {
    return make_unique<UserRecordsFileHandler>();
  }
  const string& getFileHandlerName() const override {
    return kUserFileRecordsFileHandler;
  }

  /// Minimal implementations needed for a custom WriteFileHandler used for data writes only
  /// It can only write forward, no seek operations, no read back
  int create(const string& newFilePath, const FileSpec::Extras& /* options = {} */) override {
    if (file_ != nullptr) {
      return FILE_ALREADY_OPEN;
    }
    file_ = os::fileOpen(newFilePath, "wb");
    if (file_ == nullptr) {
      lastError_ = errno;
      return lastError_;
    }
    lastError_ = 0;
    return lastError_;
  }
  int write(const void* buffer, size_t length) override {
    if (file_ == nullptr) {
      return NO_FILE_OPEN;
    }
    if (::fwrite(static_cast<const char*>(buffer), 1, length, file_) != length) {
      if (ferror(file_) != 0) {
        lastError_ = errno;
      } else {
        lastError_ = DISKFILE_PARTIAL_WRITE_ERROR;
      }
    }
    return lastError_;
  }
  int64_t getPos() const override {
    return os::fileTell(file_); // amount of data writen out so far in this file
  }
  int close() override {
    if (file_ != nullptr) {
      lastError_ = os::fileClose(file_);
      file_ = nullptr;
    }
    int status = lastError_;
    lastError_ = 0;
    return status;
  }
  int getLastError() const override {
    return lastError_;
  }

  /// Trivial implementations needed for a custom WriteFileHandler used for data writes only
  bool isOpened() const override {
    return file_ != nullptr;
  }
  bool isEof() const override {
    return true; // since we apend only, we are always at the end of the file
  }
  int64_t getChunkPos() const override {
    return getPos();
  }
  bool reopenForUpdatesSupported() const override {
    return false;
  }
  bool isReadOnly() const override {
    return false;
  }
  bool isRemoteFileSystem() const override {
    return false;
  }
  int parseUri(FileSpec& intOutFileSpec, size_t /*colonIndex*/) const override {
    intOutFileSpec.chunks.resize(1);
    return FileSpec::parseUri(
        intOutFileSpec.uri,
        intOutFileSpec.fileHandlerName,
        intOutFileSpec.chunks[0],
        intOutFileSpec.extras);
  }

  /// No implementation needed for a custom WriteFileHandler used for data writes only
  int reopenForUpdates() override {
    METHOD_NOT_SUPPORTED();
  }
  int overwrite(const void*, size_t) override {
    METHOD_NOT_SUPPORTED();
  }
  int truncate() override {
    METHOD_NOT_SUPPORTED();
  }
  size_t getLastRWSize() const override {
    METHOD_NOT_SUPPORTED();
  }
  int addChunk() override {
    METHOD_NOT_SUPPORTED();
  }
  bool getCurrentChunk(string&, size_t&) const override {
    METHOD_NOT_SUPPORTED();
  }
  int openSpec(const FileSpec&) override {
    METHOD_NOT_SUPPORTED();
  }
  void forgetFurtherChunks(int64_t) override {
    METHOD_NOT_SUPPORTED();
  }
  vector<std::pair<string, int64_t>> getFileChunks() const override {
    METHOD_NOT_SUPPORTED();
  }
  int skipForward(int64_t) override {
    METHOD_NOT_SUPPORTED();
  }
  int setPos(int64_t) override {
    METHOD_NOT_SUPPORTED();
  }
  int read(void*, size_t) override {
    METHOD_NOT_SUPPORTED();
  }
  int getChunkRange(int64_t&, int64_t&) const override {
    METHOD_NOT_SUPPORTED();
  }
  int64_t getTotalSize() const override {
    METHOD_NOT_SUPPORTED();
  }

 private:
  FILE* file_{nullptr};
  int lastError_{0};
};

} // namespace

TEST_F(UserRecordsFileHandlerTest, userRecordsFileHandler) {
  // register the custom file handler
  FileHandlerFactory::getInstance().registerFileHandler(make_unique<UserRecordsFileHandler>());

  const string testPath = os::pathJoin(os::getTempFolder(), "userRecordsFileHandler.vrs");
  string uriPath = kUserFileRecordsFileHandler + ":" + testPath;

  CreateParams t(uriPath);
  EXPECT_EQ(threadedCreateRecords(t), 0);

  RecordFileReader reader;
  ASSERT_EQ(reader.openFile(testPath), 0);
  size_t recordCount = reader.getIndex().size();
  EXPECT_EQ(recordCount, kClassicFileConfig.totalRecordCount);
  reader.closeFile();

  deleteChunkedFile(testPath);
}
