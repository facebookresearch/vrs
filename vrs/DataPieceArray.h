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

#pragma once

#ifndef DATA_PIECES_ARRAY_H
#define DATA_PIECES_ARRAY_H

#ifndef DATA_PIECES_H
#include <vrs/DataPieces.h>
#endif

namespace vrs {

using std::map;
using std::min;
using std::ostream;
using std::string;
using std::vector;

/// \brief Fixed size array of POD values.
///
/// Array of type T and fixed size. The array is stored in DataLayout's fixed size buffer.
/// The size of the array is defined at construction and may not change.
template <typename T>
class DataPieceArray : public DataPiece {
  static_assert(std::is_trivially_copyable<T>::value, "DataPieceArray only supports POD types.");

 public:
  /// @param label: Name for the DataPiece.
  /// @param count: Number of elements in the fixes size-array.
  DataPieceArray(string label, size_t count)
      : DataPiece(std::move(label), DataPieceType::Array, sizeof(T) * count), count_(count) {}
  /// @param label: Name for the DataPiece.
  /// @param count: Number of elements in the fixes size-array.
  /// @param defaultValues: Static array of default values.
  template <size_t n>
  DataPieceArray(string label, size_t count, const T (&defaultValues)[n])
      : DataPiece(std::move(label), DataPieceType::Array, sizeof(T) * count), count_(count) {
    setDefault(defaultValues, n);
  }
  /// @param bundle: Bundle to reconstruct a DataPieceArray from disk.
  /// @internal
  explicit DataPieceArray(const MakerBundle& bundle);

  /// Get the size of the array.
  size_t getArraySize() const {
    return count_;
  }

  /// Get the name of the element type <T>.
  /// @internal
  const string& getElementTypeName() const override {
    return vrs::getTypeName<T>();
  }
  /// Get variable-size. 0 here, since this is a fixed size DataPiece. [doesn't apply].
  /// @internal
  size_t getVariableSize() const override {
    return 0;
  }
  /// Copy staged variable-size data to a specific location. [doesn't apply].
  /// @internal
  size_t collectVariableData(int8_t*, size_t) override {
    return 0;
  }

  /// copy a given number of values to a given location.
  /// If not enough values are available, default values are written.
  /// If not enough default values are available, T's default constructor is used to fill-in.
  /// @param outValues: Pointer where to write the data.
  /// @param count: Number of values requested.
  /// @return True if the requested number of actual data values were copied,
  /// even if there were more values available than requested.
  /// Return false in all other cases.
  bool get(T* outValues, size_t count) const {
    const size_t bytesRequested = sizeof(T) * count;
    const T* const ptr =
        (count <= count_) ? layout_.getFixedData<T>(offset_, bytesRequested) : nullptr;
    if (ptr != nullptr && bytesRequested > 0) {
      unalignedCopy(outValues, ptr, bytesRequested);
      return true;
    }
    const size_t defaultReadCount = min<size_t>(count, defaultValues_.size());
    if (defaultReadCount > 0) {
      unalignedCopy(outValues, defaultValues_.data(), defaultReadCount * sizeof(T));
    }
    // blank values set set by default values
    for (size_t k = defaultValues_.size(); k < count; k++) {
      outValues[k] = T{};
    }
    return false;
  }
  /// get specific value in the array, by index.
  /// @param outValue: Reference to a value to set.
  /// @param index: Value requested.
  /// @return True if an actual data value was read.
  /// False, if a default value was used, or T's default constructor.
  bool get(T& outValue, size_t index) const {
    const size_t bytesRequested = sizeof(T) * (index + 1);
    const T* const ptr =
        (index < count_) ? layout_.getFixedData<T>(offset_, bytesRequested) : nullptr;
    if (ptr != nullptr) {
      outValue = readUnaligned<T>(ptr + index);
      return true;
    }
    if (index < defaultValues_.size()) {
      outValue = defaultValues_[index];
    } else {
      outValue = T{};
    }
    return false;
  }
  /// Get values or default values.
  /// @param outValues: Vector to set.
  /// @return True if actual data values were set, false if default values were read.
  bool get(vector<T>& outValues) const {
    const T* const ptr = layout_.getFixedData<T>(offset_, getFixedSize());
    if (ptr != nullptr) {
      outValues.resize(count_);
      unalignedCopy(outValues.data(), ptr, getFixedSize());
      return true;
    }
    outValues = defaultValues_;
    return false;
  }
  /// Set array values.
  /// @param values: pointer to values to write.
  /// @param count: Number of values to write.
  /// @return True if values were written.
  /// Note: if more values are given than the size of the array, fewer values will be written, but
  /// the method still returns true.
  /// If not enough values are given to set the whole array, T's default constructor is used to init
  /// the remaining values.
  bool set(const T* values, size_t count) {
    T* const ptr = layout_.getFixedData<T>(offset_, getFixedSize());
    if (ptr != nullptr) {
      if (count > 0) {
        unalignedCopy(ptr, values, sizeof(T) * min<size_t>(count, count_));
      }
      T v{};
      while (count < count_) {
        writeUnaligned<T>(ptr + count++, v);
      }
      return true;
    } else {
      return false;
    }
  }
  /// Set one value of the array.
  /// @param value: Reference to the value to write.
  /// @param index: Index of the array element to set.
  /// @return True if the array was large enough, and the value could be written.
  /// Note: does not touch the other array values.
  bool set(const T& value, size_t index) {
    T* const ptr = layout_.getFixedData<T>(offset_, getFixedSize());
    if (ptr != nullptr && index < count_) {
      writeUnaligned<T>(ptr + index, value);
      return true;
    }
    return false;
  }
  /// Set array values.
  /// @param arr: C-style array.
  /// @return True if values were written.
  /// Note: if more values are given than the size of the array, fewer values will be written, but
  /// the method still returns true.
  /// If not enough values are given to set the whole array, T's default constructor is used to init
  /// the remaining values.
  template <size_t n>
  bool set(const T (&arr)[n]) {
    return set(arr, n);
  }
  /// Set array values.
  /// @param values: Vector of values to write.
  /// @return True if values were written.
  /// Note: if more values are given than the size of the array, fewer values will be written, but
  /// the method still returns true.
  /// If not enough values are given to set the whole array, T's default constructor is used to init
  /// the remaining values.
  bool set(const vector<T>& values) {
    return values.empty() ? set(nullptr, 0) : set(values.data(), values.size());
  }

