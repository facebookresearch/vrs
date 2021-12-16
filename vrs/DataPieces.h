// Copyright (c) Meta Platforms, Inc. and affiliates.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#pragma once

#ifndef DATA_PIECES_H
#define DATA_PIECES_H

#include <cstring>

#include <algorithm>
#include <map>
#include <ostream>

#include "DataLayout.h"

namespace vrs {

using std::map;
using std::ostream;
using std::string;

/// Forward declaration of a mystery type to avoid exposing a third party json library.
struct JsonWrapper;

/// \brief Abstract class representing a piece of information part of a DataLayout.
///
/// DataPiece objets have a type (DataPieceType) and label (a text name),
/// which together are enough to identify uniquely a DataPiece of a particular DataLayout.
/// See DataLayout for more details.
class DataPiece {
 public:
  /// \brief Forward declaration, so we can pass RapidJson definitions in headers,
  /// without having to include the actual RapidJson headers everywhere.
  struct MakerBundle;

 protected:
  /// Constructor, called by derived types exclusively.
  /// @param label: Text name.
  /// @param type: A DataPieceType.
  /// @param size: Size in bytes of the DataPiece, or the constant: DataLayout::kVariableSize.
  DataPiece(const string& label, DataPieceType type, size_t size);

  /// Special tag name to specify a unit of the DataPiece.
  static const string kUnit;
  /// Special tag name to specify a human readable description the DataPiece.
  static const string kDescription;

  /// Special property name for the minimum value of the DataPiece.
  static const string kMinValue;
  /// Special property name for the maximum value of the DataPiece.
  static const string kMaxValue;
  /// Special property name for the minimum increment of the DataPiece.
  static const string kMinIncrement;
  /// Special property name for the maximum increment of the DataPiece.
  static const string kMaxIncrement;

 public:
  virtual ~DataPiece();
  /// Get the DataLayout owning this DataPiece.
  /// @return The DataLayout.
  const DataLayout& getDataLayout() const {
    return layout_;
  }
  /// Get the text label of this DataPiece.
  /// @return The label.
  const string& getLabel() const {
    return label_;
  }
  /// Get the type of the DataPiece.
  /// @return The DataPieceType.
  DataPieceType getPieceType() const {
    return pieceType_;
  }

  /// Get the type name of the DataPiece.
  /// @return PieceTypeName<DataType> e.g. "value<double>"
  string getTypeName() const;

  /// Get the offset of the DataPiece in the DataLayout.
  /// For fixed-size DataPiece objects, it's the memory index in the fixed size data buffer.
  /// For variable-size DataPiece objects, it's the index of the variable DataPiece.
  /// For both: if the DataLayout was mapped to another, but this particular DataPiece could not
  /// be mapped in the target DataLayout, the offset value is set to DataLayout::kNotFound.
  /// @return Offset value, or DataLayout::kNotFound if the DataPiece was not mapped.
  /// @internal
  size_t getOffset() const {
    return offset_;
  }
  /// Tell if the DataPiece has a fixed-size.
  /// @return True if the DataPiece needs the same memory space, no matter what the value is.
  bool hasFixedSize() const {
    return fixedSize_ != DataLayout::kVariableSize;
  }
  /// Get the number of bytes needed to store the DataPiece's value.
  /// @return A number of bytes, or DataLayout::kVariableSize.
  size_t getFixedSize() const {
    return fixedSize_;
  }
  /// Get a DataPiece tag.
  /// @param tagName: Name of the tag to retrieve.
  /// @param outTag: Reference to a string to set.
  /// @return True, if the tag was found, and outTag was set. False, otherwise.
  bool getTag(const string& tagName, string& outTag) const;
  /// Set a DataPiece tag.
  /// @param tagName: Name of the tag to set.
  /// @param tag: Value of the tag.
  void setTag(const string& tagName, const string& tag) {
    tags_[tagName] = tag;
  }
  /// Get the unit of a DataPiece.
  /// @param outUnit: Reference to a string to set.
  /// @return True, if the unit tag was found, and outUnit was set. False, otherwise.
  bool getUnit(string& outUnit) const {
    return getTag(kUnit, outUnit);
  }
  /// Set the unit of a DataPiece.
  /// @param unit: Unit of the DataPiece (preferably, a SI unit like "m", or "ms").
  void setUnit(const string& unit) {
    tags_[kUnit] = unit;
  }

  /// Get the decription of a DataPiece.
  /// @param outDescription: Reference to a string to set.
  /// @return True, if the unit tag was found, and outDescription was set. False, otherwise.
  bool getDescription(string& outDescription) const {
    return getTag(kDescription, outDescription);
  }
  /// Set the decription of a DataPiece.
  /// @param decription: Human readable description of the DataPiece.
  void setDescription(const string& decription) {
    tags_[kDescription] = decription;
  }

