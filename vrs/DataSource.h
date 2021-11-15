// Copyright (c) Facebook, Inc. and its affiliates. Confidential and proprietary.

#pragma once

#include <cstddef>
#include <cstdint>

#include <vector>

namespace vrs {

using std::enable_if;
using std::is_base_of;
using std::is_pointer;
using std::vector;

class DataLayout;

/// \brief Class referencing a DataLayout object, and abstracting the interactions for DataSource.
///
/// Only the constructor is meant to be used by code outside of DataSource.
/// The default constructor holds no data.
/// Do not change the underlying DataLayout until the DataLayoutChunk is destroyed. This is usually
/// a trivial requirement, as DataSource objects (which hold DataLayoutChunk objects) only live
/// briefly for the purpose of calling createRecord().
struct DataLayoutChunk {
  /// Default constructor to no DataLayout.
  DataLayoutChunk() {}
  /// Constructor to reference a DataLayout, which must outlive this DataLayoutChunk.
  explicit DataLayoutChunk(DataLayout& dataLayout);

  /// Get the data size required to hold all of the DataLayot's data.
  size_t size() const {
    return layoutFixedSize_ + layoutVariableSize_;
  }
  /// Copy the data of the DataLayout (if any), and move the given pointer accordingly.
  void fillAndAdvanceBuffer(uint8_t*& buffer) const;

  DataLayout* const dataLayout_{};
  const size_t layoutFixedSize_{};
  const size_t layoutVariableSize_{};
};

/// \brief Elementary part of a DataSource for a simple block of memory.
///
/// Class possibly referencing a buffer of bytes, specified in 4 possible different ways:
/// - a raw pointer + size.
/// - a trivially copyable object of type T.
/// - a vector<T> of trivially copyable objects of type T.
/// - an empty buffer (default constructor).
///
/// This class is meant to simplify the manipulation of a buffer of bytes, for use by DataSource.
/// Only the constructors and assignment operators are meant to be used by code outside of
/// DataSource.
class DataSourceChunk {
 public:
  /// Not copyable as there are derived classes that aren't safe to copy from the base class.
  DataSourceChunk(const DataSourceChunk& other) = delete;
  const DataSourceChunk& operator=(const DataSourceChunk& other) = delete;

  /// Empty DataSourceChunk.
  DataSourceChunk() : data_{}, size_{} {}
  /// Constructor for a raw pointer + size.
  DataSourceChunk(const void* _data, size_t _size);
  /// Constructor for a vector<T> of objects of POD type T.
  template <typename T>
  /* implicit */ DataSourceChunk(const vector<T>& vectorT)
      : data_{vectorT.data()}, size_{sizeof(T) * vectorT.size()} {}
  virtual ~DataSourceChunk() {}

  /// Constructor for a trivially copyable type T.
  template <
      typename T,
      // T must be trivially copyable.
      std::enable_if_t<std::is_trivially_copyable<T>::value, int> = 0,
      // T may not be a pointer.
      typename = typename enable_if<!is_pointer<T>::value, T>::type>
  /* implicit */ DataSourceChunk(const T& object) : data_{&object}, size_{sizeof(T)} {}

  /// Copy the data (if any), and update the provided buffer pointer accordingly.
  /// The number of bytes copied *must be the exact size* specified in the constructor.
  virtual void fillAndAdvanceBuffer(uint8_t*& buffer) const;

  const void* data() const {
    return data_;
  }

  /// For performance, do not make this method virtual.
  size_t size() const {
    return size_;
  }

 private:
  const void* const data_;
  const size_t size_;
};

/// \brief Class to represent a data chunk composed of multiple smaller
/// chunks that each have fixed offsets from each other in memory.
class NonContiguousChunk final : public vrs::DataSourceChunk {
 public:
  /// @param data: start of the buffer.
  /// @param blockSize: number of useful bytes in each block of data.
  /// @param numBlocks: number of blocks.
  /// @param strideInBytes: number of bytes between the first byte of consecutive blocks.
  /// (strideInBytes - blockSize) is the number of bytes to skip between blocks, therefore
  /// strideInBytes should be greater than blockSize.
  NonContiguousChunk(
      const void* data,
      std::size_t blockSize,
      std::size_t numBlocks,
      std::size_t strideInBytes);

  void fillAndAdvanceBuffer(std::uint8_t*& buffer) const final;

