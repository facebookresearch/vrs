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

#include <cstring>
#include <limits>

#include <gtest/gtest.h>

#include <vrs/ErrorCode.h>
#include <vrs/FileFormat.h>
#include <vrs/IndexRecord.h>
#include <vrs/utils/BufferRecordReader.hpp>

using namespace std;
using namespace vrs;

TEST(IndexRecord, ReadRecord_OversizedCompressedSplitIndex_ReturnsErrorWithoutIndex) {
  constexpr size_t kIndexOffset = 1;
  constexpr uint32_t kCompressedPayloadSize = 64 * 1024;
  vector<uint8_t> fileData(
      kIndexOffset + sizeof(FileFormat::RecordHeader) + kCompressedPayloadSize);
  FileFormat::FileHeader fileHeader;
  fileHeader.init();
  fileHeader.indexRecordOffset = kIndexOffset;
  fileHeader.firstUserRecordOffset = static_cast<int64_t>(fileData.size());
  FileFormat::RecordHeader indexHeader;
  indexHeader.initIndexHeader(
      IndexRecord::kSplitIndexFormatVersion, kCompressedPayloadSize, 0, CompressionType::Zstd);
  indexHeader.uncompressedSize = numeric_limits<uint32_t>::max();
  memcpy(fileData.data() + kIndexOffset, &indexHeader, sizeof(indexHeader));
  utils::BufferFileHandler file(fileData);
  set<StreamId> streamIds;
  vector<IndexRecord::RecordInfo> index;
  IndexRecord::Reader reader(file, fileHeader, nullptr, streamIds, index);
  int64_t usedFileSize = 0;
  EXPECT_NE(0, reader.readRecord(fileHeader.firstUserRecordOffset, usedFileSize));
  EXPECT_FALSE(reader.isIndexComplete());
  EXPECT_TRUE(index.empty());
  EXPECT_EQ(0, index.capacity());
}
