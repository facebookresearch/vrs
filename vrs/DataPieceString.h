// Copyright (c) Facebook, Inc. and its affiliates. Confidential and proprietary.

#pragma once

#ifndef DATA_PIECES_STRING_H
#define DATA_PIECES_STRING_H

#ifndef DATA_PIECES_H
#include "DataPieces.h"
#endif

namespace vrs {

using std::min;
using std::ostream;
using std::string;

/// For variable length string.
/// Written values are stored in the string member of this class (VRS record creation)
/// Read values are extracted from the DataLayout's buffer, (VRS records reading/decoding)
class DataPieceString : public DataPiece {
 public:
  /// @param label: Name for the DataPiece.
  DataPieceString(const string& label)
      : DataPiece(label, DataPieceType::String, DataLayout::kVariableSize) {}
  /// @param bundle: Bundle to reconstruct a DataPieceString from disk.
  /// @internal
  DataPieceString(const MakerBundle& bundle);

  /// Get the DataPiece element type name.
  /// @return "string".
  const string& getElementTypeName() const override;
  /// Get the size of the staged string, if any.
  /// @return Number of bytes to store the staged string (not including a trailing 0).
  size_t getVariableSize() const override {
    return stagedString_.size();
  }
  /// Copy the staged string to the location given.
  /// @param data: Pointer where to write the staged string's characters.
  /// @param bufferSize: Max number of bytes to write.
  /// @return Number of characters written.
  size_t collectVariableData(int8_t* data, size_t bufferSize) const override;

  /// Stage a string value.
  /// Note: does not motify the value returned by get().
  /// @param string: Value to stage.
  void stage(const string& string) {
    stagedString_ = string;
  }
  /// Stage a string value.
  /// Note: does not motify the value returned by get().
  /// @param string: Value to stage.
  void stage(string&& string) {
    stagedString_ = std::move(string);
  }
  /// Read-only access to the staged value.
  /// Note: the staged value is not the value read from disk.
  /// @return A const reference to the staged string value.
  const string& stagedValue() const {
    return stagedString_;
  }
  /// Get the staged value to write to disk, for read or write purposes.
  /// Note: the staged value is not the value read from disk.
  /// @return A reference to the staged string value.
  string& stagedValue() {
    return stagedString_;
  }

  /// Get value read from disk, or default value.
  /// @return The string value read from disk, or the default value.
  string get() const;
  /// Get value read from disk, or default value.
  /// @return True, if the value returned was read from the field or a mapped field.
  bool get(string& outString) const;

  /// Tell if a the DataPiece has a default value.
  bool hasDefault() const {
    return !defaultString_.empty();
  }
  /// Get the default string, if one was was set.
  const string& getDefault() const {
    return defaultString_;
  }
  /// Set the default string, in case the DataLayout is mapped, but this DataPiece was not mapped.
  void setDefault(const string& defaultString) {
    defaultString_ = defaultString;
  }
  /// Set the default string, in case the DataLayout is mapped, but this DataPiece was not mapped.
  void setDefault(string&& defaultString) {
    defaultString_ = std::move(defaultString);
  }

  /// Tell if a value is available, in this DataLayout or the mapped DataLayout.
  bool isAvailable() const override;

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
    return get(stagedValue());
  }

  /// Clone data piece.
  /// @return A clone of the data piece, with the same label and same type.
  /// All the other data piece properties (default and staged values) are not cloned.
  unique_ptr<DataPiece> clone() const override {
    auto other = std::make_unique<DataPieceString>(getLabel());
    other->tags_ = tags_;
    other->required_ = required_;
    other->defaultString_ = defaultString_;
    return other;
  }

 protected:
  bool stageFrom(const DataPiece* original) override {
    const DataPieceString* cloneString = reinterpret_cast<const DataPieceString*>(original);
    return cloneString->get(stagedString_);
  }

 private:
  string stagedString_;
  string defaultString_;
};

} // namespace vrs

#endif // DATA_PIECES_STRING_H