  /// Specify a default value returned by get() when the DataPiece is not mapped.
  /// This value is not automatically used as an initialization value for the DataPiece.
  /// Use initToDefault() or DataLayout::initDataPiecesToDefaultValue() for that.
  /// @param defaultValues: Pointer to the first default value.
  /// @param count: Number of default values to use.
  /// Note: if fewer default values are passed that the size of the array,
  /// T's default constructor is used to have the exact count of default values.
  void setDefault(const T* defaultValues, size_t count) {
    defaultValues_.resize(count_);
    size_t copySize = min(count, count_) * sizeof(T);
    if (copySize > 0) {
      unalignedCopy(defaultValues_.data(), defaultValues, copySize);
    }
    for (; count < count_; count++) {
      defaultValues_[count] = T{};
    }
  }
  /// Specify a default value returned by get() when the DataPiece is not mapped.
  /// This value is not automatically used as an initialization value for the DataPiece.
  /// Use initToDefault() or DataLayout::initDataPiecesToDefaultValue() for that.
  /// @param arr: C-style array ot default values.
  /// Note: if fewer default values are passed that the size of the array,
  /// T's default constructor is used to have the exact count of default values.
  template <size_t n>
  void setDefault(const T (&arr)[n]) {
    setDefault(arr, n);
  }
  /// Specify a default value returned by get() when the DataPiece is not mapped.
  /// This value is not automatically used as an initialization value for the DataPiece.
  /// Use initToDefault() or DataLayout::initDataPiecesToDefaultValue() for that.
  /// @param defaultValues: Vector of default values.
  /// Note: if fewer default values are passed that the size of the array,
  /// T's default constructor is used to have the exact count of default values.
  void setDefault(const vector<T>& values) {
    setDefault(values.data(), values.size());
  }
  /// Get the default value returned by get() when the DataPiece is not mapped.
  /// @return Default values. The vector is empty if no default values were set.
  const vector<T>& getDefault() const {
    return defaultValues_;
  }
  /// Initialize to default value.
  void initToDefault() override {
    set(defaultValues_);
  }

  /// Set a property.
  /// @param propertyName: Name of the property.
  /// @param value: Value of the property.
  void setProperty(const string& propertyName, T value) {
    properties_[propertyName] = value;
  }
  /// Get a property
  /// @param propertyName: Name of the property.
  /// @param outValue: Reference to a value to set.
  /// @return True if the property exists and outValue was set, false otherwise.
  bool getProperty(const string& propertyName, T& outValue) const {
    auto iter = properties_.find(propertyName);
    if (iter != properties_.end()) {
      outValue = iter->second;
      return true;
    }
    return false;
  }

