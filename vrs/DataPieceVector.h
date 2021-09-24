// (c) Facebook, Inc. and its affiliates. Confidential and proprietary.

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

/// Vector of type T and variable size.
/// Read values are extracted from the DataLayout's buffer, (VRS records reading/decoding).
/// Staged values are stored in the vector<T> member of this class (VRS record creation).
template <typename T>
class DataPieceVector : public DataPiece {
 public:
  /// @param label: Name for the DataPiece.
  DataPieceVector(const string& label)
      : DataPiece(label, DataPieceType::Vector, DataLayout::kVariableSize) {}
  /// @param bundle: Bundle to reconstruct a DataPieceVector from disk.
  /// @internal
  DataPieceVector(const MakerBundle& bundle);

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
  size_t collectVariableData(int8_t* data, size_t bufferSize) const override {
    const size_t writenSize = min(bufferSize, getVariableSize());
    if (writenSize > 0) {
      memcpy(data, stagedValues_.data(), writenSize);
    }
    return writenSize;
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
      memcpy(stagedValues_.data(), values, count * sizeof(T));
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
    size_t count;
    const T* ptr = layout_.getVarData<T>(offset_, count);
    if (ptr != nullptr && count > 0) {
      outValues.resize(count);
      memcpy(outValues.data(), ptr, count * sizeof(T));
      return true;
    }
    outValues = defaultValues_;
    return false;
  }

  /// Get the default value.
  /// @return Default values. The vector might be empty if there are no defaults set.
  const vector<T>& getDefault() const {
    return defaultValues_;
  }
  /// Set the default values.
  /// @param defaultValues: Pointer to the default values.
  /// @param count: Number of default values.
  void setDefault(const T* defaultValues, size_t count) {
    defaultValues_.resize(count);
    if (count > 0) {
      memcpy(defaultValues_.data(), defaultValues, count * sizeof(T));
    }
  }
  /// Set the default values using a C-style fixed size array.
  /// @param arr: C-style fixed size array of default values.
  template <size_t n>
  void setDefault(const T (&arr)[n]) {
    setDefault(arr, n);
  }
  /// Set default values using a vector.
  /// @param values: Vector of values to use a default.
  void setDefault(const vector<T>& values) {
    defaultValues_ = values;
  }
  /// Set default values using a vector.
  /// @param values: Vector of values to use a default.
  void setDefault(vector<T>&& values) {
    defaultValues_ = std::move(values);
  }

  /// Tell if the DataPiece is available, directly or mapped successfully.
  /// @return True if values can be read without using default values.
  bool isAvailable() const override {
    size_t count;
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
  bool stageFrom(const DataPiece* original) override {
    const DataPieceVector<T>* originalVect = reinterpret_cast<const DataPieceVector<T>*>(original);
    return originalVect->get(stagedValues_);
  }

 private:
  vector<T> stagedValues_; // values to write to disk
  vector<T> defaultValues_;
};

// Specializations declaration, required here for gcc Android.
template <>
void DataPieceVector<string>::stage(const string* values, size_t count);
template <>
size_t DataPieceVector<string>::getVariableSize() const;
template <>
size_t DataPieceVector<string>::collectVariableData(int8_t* data, size_t bufferSize) const;
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
