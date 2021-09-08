// Facebook Technologies, LLC Proprietary and Confidential.

#pragma once

#ifndef DATA_PIECES_VALUE_H
#define DATA_PIECES_VALUE_H

#include <memory>

#ifndef DATA_PIECES_H
#include "DataPieces.h"
#endif

namespace vrs {

using std::map;
using std::ostream;
using std::string;
using std::unique_ptr;

/// DataPiece for a single value of type T. The value is stored in DataLayout's fixed size buffer.
template <typename T>
class DataPieceValue : public DataPiece {
 public:
  /// @param label: Name for the DataPiece.
  DataPieceValue(const string& label) : DataPiece(label, DataPieceType::Value, sizeof(T)) {}
  /// @param label: Name for the DataPiece.
  /// @param defaultValue: Default value for the DataPiece.
  DataPieceValue(const string& label, T defaultValue)
      : DataPiece(label, DataPieceType::Value, sizeof(T)) {
    defaultValue_ = std::make_unique<T>(defaultValue);
  }
  /// @param bundle: Bundle to reconstruct a DataPieceValue from disk.
  /// @internal
  DataPieceValue(const MakerBundle& bundle);

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
  size_t collectVariableData(int8_t*, size_t) const override {
    return 0;
  }

  /// @return Value or mapped value, or default value if any, or default value for type T.
  T get() const {
    const T* const ptr = layout_.getFixedData<T>(offset_, sizeof(T));
    return ptr != nullptr ? readUnaligned<T>(ptr) : getDefault();
  }
  /// Get value or mapped value, or default value if any, or default value for type T.
  /// @param outValue: Reference to a variable to set.
  /// @return True if outValue was set to the value or the mapped value.
  bool get(T& outValue) const {
    const T* const ptr = layout_.getFixedData<T>(offset_, sizeof(T));
    if (ptr != nullptr) {
      outValue = readUnaligned<T>(ptr);
      return true;
    } else {
      getDefault(outValue);
      return false;
    }
  }

  /// Set value in fixed size buffer.
  /// @param value: Value to set the DataPiece to.
  /// @return False if the DataLayout is mapped, but this DataPiece is not mapped.
  bool set(const T& value) {
    T* const ptr = layout_.getFixedData<T>(offset_, sizeof(T));
    if (ptr != nullptr) {
      writeUnaligned<T>(ptr, value);
      return true;
    } else {
      return false;
    }
  }

  /// Get default value.
  /// @return Default value, or default value for type T.
  T getDefault() const {
    return defaultValue_ ? *defaultValue_.get() : T{};
  }
  /// Get default value.
  /// @param reference: Reference to a default value to set.
  /// @return True if the value was set to default value.
  bool getDefault(T& outDefault) const {
    if (defaultValue_) {
      outDefault = *defaultValue_.get();
      return true;
    }
    return false;
  }