  /// Set the minimum valid value for each element of the array.
  /// @param min: Minimum valid value.
  /// Note: min checking is a sanity check operation only.
  /// Nothing prevents users of the API to set the values of the array any way they want.
  void setMin(T min) {
    properties_[kMinValue] = min;
  }
  /// Set the maximum valid value for each element of the array.
  /// @param max: Maximum valid value.
  /// Note: max checking is a sanity check operation only.
  /// Nothing prevents users of the API to set the values of the array any way they want.
  void setMax(T max) {
    properties_[kMaxValue] = max;
  }
  /// Set the min & max valid values for each element of the array.
  /// @param min: Minimum valid value.
  /// @param max: Maximum valid value.
  /// Note: min/max checking is a sanity check operation only.
  /// Nothing prevents users of the API to set the values of the array any way they want.
  void setRange(T min, T max) {
    properties_[kMinValue] = min;
    properties_[kMaxValue] = max;
  }
  /// Get minimum value for each element of the array.
  /// @param outMin: Reference to set to the minimum valid value.
  /// @return True if there is minimum value & outMin was set.
  bool getMin(T& outMin) const {
    return getProperty(kMinValue, outMin);
  }
  /// Get maximum value for each element of the array.
  /// @param outMax: Reference to set to the maximum valid value.
  /// @return True if there is maximum value & outMax was set.
  bool getMax(T& outMax) const {
    return getProperty(kMinValue, outMax);
  }

  /// Patch values in the mapped DataLayout.
  /// This method is named patchValue, because it's meant to edit a DataLayout found in a file,
  /// when doing a filter-copy operation.
  /// @return True if the piece is mapped and the values were set.
  bool patchValue(const T* values, size_t count) const {
    auto* patchedPiece = layout_.getMappedPiece<DataPieceArray<T>>(pieceIndex_);
    return patchedPiece != nullptr && patchedPiece->set(values, count);
  }

  /// Tell if a DataPiece value is available.
  /// @return True if the value is available, false if the DataPiece could not be mapped.
  bool isAvailable() const override {
    return layout_.getFixedData<T>(offset_, getFixedSize()) != nullptr;
  }

  /// Print the DataPiece to the out stream, with many details,
  /// using indent text at the start of each line of output.
  /// @param out: Output stream to print to.
  /// @param indent: Text to insert at the beginning of each output line, for indentation purposes.
  void print(ostream& out, const string& indent) const override;
  /// Print the DataPiece to the out stream in compact form,
  /// using indent text at the start of each line of output.
  /// @param out: Output stream to print to.
  /// @param indent: Text to insert at the beginning of each output line, for indentation purposes.
  void printCompact(ostream& out, const string& indent) const override;

  /// Compare two DataPiece objects for their equivalence.
  /// Note: the values are **not** compared, all the other properties are (type, name, tags, etc).
  /// @param rhs: Other DataPiece to compare to.
  /// @return True if the DataPiece objects are considered the same.
  bool isSame(const DataPiece* rhs) const override;

  /// Export the DataPiece as json, using a specific profile.
  /// @param jsonWrapper: Wrapper around a json type (to isolate any 3rd party library dependency).
  /// @param profile: Profile describing what information needs to be exported as json.
  void serialize(JsonWrapper& jsonWrapper, const JsonFormatProfileSpec& profile) override;

  /// Clone data piece.
  /// @return A clone of the data piece, with the same label, same type and same size.
  /// All the other data piece properties (default value and properties) are not cloned.
  unique_ptr<DataPiece> clone() const override {
    auto other = std::make_unique<DataPieceArray<T>>(getLabel(), getArraySize());
    other->tags_ = tags_;
    other->required_ = required_;
    other->properties_ = properties_;
    other->defaultValues_ = defaultValues_;
    return other;
  }

 protected:
  bool copyFrom(const DataPiece* original) override {
    const auto* source = reinterpret_cast<const DataPieceArray<T>*>(original);
    vector<T> values;
    bool retrieved = source->get(values);
    set(values);
    return retrieved;
  }

 private:
  const size_t count_{};
  map<string, T> properties_;
  vector<T> defaultValues_;
};

} // namespace vrs

#endif // DATA_PIECES_ARRAY_H
