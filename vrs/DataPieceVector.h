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

#ifndef DATA_PIECES_VECTOR_H
#define DATA_PIECES_VECTOR_H

#ifndef DATA_PIECES_H
#include "DataPieces.h"
#endif

namespace vrs {

using std::ostream;
using std::string;
using std::vector;

/// \brief Vector of type T and variable size.
///
/// Read values are extracted from the DataLayout's buffer, (VRS records reading/decoding).
/// Staged values are stored in the vector<T> member of this class (VRS record creation).
template <typename T>
class DataPieceVector : public DataPiece {
 public:
  /// @param label: Name for the DataPiece.
  explicit DataPieceVector(string label, vector<T> defaultValues = {})
      : DataPiece(std::move(label), DataPieceType::Vector, DataLayout::kVariableSize),
        defaultValues_{std::move(defaultValues)} {}
  /// @param bundle: Bundle to reconstruct a DataPieceVector from disk.
  /// @internal
  explicit DataPieceVector(const MakerBundle& bundle);

  /// Get the name of the element type <T>.
  /// @internal
  const string& getElementTypeName() const override {
    return vrs::getTypeName<T>();
  }
  /// Get the size of the staged vector, if any.
  /// @return Number of bytes to store the staged vector.
  size_t getVariableSize() const override {
    return sizeof(T) * stagedValues_.size();
  }
  /// Copy the staged vector to the location given.
  /// @param data: Pointer where to write the staged values.
  /// @param bufferSize: Max number of bytes to write.
  /// @return Number of bytes written.
  size_t collectVariableData(int8_t* data, size_t bufferSize) override {
    const size_t writtenSize = min(bufferSize, getVariableSize());
    if (writtenSize > 0) {
      unalignedCopy(data, stagedValues_.data(), writtenSize);
    }
    return writtenSize;
  }

  /// Read-only access to the vector of values you wish to write to disk.
  /// @return A const reference to the staged vector<T>.
  const vector<T>& stagedValues() const {
    return stagedValues_;
  }
  /// Set or modify the vector of values you wish to write to disk. Does not modify the read values!
  /// @return A reference to the staged vector<T>.
  vector<T>& stagedValues() {
    return stagedValues_;
  }
  /// Stage values you wish to write to disk. Does not modify the read values!
  /// @param values: Values to stage, to include them in the next record created.
  void stage(const vector<T>& values) {
    stagedValues_ = values;
  }
  /// Stage values you wish to write to disk. Does not modify the read values!
  /// @param values: Values to stage, to include them in the next record created.
  void stage(vector<T>&& values) {
    stagedValues_ = std::move(values);
  }

  /// Stage an array values.
  /// @param arr: C-style array.
  /// @param count: number of values to stage
  void stage(const T* values, size_t count) {
    stagedValues_.resize(count);
    if (count > 0) {
      unalignedCopy(stagedValues_.data(), values, count * sizeof(T));
    }
  }
  /// Stage an array of values.
  /// @param arr: C-style array.
  template <size_t n>
  void stage(const T (&arr)[n]) {
    stage(arr, n);
  }

  /// Get read values or default values in a vector.
  /// @param outValues: Reference to a vector<T> for the read or default values.
  /// @return True if outValues was set from read values (maybe mapped), not default values.
  bool get(vector<T>& outValues) const {
    size_t count = 0;
    const T* ptr = layout_.getVarData<T>(offset_, count);
    if (ptr != nullptr && count > 0) {
      outValues.resize(count);
      unalignedCopy(outValues.data(), ptr, count * sizeof(T));
      return true;
    }
    if (pieceIndex_ != DataLayout::kNotFound) {
      outValues.clear();
      return true;
    }
    outValues = defaultValues_;
    return false;
  }

