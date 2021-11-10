// Copyright (c) Facebook, Inc. and its affiliates. Confidential and proprietary.

#include "DataSource.h"

#include <cstring>

#include "DataLayout.h"

#include <logging/Checks.h>

namespace vrs {

DataLayoutChunk::DataLayoutChunk(DataLayout& dataLayout)
    : dataLayout_{&dataLayout},
      layoutFixedSize_{dataLayout.getFixedDataSizeNeeded()},
      layoutVariableSize_{dataLayout.getVarDataSizeNeeded()} {}

void DataLayoutChunk::fillAndAdvanceBuffer(uint8_t*& buffer) const {
  if (dataLayout_ != nullptr) {
    // Place the variable-size data after the fixed-size data in the buffer,
    // which updates the variable size index, so we must do this first!
    dataLayout_->collectVariableDataAndUpdateIndex(buffer + layoutFixedSize_);
    if (layoutFixedSize_ > 0) {
      // copy the fixed-size data + index after copying the variable size,
      // so that the variable-size data index is updated first!
      memcpy(buffer, dataLayout_->getFixedData().data(), layoutFixedSize_);
    }
    buffer += layoutFixedSize_ + layoutVariableSize_;
  }
}

void DataSourceChunk::fillAndAdvanceBuffer(uint8_t*& buffer) const {
  if (size_ > 0) {
    memcpy(buffer, data_, size_);
    buffer += size_;
  }
}

DataSourceChunk DataSource::kEmptyDataSourceChunk;

NonContiguousChunk::NonContiguousChunk(
    const void* data,
    std::size_t blockSize,
    std::size_t numBlocks,
    std::size_t strideInBytes)
    : vrs::DataSourceChunk{data, blockSize * numBlocks},
      blockSize_{blockSize},
      numBlocks_{numBlocks},
      strideInBytes_{strideInBytes} {}

void NonContiguousChunk::fillAndAdvanceBuffer(std::uint8_t*& buffer) const {
  const std::uint8_t* ptr = static_cast<const std::uint8_t*>(data());
  XR_DEV_CHECK_GT(blockSize_, 0UL); // otherwise the virtual method won't be called
  XR_DEV_CHECK_GT(numBlocks_, 0UL);
  for (std::size_t b = 0; b < numBlocks_; ++b) {
    std::memcpy(buffer, ptr, blockSize_);
    buffer += blockSize_;
    ptr += strideInBytes_;
  }
}

DataSource::DataSource(
    DataLayout& dataLayout,
    const DataSourceChunk& firstChunk,
    const DataSourceChunk& secondChunk)
    : dataLayout1_{dataLayout}, chunk1_{firstChunk}, chunk2_{secondChunk}, size_{getChunksSize()} {}

DataSource::DataSource(
    DataLayout& dataLayout,
    const DataSourceChunk& firstChunk,
    const DataSourceChunk& secondChunk,
    const DataSourceChunk& thirdChunk)
    : dataLayout1_{dataLayout},
      chunk1_{firstChunk},
      chunk2_{secondChunk},
      chunk3_(thirdChunk),
      size_{getChunksSize()} {}

DataSource::DataSource(DataLayout& dataLayout1, DataLayout& dataLayout2)
    : dataLayout1_{dataLayout1},
      dataLayout2_{dataLayout2},
      size_{dataLayout1_.size() + dataLayout2_.size()} {}

DataSource::DataSource(
    DataLayout& dataLayout1,
    DataLayout& dataLayout2,
    const DataSourceChunk& chunk)
    : dataLayout1_{dataLayout1},
      dataLayout2_{dataLayout2},
      chunk1_{chunk},
      size_{getChunksSize()} {}

DataSource::DataSource(
    DataLayout& dataLayout1,
    DataLayout& dataLayout2,
    const DataSourceChunk& firstChunk,
    const DataSourceChunk& secondChunk)
    : dataLayout1_{dataLayout1},
      dataLayout2_{dataLayout2},
      chunk1_{firstChunk},
      chunk2_{secondChunk},
      size_{getChunksSize()} {}

DataSource::DataSource(
    DataLayout& dataLayout1,
    DataLayout& dataLayout2,
    const DataSourceChunk& firstChunk,
    const DataSourceChunk& secondChunk,
    const DataSourceChunk& thirdChunk)
    : dataLayout1_{dataLayout1},
      dataLayout2_{dataLayout2},
      chunk1_{firstChunk},
      chunk2_{secondChunk},
      chunk3_(thirdChunk),
      size_{getChunksSize()} {}
DataSourceChunk::DataSourceChunk(const void* _data, size_t _size) : data_{_data}, size_{_size} {}

void DataSource::copyTo(uint8_t* buffer) const {
  dataLayout1_.fillAndAdvanceBuffer(buffer);
  dataLayout2_.fillAndAdvanceBuffer(buffer);
  if (chunk1_.size() > 0) {
    chunk1_.fillAndAdvanceBuffer(buffer);
  }
  if (chunk2_.size() > 0) {
    chunk2_.fillAndAdvanceBuffer(buffer);
  }
  if (chunk3_.size() > 0) {
    chunk3_.fillAndAdvanceBuffer(buffer);
  }
}

size_t DataSource::getChunksSize() {
  return dataLayout1_.size() + dataLayout2_.size() + chunk1_.size() + chunk2_.size() +
      chunk3_.size();
}

} // namespace vrs