  /// Specify if the DataPiece is required when mapping to another DataLayout.
  /// @param required: True if mapping the DataPiece successfully is required.
  void setRequired(bool required = true) {
    required_ = required;
  }
  /// Tell if the DataPiece requires to be mapped for the mapping
  /// of the DataLayout to be considered successful.
  /// @return True if the DataPiece must be mapped.
  bool isRequired() const {
    return required_;
  }

  /// Get the size of variable-size DataPiece staged value.
  /// @return 0 for fixed-size DataPieces, otherwise, the number of bytes required to store the
  /// variable-size staged value.
  virtual size_t getVariableSize() const = 0;
  /// Copy the staged variable-size to a location.
  /// @param data: Pointer where to copy the variable-size data.
  /// @param bufferSize: Max number of bytes to copy.
  /// @return Number of bytes actually copied.
  virtual size_t collectVariableData(int8_t* data, size_t bufferSize) const = 0;
  /// Get the name of the template element type T of the derived template class,
  /// or "string" in the case of DataPieceString.
  /// @return The DataPiece's element type name.
  virtual const string& getElementTypeName() const = 0;
  /// Tell if the DataPiece is available.
  /// @return True if the DataLayout was not mapped, or if a matching DataPiece was found,
  /// False if the DataLayout was mapped and no matching DataPiece was found.
  virtual bool isAvailable() const = 0;
  /// Print the DataPiece to the out stream, with many details,
  /// using indent text at the start of each line of output.
  /// @param out: Output stream to print to.
  /// @param indent: Text to insert at the beginning of each output line, for indentation purposes.
  virtual void print(ostream& out, const string& indent = "") const = 0;
  /// Print the DataPiece to the out stream in compact form,
  /// using indent text at the start of each line of output.
  /// @param out: Output stream to print to.
  /// @param indent: Text to insert at the beginning of each output line, for indentation purposes.
  virtual void printCompact(ostream& out, const string& indent = "") const = 0;
  /// Export the DataPiece as json, using a specific profile.
  /// @param jsonWrapper: Wrapper around a json type (to isolate any 3rd party library dependency).
  /// @param profile: Profile describing what information needs to be exported as json.
  virtual void serialize(JsonWrapper& jsonWrapper, const JsonFormatProfileSpec& profile);
  /// Take the current value of the field, and stage it for writing during record creation.
  /// This operation does nothing for fixed size fields, which value & stage data are the same.
  /// For variable size fields, the value is read from the DataLayout's variable size data buffer,
  /// and staged as the value to write out.
  /// @return True if the value is available and was staged.
  virtual bool stageCurrentValue() {
    return isAvailable();
  }
  /// Create a new DataPiece of the same type, with the same label.
  virtual unique_ptr<DataPiece> clone() const = 0;

 protected:
  /// Match signature only.
  bool isMatch(const DataPiece& rhs) const;
  /// Match signature & properties (default value, min/max, etc).
  virtual bool isSame(const DataPiece* rhs) const;
  void setOffset(size_t offset) {
    offset_ = offset;
  }
  /// Stage value from another piece known to be of the same type.
  virtual bool stageFrom(const DataPiece* original) = 0;

  const string label_;
  const DataPieceType pieceType_;
  const size_t fixedSize_;
  size_t offset_;
  DataLayout& layout_;
  map<string, string> tags_;
  bool required_;

  friend class DataLayout;
};

/// Get the name of the type <T>.
template <class T>
const string& getTypeName();

template <>
inline const string& getTypeName<string>() {
  static const string sName("string");
  return sName;
};

/// \brief Template to represent some POD object without memory alignment.
///
/// Data read from disk might not be aligned as the architecture would like it,
/// which can be extremely slow and even crash on some architectures (ARM7).
/// These helper methods make the code safe & readable.
#pragma pack(push, 1) // tells the compiler that the data might not be aligned
template <class T>
struct UnalignedValue {
  T value;
};
#pragma pack(pop)

/// Helper to make dereferencing a pointer to read an unaligned POD object safe.
template <class T>
T readUnaligned(const void* ptr) {
  return reinterpret_cast<const UnalignedValue<T>*>(ptr)->value;
}

/// Helper to make dereferencing a pointer to write an unaligned POD object safe.
template <class T>
void writeUnaligned(void* ptr, const T& value) {
  reinterpret_cast<UnalignedValue<T>*>(ptr)->value = value;
}

} // namespace vrs

// These are required pretty much every single time we use DataLayout. Just include them all.
#include "DataPieceTypes.h"

#ifndef DATA_PIECES_ARRAY_H
#include "DataPieceArray.h"
#endif

#ifndef DATA_PIECES_STRING_H
#include "DataPieceString.h"
#endif

#ifndef DATA_PIECES_STRING_MAP_H
#include "DataPieceStringMap.h"
#endif

#ifndef DATA_PIECES_VALUE_H
#include "DataPieceValue.h"
#endif

#ifndef DATA_PIECES_VECTOR_H
#include "DataPieceVector.h"
#endif

#endif // DATA_PIECES_H
