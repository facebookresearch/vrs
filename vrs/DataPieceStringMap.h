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

#ifndef DATA_PIECES_STRING_MAP_H
#define DATA_PIECES_STRING_MAP_H

#include <cstring>

#ifndef DATA_PIECES_H
#include "DataPieces.h"
#endif

namespace vrs {

using std::ostream;
using std::string;
using std::vector;

/// \brief DataPiece map container, with string keys and values of type T.
///
/// Read values are extracted from the DataLayout's buffer, (VRS records reading/decoding).
/// Staged values are stored in the map<string, T> member of this class (VRS record creation).
template <typename T>
class DataPieceStringMap : public DataPiece {
 public:
  /// @param label: Name for the DataPiece.
  explicit DataPieceStringMap(const string& label)
      : DataPiece(label, DataPieceType::StringMap, DataLayout::kVariableSize) {}
  /// @param bundle: Bundle to reconstruct a DataPieceStringMap from disk.
  /// @internal
  explicit DataPieceStringMap(const MakerBundle& bundle);

  /// Get the name of the element type <T>.
  /// @internal
  const string& getElementTypeName() const override {
    return vrs::getTypeName<T>();
  }
  /// Get the size of the staged map, if any.
  /// @return Number of bytes to store the staged map.
  size_t getVariableSize() const override;
  /// Copy the staged map to the location given.
  /// @param data: Pointer where to write the staged values.
  /// @param bufferSize: Max number of bytes to write.
  /// @return Number of bytes written.
  size_t collectVariableData(int8_t* data, size_t bufferSize) const override;

  /// Read-only access to the map of values you wish to write to disk.
  /// @return A const reference to the staged map<string, T>.
  const map<string, T>& stagedValues() const {
    return stagedValues_;
  }
  /// Set or modify the map of values you wish to write to disk. Does not modify the read values!
  /// @return A reference to the staged map<string, T>.
  map<string, T>& stagedValues() {
    return stagedValues_;
  }
  /// Stage values you wish to write to disk. Does not modify the read values!
  /// @param values: Values to stage, to include them in the next record created.
  void stage(const map<string, T>& values) {
    stagedValues_ = values;
  }
  /// Stage values you wish to write to disk. Does not modify the read values!
  /// @param values: Values to stage, to include them in the next record created.
  void stage(map<string, T>&& values) {
    stagedValues_ = std::move(values);
  }

  /// Get read values or default values in a map.
  /// @param outValues: Reference to a map<string, T> for the read or default values.
  /// @return True if outValues was set from read values (maybe mapped), not default values.
  bool get(map<string, T>& outValues) const;

  /// Get the default value.
  /// @return Default values. The vector might be empty if there are no defaults set.
  const map<string, T>& getDefault() const {
    return defaultValues_;
  }
  /// Set default values using a vector.
  /// @param values: Vector of values to use a default.
  void setDefault(const map<string, T>& values) {
    defaultValues_ = values;
  }
  /// Set default values using a vector.
  /// @param values: Vector of values to use a default.
  void setDefault(map<string, T>&& values) {
    defaultValues_ = std::move(values);
  }

  /// Tell if the DataPiece is available, directly or mapped successfully.
  /// @return True if values can be read without using default values.
  bool isAvailable() const override {
    size_t count = 0;
    return layout_.getVarData<int8_t>(offset_, count) != nullptr;
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
    auto other = std::make_unique<DataPieceStringMap<T>>(getLabel());
    other->tags_ = tags_;
    other->required_ = required_;
    other->defaultValues_ = defaultValues_;
    return other;
  }

 protected:
  bool stageFrom(const DataPiece* original) override {
    const DataPieceStringMap<T>* origMap = reinterpret_cast<const DataPieceStringMap<T>*>(original);
    return origMap->get(stagedValues_);
  }

 private:
  map<string, T> stagedValues_; // values to write to disk
  map<string, T> defaultValues_;
};

} // namespace vrs

#endif // DATA_PIECES_STRING_MAP_H
