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

#include <vrs/Compressor.h>
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

TEST(IndexRecord, ReadRecord_MultiFrameCompressedSplitIndex_ReadsAllRecords) {
  // The writer emits one zstd frame per 100,000 index entries, matching the reader's batch size.
  constexpr size_t kBatchSize = 100'000;
  constexpr size_t kFrameCount = 40;
  // Only each frame's first zstd block (128 KiB) is hard to compress, so most of the compressed
  // bytes are in first blocks, and buffered reads are likely to end in the middle of one.
  constexpr size_t kRandomCount = 128 * 1024 / sizeof(IndexRecord::DiskRecordInfo);
  constexpr size_t kRecordCount = kBatchSize * kFrameCount;
  FileFormat::FileHeader fileHeader;
  fileHeader.init();
  const uint32_t recordSize = fileHeader.recordHeaderSize;
  const StreamId streamId{RecordableTypeId::UnitTestRecordableClass, 1};
  vector<uint8_t> compressedIndex;
  vector<IndexRecord::DiskRecordInfo> batch(kBatchSize);
  uint64_t seed = 42;
  for (size_t frame = 0; frame < kFrameCount; ++frame) {
    for (size_t k = 0; k < kBatchSize; ++k) {
      seed = seed * 6364136223846793005ULL + 1442695040888963407ULL;
      const double timestamp = k < kRandomCount ? static_cast<double>(seed >> 11) : 0;
      batch[k] = IndexRecord::DiskRecordInfo(timestamp, recordSize, streamId, Record::Type::DATA);
    }
    Compressor compressor;
    uint32_t compressedSize = compressor.compress(
        batch.data(), batch.size() * sizeof(batch[0]), CompressionPreset::ZstdLight);
    ASSERT_GT(compressedSize, 0);
    const uint8_t* data = static_cast<const uint8_t*>(compressor.getData());
    compressedIndex.insert(compressedIndex.end(), data, data + compressedSize);
  }
  constexpr size_t kIndexOffset = 1;
  const size_t firstUserRecordOffset =
      kIndexOffset + sizeof(FileFormat::RecordHeader) + compressedIndex.size();
  vector<uint8_t> fileData(firstUserRecordOffset + kRecordCount * recordSize);
  fileHeader.indexRecordOffset = kIndexOffset;
  fileHeader.firstUserRecordOffset = static_cast<int64_t>(firstUserRecordOffset);
  FileFormat::RecordHeader indexHeader;
  indexHeader.initIndexHeader(
      IndexRecord::kSplitIndexFormatVersion,
      static_cast<uint32_t>(compressedIndex.size()),
      0,
      CompressionType::Zstd);
  indexHeader.uncompressedSize =
      static_cast<uint32_t>(kRecordCount * sizeof(IndexRecord::DiskRecordInfo));
  memcpy(fileData.data() + kIndexOffset, &indexHeader, sizeof(indexHeader));
  memcpy(
      fileData.data() + kIndexOffset + sizeof(indexHeader),
      compressedIndex.data(),
      compressedIndex.size());
  utils::BufferFileHandler file(fileData);
  set<StreamId> streamIds;
  vector<IndexRecord::RecordInfo> index;
  IndexRecord::Reader reader(file, fileHeader, nullptr, streamIds, index);
  int64_t usedFileSize = 0;
  EXPECT_EQ(0, reader.readRecord(fileHeader.firstUserRecordOffset, usedFileSize));
  EXPECT_TRUE(reader.isIndexComplete());
  EXPECT_EQ(kRecordCount, index.size());
}
