// Facebook Technologies, LLC Proprietary and Confidential.

#pragma once

#include <cstdint>
#include <cstdio>
#include <vector>

namespace vrs {

using std::vector;

class FileHandler;

/// Container of data container references, to tell VRS where to write data when reading a record.
/// It's essentially a wrapper for two pairs of pointers to a buffer & the size of that buffer,
/// which may both be null.
///
/// Note: this class used to be used both for reading & writing record, but the record creation side
/// has now been entirely replaced with DataSource, which adds support for DataLayout.
///
/// Examples:
///
/// Reference a single pointer/length:
///
///    uint8_t buffer[kBufferLength];
///    DataReference dataReference(buffer, kBufferLength);
///
///
/// Reference the content of a POD struct:
///
///    struct {
///      int value;
///      double otherValue;
///    } someStruct;
///    DataReference dataReference(someStruct);
///
///
/// Reference the content of a struct and a pointer+length:
///
///    uint8_t buffer[kBufferLength];
///    struct {
///      int value;
///      double otherValue;
///    } someStruct;
///    DataReference dataReference(someStruct, buffer, kBufferLength);
class DataReference {
 public:
  /// @param data1: Pointer to first block of bytes.
  /// @param size1: Size of first block of bytes.
  /// @param data2: Pointer to second block of bytes.
  /// @param size2: Size of second block of bytes.
  DataReference(
      void* data1 = nullptr,
      uint32_t size1 = 0,
      void* data2 = nullptr,
      uint32_t size2 = 0)
      : data1_(data1), size1_(size1), data2_(data2), size2_(size2) {}

  /// @param vectorT: Vector of POD objects of type T to read.
  /// @param data: Pointer to second block of bytes.
  /// @param size: Size of second block of bytes.
  template <class T>
  DataReference(vector<T>& vectorT, void* data = nullptr, uint32_t size = 0)
      : DataReference(
            vectorT.data(),
            static_cast<uint32_t>(sizeof(T) * vectorT.size()),
            data,
            size) {}

  /// @param vectorT: Vector of POD objects of type T to read.
  /// @param vectorU: Vector of POD objects of type U to read.
  template <class T, class U>
  DataReference(vector<T>& vectorT, vector<U>& vectorU)
      : DataReference(
            vectorT.data(),
            static_cast<uint32_t>(sizeof(T) * vectorT.size()),
            vectorU.data(),
            static_cast<uint32_t>(sizeof(U) * vectorU.size())) {}

  /// @param object: POD object to read.
  /// @param vectorU: Vector of POD objects of type U to read.
  template <class T, class U>
  DataReference(T& object, vector<U>& vectorU)
      : DataReference(
            &object,
            sizeof(T),
            vectorU.data(),
            static_cast<uint32_t>(sizeof(U) * vectorU.size())) {}

  /// @param object: POD object to read.
  /// @param data: Pointer to second block of bytes.
  /// @param size: Size of second block of bytes.
  template <class T>
  DataReference(T& object, void* data = nullptr, uint32_t size = 0)
      : DataReference(&object, sizeof(T), data, size) {}

  /// @param data1: Pointer to first block of bytes.
  /// @param size1: Size of first block of bytes.
  /// @param data2: Pointer to second block of bytes.
  /// @param size2: Size of second block of bytes.
  void useRawData(void* data1, uint32_t size1, void* data2 = nullptr, uint32_t size2 = 0);

  /// @param vectorT: Vector of POD objects of type T to read.
  /// @param data: Pointer to second block of bytes.
  /// @param size: Size of second block of bytes.
  template <class T>
  void useVector(vector<T>& vectorT, void* data = nullptr, uint32_t size = 0) {
    data1_ = vectorT.data();
    size1_ = static_cast<uint32_t>(sizeof(T) * vectorT.size());
    data2_ = data;
    size2_ = size;
  }

  /// @param object: POD object to read.
  /// @param data: Pointer to second block of bytes.
  /// @param size: Size of second block of bytes.
  template <class T>
  void useObject(T& object, void* data = nullptr, uint32_t size = 0) {
    data1_ = &object;
    size1_ = sizeof(T);
    data2_ = data;
    size2_ = size;
  }

  /// @param vectorT: Vector of POD objects of type T to read.
  /// @param vectorU: Vector of POD objects of type U to read.
  template <class T, class U>
  void useVectors(vector<T>& vectorT, vector<U>& vectorU) {
    data1_ = vectorT.data();
    size1_ = static_cast<uint32_t>(sizeof(T) * vectorT.size());
    data2_ = vectorU.data();
    size2_ = static_cast<uint32_t>(sizeof(U) * vectorU.size());
  }

  /// @param object1: First POD object to read.
  /// @param object2: Second POD object to read.
  template <class T, class U>
  void useObjects(T& object1, U& object2) {
    data1_ = &object1;
    size1_ = sizeof(T);
    data2_ = &object2;
    size2_ = sizeof(U);
  }

  /// @return Number of bytes referenced, in total.
  uint32_t getSize() const {
    return size1_ + size2_;
  }

  /// Copy referenced data to a specific location, in one stream of bytes.
  /// The destination buffer must be large enough for the entire data.
  /// @param destination: Pointer where to write everything.
  void copyTo(void* destination) const;

  /// Fill the referenced data from a file (uncompressed).
  /// @param file: Open file to read from.
  /// @param outReadSize: Number of bytes actually read.
  /// Might be less than the size of the DataReference, if an error occurred.
  /// @return 0 if no error happened, and enough data was read.
  /// Otherwise, set to a file system error.
  int readFrom(FileHandler& file, uint32_t& outReadSize);

  /// Get pointer to first chunk of data. Might be nullptr.
  /// @return Pointer to first chunk of data.
  void* getDataPtr1() const {
    return data1_;
  }
  /// Get size in bytes of the fist chunk of data. Might be 0.
  /// @return Number of bytes.
  uint32_t getDataSize1() const {
    return size1_;
  }
  /// Get pointer to second chunk of data. Might be nullptr.
  /// @return Pointer to second chunk of data.
  void* getDataPtr2() const {
    return data2_;
  }
  /// Get size in bytes of the second chunk of data. Might be 0.
  /// @return Number of bytes.
  uint32_t getDataSize2() const {
    return size2_;
  }

 private:
  void* data1_;
  uint32_t size1_;
  void* data2_;
  uint32_t size2_;
};

} // namespace vrs