 private:
  const std::size_t blockSize_;
  const std::size_t numBlocks_;
  const std::size_t strideInBytes_;
};

/// \brief A class referencing data to be captured in a record at creation.
///
/// VRS records data content are passed to VRS using a single DataSource object.
///
/// DataSource objects point to data to be copied in a record's buffer when createRecord() is called
/// to perform a deep copy. This reduces the number of data copy required, as each DataLayoutChunk
/// and DataSourceChunk object may point to internal data buffers, possibly owned by device drivers.
///
/// DataSource's default implementation holds 2 DataLayoutChunk objects, and 3 DataSourceChunk
/// objects, in that order, which covers standard use cases, as each only optionaly contains data.
///
/// For more advanced needs, you can override copyTo() and copy data at the provided location the
/// way you like, but you must provide the exact amount of data upfront in the constructor.
/// Alternatively, you can use custom DataSourceChunk objects that override fillAndAdvanceBuffer().
/// DataSource objects are expected to be temporary object created on the stack each time
/// createdRecord() is called.
class DataSource {
 public:
  /// No DataLayout, no DataSourceChunk
  /// @param size: Cached number of bytes of the data source.
  explicit DataSource(size_t size = 0) : size_{size} {}
  virtual ~DataSource() = default;
  /// Constructor for a single DataSourceChunk
  explicit DataSource(const DataSourceChunk& chunk) : chunk1_{chunk}, size_{chunk.size()} {}
  /// Constructor for two DataSourceChunk objects
  explicit DataSource(const DataSourceChunk& firstChunk, const DataSourceChunk& secondChunk)
      : chunk1_{firstChunk}, chunk2_{secondChunk}, size_{firstChunk.size() + secondChunk.size()} {}
  /// Constructor for three DataSourceChunk objects
  explicit DataSource(
      const DataSourceChunk& firstChunk,
      const DataSourceChunk& secondChunk,
      const DataSourceChunk& thirdChunk)
      : chunk1_{firstChunk}, chunk2_{secondChunk}, chunk3_{thirdChunk}, size_{getChunksSize()} {}
  /// Constructor for a single DataLayout, no DataSourceChunk
  explicit DataSource(DataLayout& dataLayout)
      : dataLayout1_{dataLayout}, size_{dataLayout1_.size()} {}
  /// Constructor for single DataLayout, single DataSourceChunk
  explicit DataSource(DataLayout& dataLayout, const DataSourceChunk& chunk)
      : dataLayout1_{dataLayout}, chunk1_{chunk}, size_{dataLayout1_.size() + chunk1_.size()} {}
  /// Constructor for single DataLayout, two DataSourceChunk objects
  explicit DataSource(
      DataLayout& dataLayout,
      const DataSourceChunk& firstChunk,
      const DataSourceChunk& secondChunk);
  /// Constructor for single DataLayout, three DataSourceChunk objects
  explicit DataSource(
      DataLayout& dataLayout,
      const DataSourceChunk& firstChunk,
      const DataSourceChunk& secondChunk,
      const DataSourceChunk& thirdChunk);
  /// Constructor for two DataLayout objects, no DataSourceChunk
  explicit DataSource(DataLayout& dataLayout1, DataLayout& dataLayout2);
  /// Constructor for two DataLayout objects, single DataSourceChunk.
  explicit DataSource(
      DataLayout& dataLayout1,
      DataLayout& dataLayout2,
      const DataSourceChunk& chunk);
  /// Two DataLayout objects, two DataSourceChunk objects.
  explicit DataSource(
      DataLayout& dataLayout1,
      DataLayout& dataLayout2,
      const DataSourceChunk& firstChunk,
      const DataSourceChunk& secondChunk);
  /// Two DataLayout objects, three DataSourceChunk objects.
  explicit DataSource(
      DataLayout& dataLayout1,
      DataLayout& dataLayout2,
      const DataSourceChunk& firstChunk,
      const DataSourceChunk& secondChunk,
      const DataSourceChunk& thirdChunk);

  DataSource(const DataSource&) = delete;
  DataSource& operator=(const DataSource&) = delete;

  /// Get the total amount of data to copy, calculated once & cached at construction.
  /// Might not be the same as the sum of the chunks sizes, when this class is derived.
  /// @return The size requested.
  size_t getDataSize() const {
    return size_;
  }

  /// Copy all the source data to a buffer guaranteed to be at least getDataSize() large.
  /// The total amount of data copied is expected to be exactly getDataSize().
  /// @param buffer: Pointer to a buffer of bytes to copy the data to.
  virtual void copyTo(uint8_t* buffer) const;

 protected:
  static DataSourceChunk kEmptyDataSourceChunk;

  /// Get the overall size of all the parts combined, DataLayout & DataSourceChunks.
  /// @return The combined size of all the member chunks.
  size_t getChunksSize();

  const DataLayoutChunk dataLayout1_;
  const DataLayoutChunk dataLayout2_;
  const DataSourceChunk& chunk1_{kEmptyDataSourceChunk};
  const DataSourceChunk& chunk2_{kEmptyDataSourceChunk};
  const DataSourceChunk& chunk3_{kEmptyDataSourceChunk};
  const size_t size_;
};

} // namespace vrs