  /// Specify a default value returned by get() when the DataPiece is not mapped.
  /// This value is not automatically used as an initialization value for the DataPiece.
  /// Use initToDefault() or DataLayout::initDataPiecesToDefaultValue() for that.
  /// @param defaultValues: Pointer to the default values.
  /// @param count: Number of default values.
  void setDefault(const T* defaultValues, size_t count) {
    defaultValues_.resize(count);
    if (count > 0) {
      unalignedCopy(defaultValues_.data(), defaultValues, count * sizeof(T));
    }
  }
  /// Specify a default value returned by get() when the DataPiece is not mapped.
  /// This value is not automatically used as an initialization value for the DataPiece.
  /// Use initToDefault() or DataLayout::initDataPiecesToDefaultValue() for that.
  /// @param arr: C-style fixed size array of default values.
  template <size_t n>
  void setDefault(const T (&arr)[n]) {
    setDefault(arr, n);
  }
  /// Specify a default value returned by get() when the DataPiece is not mapped.
  /// This value is not automatically used as an initialization value for the DataPiece.
  /// Use initToDefault() or DataLayout::initDataPiecesToDefaultValue() for that.
  /// @param values: Vector of values to use a default.
  void setDefault(const vector<T>& values) {
    defaultValues_ = values;
  }
  /// Specify a default value returned by get() when the DataPiece is not mapped.
  /// This value is not automatically used as an initialization value for the DataPiece.
  /// Use initToDefault() or DataLayout::initDataPiecesToDefaultValue() for that.
  /// @param values: Vector of values to use a default.
  void setDefault(vector<T>&& values) {
    defaultValues_ = std::move(values);
  }
  /// Get the default value returned by get() when the DataPiece is not mapped.
  /// @return Default values. The vector might be empty if there are no defaults set.
  const vector<T>& getDefault() const {
    return defaultValues_;
  }
  /// Stage default value.
  void initToDefault() override {
    stagedValues_ = defaultValues_;
  }

  /// Patch values in the mapped DataLayout.
  /// This method is named patchValue, because it's meant to edit a DataLayout found in a file,
  /// when doing a filter-copy operation.
  /// @return True if the piece is mapped and the values were staged.
  bool patchValue(const vector<T>& values) const {
    auto* patchedPiece = layout_.getMappedPiece<DataPieceVector<T>>(pieceIndex_);
    return patchedPiece != nullptr && (patchedPiece->stage(values), true);
  }

  /// Tell if the DataPiece is available, directly or mapped successfully.
  /// @return True if values can be read without using default values.
  bool isAvailable() const override {
    size_t count = 0;
    return layout_.getVarData<T>(offset_, count) != nullptr;
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

  /// Take the current value of the field, and stage it for writing during record creation.
  /// @return True if the value is available and was staged.
  bool stageCurrentValue() override {
    return get(stagedValues());
  }

  /// Clone data piece.
  /// @return A clone of the data piece, with the same label and same type.
  /// All the other data piece properties (default and staged values) are not cloned.
  unique_ptr<DataPiece> clone() const override {
    auto other = std::make_unique<DataPieceVector<T>>(getLabel());
    other->tags_ = tags_;
    other->required_ = required_;
    other->defaultValues_ = defaultValues_;
    return other;
  }

 protected:
  bool copyFrom(const DataPiece* original) override {
    const DataPieceVector<T>* originalVect = reinterpret_cast<const DataPieceVector<T>*>(original);
    return originalVect->get(stagedValues_);
  }

 private:
  vector<T> stagedValues_; // values to write to disk
  vector<T> defaultValues_;
};

// Specializations declaration, required here for come compilers.
template <>
void DataPieceVector<string>::stage(const string* values, size_t count);
template <>
size_t DataPieceVector<string>::getVariableSize() const;
template <>
size_t DataPieceVector<string>::collectVariableData(int8_t* data, size_t bufferSize);
template <>
void DataPieceVector<string>::setDefault(const string* defaultValues, size_t count) = delete;
template <>
bool DataPieceVector<string>::get(vector<string>& outValues) const;
template <>
void DataPieceVector<string>::print(ostream& out, const string& indent) const;
template <>
void DataPieceVector<string>::printCompact(ostream& out, const string& indent) const;

} // namespace vrs

#endif // DATA_PIECES_VECTOR_H