  /// Set default value.
  /// @param defaultValue: Value to use as default.
  void setDefault(const T& defaultValue) {
    if (defaultValue_) {
      *defaultValue_.get() = defaultValue;
    } else {
      defaultValue_ = std::make_unique<T>(defaultValue);
    }
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
  /// Set a property.
  /// @param propertyName: Name of the property.
  /// @param value: Value of the property.
  void setProperty(const string& propertyName, T value) {
    properties_[propertyName] = value;
  }

  /// Get minimum value.
  /// @param outMin: Reference to set to the minimum valid value.
  /// @return True if there is minimum value & outMin was set.
  bool getMin(T& outMin) const {
    return getProperty(kMinValue, outMin);
  }
  /// Get maximum value.
  /// @param outMax: Reference to set to the maximum valid value.
  /// @return True if there is maximum value & outMax was set.
  bool getMax(T& outMax) const {
    return getProperty(kMaxValue, outMax);
  }
  /// Get minimum increment for this value, between two successive records.
  /// @param outMinIncrement: Reference to set to the minimum incremenet value.
  /// @return True if there is minimum increment value & outMinIncrement was set.
  bool getMinIncrement(T& outMinIncrement) const {
    return getProperty(kMinIncrement, outMinIncrement);
  }
  /// Get maximum increment for this value, between two successive records.
  /// @param outMaxIncrement: Reference to set to the maximum incremenet value.
  /// @return True if there is maximum increment value & outMaxIncrement was set.
  bool getMaxIncrement(T& outMaxIncrement) const {
    return getProperty(kMaxIncrement, outMaxIncrement);
  }
  /// Set the minimum valid value.
  /// @param min: Minimum valid value.
  /// Note: min checking is a sanity check operation only.
  /// Nothing prevents users of the API to set the values of the array any way they want.
  void setMin(T min) {
    properties_[kMinValue] = min;
  }
  /// Set the maximum valid value.
  /// @param max: Maximum valid value.
  /// Note: max checking is a sanity check operation only.
  /// Nothing prevents users of the API to set the values of the array any way they want.
  void setMax(T max) {
    properties_[kMaxValue] = max;
  }
  /// Set the min & max valid values.
  /// @param min: Minimum valid value.
  /// @param max: Maximum valid value.
  /// Note: min/max checking is a sanity check operation only.
  /// Nothing prevents users of the API to set the values of the array any way they want.
  void setRange(T min, T max) {
    properties_[kMinValue] = min;
    properties_[kMaxValue] = max;
  }
  /// Set the minimum increment value, between two successive records.
  /// @param minIncrement: Minimum increment value.
  /// Note: min increment checking is a sanity check operation only.
  /// This won't prevents users of the API to set the values any way they want.
  void setMinIncrement(T minIncrement) {
    properties_[kMinIncrement] = minIncrement;
  }
  /// Set the maximum increment value, between two successive records.
  /// @param maxIncrement: Maximum increment value.
  /// Note: max increment checking is a sanity check operation only.
  /// This won't prevents users of the API to set the values any way they want.
  void setMaxIncrement(T maxIncrement) {
    properties_[kMaxIncrement] = maxIncrement;
  }
  /// Set the min & max increment values, between two successive records.
  /// @param minIncrement: Minimum increment value.
  /// @param maxIncrement: Maximum increment value.
  /// Note: min & max increment checking is a sanity check operation only.
  /// This won't prevents users of the API to set the values any way they want.
  void setIncrement(T minIncrement, T maxIncrement) {
    properties_[kMinIncrement] = minIncrement;
    properties_[kMaxIncrement] = maxIncrement;
  }

  /// Tell if a DataPiece value is available.
  /// @return True if the value is available, false if the DataPiece could not be mapped.
  bool isAvailable() const override {
    return layout_.getFixedData<T>(offset_, sizeof(T)) != nullptr;
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
  /// @return A clone of the data piece, with the same label and same type.
  /// All the other data piece properties (default value and properties) are not cloned.
  unique_ptr<DataPiece> clone() const override {
    auto other = std::make_unique<DataPieceValue<T>>(getLabel());
    other->tags_ = tags_;
    other->required_ = required_;
    other->properties_ = properties_;
    if (defaultValue_) {
      other->defaultValue_ = std::make_unique<T>(*defaultValue_);
    }
    return other;
  }

 protected:
  bool stageFrom(const DataPiece* /*original*/) override {
    return false; // not applicable
  }

 private:
  map<string, T> properties_;
  unique_ptr<T> defaultValue_;
};

/// Helper class to store enums more easily.
/// This DataPiece type is very light layer on top of DataPieceValue.
/// DataPieceEnum provides automatic casting between an enum type and an explicit POD type.
/// DataPieceEnum is safer to use, because it gives explicit control over the numeric type used to
/// store the enum in the file format.
///
/// ATTENTION! STORING ENUM VALUES IS RISKY!
/// - It's very easy to change an enum definition and break compatibility with existing files.
///   Be certain the enum's numeric values aren't changed, or shifted when the enum is modified,
///   maybe by adding static asserts verifying enum numeric values are never changed.
///   In particular, avoid storing enums which definition is maintained by other teams.
/// - The underlying type is DataPieceValue<StorageType>, therefore labels must not collide.
template <typename EnumType, typename StorageType>
class DataPieceEnum : public DataPieceValue<StorageType> {
 public:
  /// @param label: Name for the DataPiece.
  DataPieceEnum(const string& label) : DataPieceValue<StorageType>(label) {}

  /// @param label: Name for the DataPiece.
  /// @param defaultValue: Default value for the DataPiece.
  DataPieceEnum(const string& label, EnumType defaultValue)
      : DataPieceValue<StorageType>(label, static_cast<StorageType>(defaultValue)) {}

  /// Get the value in enum form.
  /// If the value isn't available, returns the default value.
  /// @return The value as an enum.
  EnumType get() const {
    return static_cast<EnumType>(DataPieceValue<StorageType>::get());
  }

  /// Get the value in enum form, with test for availability.
  /// @param e: A reference to the enum to get.
  /// @return True if the value was set.
  /// If the value wasn't available, returns false.
  bool get(EnumType& e) const {
    StorageType v;
    bool r = DataPieceValue<StorageType>::get(v);
    e = static_cast<EnumType>(v);
    return r;
  }

  /// Set value in enum form.
  /// @param value: Value to set the DataPiece to.
  /// @return False if the DataLayout is mapped, but this DataPiece is not mapped.
  bool set(const EnumType e) {
    return DataPieceValue<StorageType>::set(static_cast<StorageType>(e));
  }

  /// Get default value.
  /// @return Default value if one was specified, or default value for the enum.
  EnumType getDefault() const {
    return static_cast<EnumType>(DataPieceValue<StorageType>::getDefault());
  }

  /// Get default value.
  /// @param reference: Reference to the variable to set.
  /// @return True if the value was set to an explicitly specified default value.
  bool getDefault(EnumType& outDefault) const {
    StorageType v;
    bool r = DataPieceValue<StorageType>::getDefault(v);
    outDefault = static_cast<EnumType>(v);
    return r;
  }

  /// Explicitly specify a default value.
  /// @param defaultValue: Value to use as default.
  void setDefault(const EnumType defaultValue) {
    DataPieceValue<StorageType>::setDefault(static_cast<StorageType>(defaultValue));
  }
};

} // namespace vrs

#endif // DATA_PIECES_VALUE_H
