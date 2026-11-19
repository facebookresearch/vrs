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

#include <TestDataDir/TestDataDir.h>

#include <vrs/DiskFile.h>
#include <vrs/ErrorCode.h>
#include <vrs/RecordFileReader.h>
#include <vrs/os/Utils.h>

using namespace std;
using namespace vrs;

namespace {

constexpr size_t kOpenPrefetchSize = 4 * 1024 * 1024;

using Sequence = vector<pair<size_t, size_t>>;

// A DiskFile that may pretend to be remote, recording the read sequences declared and the reads.
class RecordingFile : public DiskFile {
 public:
  RecordingFile(bool remote, CachingStrategy strategy) : remote_{remote}, strategy_{strategy} {}

  bool isRemoteFileSystem() const override {
    return remote_;
  }
  bool prefetchReadSequence(const Sequence& sequence, bool clearSequence) override {
    declarations.emplace_back(sequence, clearSequence);
    return true;
  }
  bool setCachingStrategy(CachingStrategy strategy) override {
    strategy_ = strategy;
    return true;
  }
  CachingStrategy getCachingStrategy() const override {
    return strategy_;
  }

  using DiskFile::read;
  int read(void* buffer, size_t length) override {
    const int64_t offset = getPos();
    if (length > 0) {
      reads.push_back({offset, length, strategy_});
    }
    if (failFrom >= 0 && offset + static_cast<int64_t>(length) > failFrom) {
      return DISKFILE_NOT_ENOUGH_DATA;
    }
    return DiskFile::read(buffer, length);
  }

  bool isDeclared(int64_t offset, size_t length) const {
    for (const auto& declaration : declarations) {
      for (const auto& range : declaration.first) {
        if (offset >= static_cast<int64_t>(range.first) &&
            offset + length <= range.first + range.second) {
          return true;
        }
      }
    }
    return false;
  }

  struct Read {
    int64_t offset;
    size_t length;
    CachingStrategy strategy;
  };
  vector<pair<Sequence, bool>> declarations;
  vector<Read> reads;
  int64_t failFrom = -1;

 private:
  bool remote_;
  CachingStrategy strategy_;
};

string getTestFilePath(const string& name) {
  return os::pathJoin(coretech::getTestDataDir(), "VRS_Files/" + name);
}

struct OpenedFile {
  int status;
  int64_t size;
  unique_ptr<RecordFileReader> reader;
  RecordingFile* file;
};

OpenedFile openFile(
    const string& name,
    bool remote,
    CachingStrategy strategy = CachingStrategy::Passive,
    int64_t failFrom = -1) {
  auto file = make_unique<RecordingFile>(remote, strategy);
  file->failFrom = failFrom;
  OpenedFile opened{0, 0, make_unique<RecordFileReader>(), file.get()};
  opened.reader->setFileHandler(std::move(file));
  FileSpec spec({getTestFilePath(name)});
  if (failFrom >= 0) {
    spec.extras[kFailFastOnIncompleteIndex] = "1";
  }
  opened.status = opened.reader->openFile(spec);
  opened.size = os::getFileSize(spec.chunks.front());
  return opened;
}

} // namespace

TEST(RecordFileReaderTest, localOpenDeclaresNothing) {
  OpenedFile opened = openFile("sample_file.vrs", /* remote */ false);
  ASSERT_EQ(opened.status, 0);
  EXPECT_TRUE(opened.file->declarations.empty());
}

// Remote opens declare the start of the file, then what's past it, in file order. They read nothing
// else, never with Streaming, and clear the sequence when done.
TEST(RecordFileReaderTest, remoteOpenDeclaresWhatItReads) {
  // File name, and whether the open reads past the first 4 MB
  const vector<pair<string, bool>> files = {
      {"sample_file.vrs", false}, // old format, index at the end
      {"HollywoodRecording10bits.vrs", true}, // old format, index at the end
      {"hfs_purposes_recording.vrs", false}, // index at the end
      {"segundo.p.config.100ms.vrs", true}, // index at the end
      {"arcata_raw10.vrs", false}, // index at the front
  };
  // Most of these files are not in the open source test data.
  for (const auto& file : files) {
    if (!os::isFile(getTestFilePath(file.first))) {
      GTEST_SKIP() << "Missing test file " << file.first;
    }
  }
  for (const auto& [name, extended] : files) {
    for (CachingStrategy strategy : {CachingStrategy::Passive, CachingStrategy::Streaming}) {
      SCOPED_TRACE(name + " " + toString(strategy));
      OpenedFile opened = openFile(name, /* remote */ true, strategy);
      ASSERT_EQ(opened.status, 0);
      EXPECT_TRUE(opened.reader->hasIndex());
      EXPECT_EQ(opened.reader->getCachingStrategy(), strategy);
      const auto& declarations = opened.file->declarations;
      ASSERT_GE(declarations.size(), 2);
      EXPECT_EQ(
          declarations.front(),
          make_pair(Sequence{{0, min<size_t>(kOpenPrefetchSize, opened.size)}}, true));
      EXPECT_EQ(declarations.back(), make_pair(Sequence{}, true));
      ASSERT_EQ(declarations.size(), extended ? 3 : 2);
      if (extended) {
        EXPECT_FALSE(declarations[1].second);
        size_t previousEnd = kOpenPrefetchSize;
        for (const auto& range : declarations[1].first) {
          EXPECT_GE(range.first, previousEnd);
          previousEnd = range.first + range.second;
        }
        EXPECT_LE(previousEnd, opened.size);
      }
      for (const auto& read : opened.file->reads) {
        EXPECT_EQ(read.strategy, CachingStrategy::Passive);
        EXPECT_TRUE(opened.file->isDeclared(read.offset, read.length))
            << "Undeclared read of " << read.length << " bytes at " << read.offset;
      }
    }
  }
}

TEST(RecordFileReaderTest, failedRemoteOpenClearsTheSequence) {
  FileFormat::FileHeader header;
  DiskFile disk;
  ASSERT_EQ(disk.open(getTestFilePath("sample_file.vrs")), 0);
  ASSERT_EQ(disk.read(header), 0);
  OpenedFile opened = openFile(
      "sample_file.vrs",
      /* remote */ true,
      CachingStrategy::Passive,
      header.indexRecordOffset);
  EXPECT_EQ(opened.status, INDEX_RECORD_ERROR);
  ASSERT_GE(opened.file->declarations.size(), 2);
  EXPECT_EQ(opened.file->declarations.back(), make_pair(Sequence{}, true));
}
