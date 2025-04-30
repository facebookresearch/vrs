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

#include <array>

#include <functional>
#include <memory>
#include <ostream>
#include <string>
#include <vector>

#include "ForwardDefinitions.h"

namespace vrs {

using std::ostream;
using std::string;
using std::unique_ptr;
using std::vector;

class DataPiece;
class AutoDataLayoutEnd;

template <typename T>
class DataPieceValue;
template <typename T>
class DataPieceArray;
template <typename T>
class DataPieceVector;
template <typename T>
class DataPieceStringMap;
class DataPieceString;

namespace internal {
class DataLayouter;
}

/// Forward declaration of a mystery type to avoid exposing a third party json library.
struct JsonWrapper;

/// Specifier for a type of DataPiece.
enum class DataPieceType : uint8_t {
  Undefined = 0, ///< Undefined type.
  Value = 1, ///< Single value.
  Array = 2, ///< Fixed size array.
  Vector = 3, ///< Variable size array of T.
  String = 4, ///< Variable size array of char, null terminated.
  StringMap = 5, ///< Map with string keys, and T values.

  COUNT ///< Count of enum values
};

/// Enum for a DataLayout printout json formatting profile
enum class JsonFormatProfile {
  VrsFormat, ///< for internal VRS usage. (default)
  ExternalCompact, ///< for external tools (VRStools in particular), but compact.
  ExternalPretty, ///< for external tools (VRStools in particular), formatted for readability.
  Public, ///< for public use cases, avoiding VRS internal names
};

/// \brief When printing out a DataLayout as json, this class allows to specify what should be
/// included in the generated json message.
///
/// The default constructor provides the profile needed for the description of DataLayout stored
/// in VRS files, to document a DataLayoutContentBlock. Therefore, this default profile should not
/// be changed, or DataLayout blocks might not be read correctly.
///
/// Default values are for VRS internal usage: DO NOT CHANGE THIS FORMAT IN ANY WAY!
struct JsonFormatProfileSpec {
  bool publicNames = false; ///< Use internal names, or public names. "data_layout" vs. "metadata"
  bool prettyJson = false; ///< Format the text so that it is easier to read.
  bool value = false; ///< Include the value of the data piece elements.
  bool name = true; ///< Include the label name.
  bool type = true; ///< Include the type name.
  bool shortType = false; ///< Use the short version of the type names.
  bool index = true; ///< Include the index of the data pieces.
  bool defaults = true; ///< Include default values.
  bool tags = true; ///< Include tags.
  bool properties = true; ///< Includes properties.
  bool required = true; ///< Include the required flag.

  // Default format
  JsonFormatProfileSpec() = default;
  explicit JsonFormatProfileSpec(JsonFormatProfile profile);
};

/// \brief The DataLayout class describes the data stored inside a DataLayoutContentBlock.
///
/// A DataLayout object is usually constructed using AutoDataLayout and AutoDataLayoutEnd helpers.
/// This method allows the easy & safe definition of a DataLayout in the form of a struct.
/// The member variables of an AutoDataLayout allow the easy access of the individual pieces of
/// content that make up a DataLayoutContentBlock, both for writing & reading.
///
/// Note that though DataLayout objects may look like structs, constructing them is relatively
/// expensive, and creating AutoDataLayout objects involve a synchronisation lock. Therefore, avoid
/// creating & destroying using short lived AutoDataLayout stack variables. Instead, prefer
/// allocating the DataLayout objects you will need frequently as member variables of other classes
/// that are long lived.
///
/// A DataLayout object can generate a json representation of itself, that is stored by VRS as an
/// internal tag of a VRS record stream. This json data will be used when reading the VRS file to
/// interpret the data of the corresponding DataLayoutContentBlock. The instantiation of a
/// DataLayout from a json message is meant for exclusive use by VRS itself, when reading a file.
/// VRS is very careful to only do this json expensive deserialization once per type.
///
/// The key feature of DataLayout is the separation of the layout description saved once per stream,
/// from the actual payload in each record, which can be minimal, containing only binary data.
/// The DataLayout sections of records are not stored in json format.
///
/// Most methods of this class are meant for exclusive use by VRS itself, even if they are public.
///
/// Anatomy of a DataLayout
/// =======================
///
/// A DataLayout is an ordered collection of individual pieces of data, all deriving from the
/// DataPiece abstract class. Classes deriving from DataPiece fall in two categories:
/// Fixed-size DataPiece objects, and variable-size DataPiece objects.
///
/// Fixed-size DataPiece objects, use the same number of bytes, no matter the actual value:
/// - single values of the supported POD types.
/// - fixed size arrays of the supported POD types.
///
/// DataLayout supports the following POD types:
/// - native C/C++ types, like int8, int16, int32, int64, signed or unsigned, float & double.
/// - Bool, which is a replacement for bool, because vector<bool> is not an STL container, see
///   https://stackoverflow.com/questions/17794569/why-is-vectorbool-not-a-stl-container
/// - 2, 3 & 4 D points, 3 & 4 D matrices, of types int32, float & double, for instance:
///   Point2Df: a 2 dimension point using float.
///   Matrix4Dd: a 4 dimension matrix using double.
///
/// DataPieceValue<T> and DataPieceArray<T> are the two fixed size DataPiece template classes.
///
/// Note: You can not use DataLayout template types with your own POD types, because:
///  1. DataLayout needs a factory able to instantiate them, even when your code isn't around,
///  2. it would defeat the purpose of handling format changes, as any change made to your
///  POD type would have to be versioned & managed by hand.
///
/// Variable-size DataPiece objects use a number of bytes that depends on their actual value:
/// - DataPieceVector<T>: a vector of any of the supported POD types, or std::string.
/// - DataPieceStringMap<T>: a string map of any of the supported POD types, or std::string.
/// - DataPieceString: an ASCII or utf8 text string.
///
/// Internal representation (what you don't need to know unless you are working on DataLayout)
///
/// Because there is a known fixed number of fixed-size DataPiece objects, each of known size,
/// we can pre-allocate a buffer to hold the data values of all those DataPiece objects.
/// Fixed-size DataPiece objects reference their data using an offset and the size of their data.
/// All read & write value operations on fixed-size DataPiece objects use that buffer, so that
/// we can directly read from and write to disk that part of a DataLayout's data.
/// That buffer is the first section of the fixedData_ byte vector.
///
/// For variable-size DataPiece objects, because the DataPiece data has an unforeseeable size,
/// data reads & writes are not handled the same as for fixed-size DataPiece objects.
/// Variable-size DataPiece objects don't have setter methods, but "stage" methods, that set the
/// value in a field of the DataPiece object itself. The "stage" methods also allows to read the
/// staged values. Once the value of all the variable-size DataPiece objects are staged, the staged
/// values can be collected in a buffer, the varData_ byte vector. To interpret a variable size
/// buffer read from disk, we need an index, to know where each piece starts, and their size.
/// That's the variable size index. The size of that index is known ahead of time, since there is
/// one index entry per variable-size DataPiece, and we know up-front how many we have.
/// The index is stored in the fixedData_ vector, after the data of fixed-size DataPiece values.
/// The values of the index are set when the variable size data is collected.
/// When reading the value of a variable-size DataPiece, the varData_ buffer is used, if present.
///
/// To write a DataLayout section, the 3 following operations are needed:
///  1. collect the variable-size DataPiece values. That operations allocates the varData_ buffer,
///    and updates the var data index in the fixedData_ buffer.
///  2. write the fixedData_ buffer.
///  3. write the varData_ buffer.
///
/// Therefore, to read & write the data of a DataLayout from & to disk, there is no encoding of the
/// data, unlike with json, which guaranties the bit accuracy of your data.
/// That can be critically important when using float & double numbers.
///
/// Of course, all of this is the responsibility of VRS itself.
class DataLayout {
 protected:
  DataLayout() = default;

 public:
  DataLayout& operator=(const DataLayout&) = delete;
  DataLayout(const DataLayout&) = delete;

  /// DataLayout has no virtual method, but it is used in containers, and some of its
  /// derived classes have important clean-up work to do in their destructor.
  /// Therefore, DataLayout requires a virtual destructor.
  virtual ~DataLayout();
  /// Special OffsetAndLength offset value marking that a piece of data isn't available.
  static const size_t kNotFound;
  /// Special value used for a DataPiece size, telling that that DataPiece has a variable size.
  static const size_t kVariableSize;

// Pack and use unit32_t because we're storing on disk, and size_t might be 32 or 64 bits
#pragma pack(push, 1)
  /// Describes where the data of a variable size DataPiece is in the varData_ buffer.
  class IndexEntry {
   public:
    void setOffset(size_t offset) {
      offset_ = static_cast<uint32_t>(offset);
    }
    size_t getOffset() const {
      return static_cast<uint32_t>(offset_);
    }
    void setLength(size_t length) {
      length_ = static_cast<uint32_t>(length);
    }
    size_t getLength() const {
      return static_cast<uint32_t>(length_);
    }

   private:
    uint32_t offset_{}; ///< byte offset in the varData_ buffer
    uint32_t length_{}; ///< number of bytes in the varData_ buffer
  };
#pragma pack(pop)

  /// @return ContentBlock object to build a RecordFormat definition.
  ContentBlock getContentBlock() const;

  /// To access the buffer for fixed-size DataPieces. No need for a const version.
  /// @internal
  vector<int8_t>& getFixedData() {
    return fixedData_;
  }
  /// To access the buffer for variable-size DataPieces. No need for a const version.
  /// @internal
  vector<int8_t>& getVarData() {
    return varData_;
  }
  /// Size required to fit all fixed size data.
  /// @internal
  size_t getFixedDataSizeNeeded() const {
    return fixedDataSizeNeeded_;
  }

  /// Set or stage all the DataPieces to their default value.
  /// Attention! DataPieces are NOT automatically set or staged to default values when a DataLayout
  /// instance is created! Default values primary usage is to provide a value when trying to get a
  /// value from an unmapped DataPiece object. However, with this method, you can also initialize
  /// all the DataPieces of a DataLayout explicitly.
  void initDataPiecesToDefaultValue();

  /// Retrieve size of the var size pieces *from the index*.
  ///
  /// Only really meaningful after variable-size pieces:
  ///  - have been collected, or
  ///  - the layout's data has been read from disk, or
  ///  - after the layout has been mapped onto another layout.
  /// @return Byte count.
  /// @internal
  size_t getVarDataSizeFromIndex() const;

  /// @return Size to fit the variable-size data written to its variable-size DataPieces fields.
  /// Won't match varData_'s size, unless collectVariableDataAndUpdateIndex() was called.
  /// @internal
  size_t getVarDataSizeNeeded() const;

  /// Collect all the variable-size fields in the varData_ buffer.
  /// @internal
  void collectVariableDataAndUpdateIndex();
  /// Collect variable-size data to the buffer specified.
  /// @param destination: Pointer to the buffer.
  /// The buffer must be at least getVarDataSizeNeeded() large.
  /// @internal
  void collectVariableDataAndUpdateIndex(void* destination);
  /// Get this layout's raw data. Assumes no data needs to be collected (nothing was "staged").
  /// In particular, can be used in RecordFormatStreamPlayer's callback to copy the layout verbatim.
  /// @internal
  void getRawData(vector<int8_t>& outRawData) const;
  /// Take the values of the variable size fields and stage them.
  /// This is basically the opposite of collectVariableDataAndUpdateIndex().
  /// This is useful if you have read a DataLayout from disk, and want to edit some fields to write
  /// an edited version of the DataLayout.
  /// @internal
  void stageCurrentValues();
  /// When a layout was cloned from another layout - this must be true -, this method will copy or
  /// stage all the data piece values from the original layout, so edits can be made and a new
  /// record generated with the DataLayout, using the values from originalLayout when possible.
  /// @param originalLayout: cloned datalayout from which DataPiece values need to be copied.
  /// @return True if the copy was successful, false if the layout isn't a clone of originalLayout.
  bool copyClonedDataPieceValues(const DataLayout& originalLayout);
  /// Copy (set or stage) the DataPiece values from a mapped layout, to this layout.
  /// The mapped layout must be a different instance of the same layout as this object (identical
  /// fields), but it may be mapped on any layout, and maybe have all, some, or no pieces mapped.
  /// By using a mapped layout hopefully provided by the getExpectedLayout() API and therefore be
  /// cached, we can make sure that repeat copy operations are as fast as possible, no matter what
  /// the underlying layout is.
  /// @param mappedLayout: layout from which DataPiece values are copied from. The type of
  /// mappedLayout must be identical with this layout, and it must be mapped on another layout,
  /// but that other layout can be anything. Every mapped data piece will be set or staged with the
  /// value accessed through mappedLayout. Unmapped data pieces are unmodified.
  /// Note that some safety checks will be done, to avoid incorrect use of the API.
  /// @return The number of DataPiece values copied.
  size_t copyDataPieceValuesFromMappedLayout(const DataLayout& mappedLayout);

  /// Map the data pieces of this layout to that of another layout, field by field.
  /// Note that this call is fairly expensive, and it is normally not necessary to call directly.
  /// In particular, in the callback RecordFormatStreamPlayer::onDataLayoutRead(), calling
  /// mapLayout() directly can usually be avoided by using the methods
  /// RecordFormatStreamPlayer::getExpectedLayout() and RecordFormatStreamPlayer::getLegacyLayout(),
  /// which will cache the mapping for you.
  /// @param targetLayout: Other DataLayout to map each DataPiece to.
  /// This is usually the layout that contains a record's actual data.
  /// @return True if all *required* DataPieces were found.
  /// When returning false, all possible matches are still made, and a false return value doesn't
  /// mean that the mapping "failed". It only means that some (or all) the DataPieces that have been
  /// explicitly marked as required have not been found in the target layout.
  /// This result value is cached and returned by hasAllRequiredPieces().
  /// Again, DataPieces are NOT marked required by default, so you must call
  /// DataPiece::setRequired(true) for each DataPiece you require, or call
  /// DataLayout::requireAllPieces() to require them all in one shot.
  /// A good place to do this is probably your DataLayout's constructor itself.
  bool mapLayout(DataLayout& targetLayout);
  /// @return True if the layout is mapped to another layout, in which case read values will come
  /// from the fields of the mapped layout.
  bool isMapped() const {
    return mappedDataLayout_ != nullptr;
  }
  /// @return True if the layout is mapped to another layout, and all the fields marked required
  /// have been successfully mapped onto a field of the target layout.
  /// Also returns True if the layout isn't mapped to another layout but has been initialized
  /// successfully by dataLayoutEnd(), initLayout(), and, for subclasses of AutoDataLayout, by
  /// the customary extra AutoDataLayoutEnd field added to the end of every AutoDataLayout.
  bool hasAllRequiredPieces() const {
    return mappedDataLayout_ == nullptr || hasAllRequiredPieces_;
  }
  /// Mark all the fields of the layout as required.
  void requireAllPieces();

  /// Print the fields of this DataLayout, showing all known details & values.
  /// If the layout is mapped, the values are that of the mapped fields.
  /// If a field of a mapped layout wasn't successfully mapped, no value is shown.
  /// @param out: Ostream to write to.
  /// @param indent: Prefix to each output line, for nested presentation.
  void printLayout(ostream& out, const string& indent = "") const;
  /// Print the values of the fields of this datalayout, in a compact form.
  /// @param out: Ostream to write to.
  /// @param indent: Prefix to each output line, for nested presentation.
  void printLayoutCompact(ostream& out, const string& indent = "") const;

  /// Generate json representation of this layout, using a profile telling which fields to include,
  /// and other presentation options.
  /// @param profile: Formatting profile.
  /// @return json text string.
  string asJson(JsonFormatProfile profile) const;
  /// Generate json representation of this layout, using a profile telling which fields to include,
  /// and other presentation options.
  /// @param profile: Formatting profile.
  /// @return json text string.
  string asJson(const JsonFormatProfileSpec& profile = JsonFormatProfileSpec()) const;

  /// Get a text list of fields, types & names, one per line. Useful for tests.
  string getListOfPiecesSpec() const;

  /// Compare two layouts, and tells if all the pieces are in the same order,
  /// with the same properties (name, type, tags, etc). Does not compare actual values!
  /// @param otherLayout: Layout to compare to.
  /// @return True if the layouts are equivalent.
  /// @internal (for debugging/testing)
  bool isSame(const DataLayout& otherLayout) const;

  /// Create a DataLayout from a json description generated with asJson().
  /// @param json: json datalayout description.
  /// @return A DataLayout, or nullptr in case of error.
  /// @internal
  static unique_ptr<DataLayout> makeFromJson(const string& json);

  /// Find a field of type DataPieceValue<T> by name.
  /// @param label: Text name of the field (not the variable name).
  /// @return Pointer to the DataPiece found, or nullptr.
  /// @internal (meant for testing)
  template <class T>
  const DataPieceValue<T>* findDataPieceValue(const string& label) const;
  template <class T>
  DataPieceValue<T>* findDataPieceValue(const string& label);
  /// Find a field of type DataPieceArray<T> by name.
  /// @param label: Text name of the field (not the variable name).
  /// @return Pointer to the DataPiece found, or nullptr.
  /// @internal (meant for testing)
  template <class T>
  const DataPieceArray<T>* findDataPieceArray(const string& label, size_t arraySize) const;
  template <class T>
  DataPieceArray<T>* findDataPieceArray(const string& label, size_t arraySize);
  /// Find a field of type DataPieceVector<T> by name.
  /// @param label: Text name of the field (not the variable name).
  /// @return Pointer to the DataPiece found, or nullptr.
  /// @internal (meant for testing)
  template <class T>
  const DataPieceVector<T>* findDataPieceVector(const string& label) const;
  template <class T>
  DataPieceVector<T>* findDataPieceVector(const string& label);
  /// Find a field of type DataPieceStringMap<T> by name.
  /// @param label: Text name of the field (not the variable name).
  /// @return Pointer to the DataPiece found, or nullptr.
  /// @internal (meant for testing)
  template <class T>
  const DataPieceStringMap<T>* findDataPieceStringMap(const string& label) const;
  template <class T>
  DataPieceStringMap<T>* findDataPieceStringMap(const string& label);
  /// Find a field of type DataPieceString by name.
  /// @param label: Text name of the field (not the variable name).
  /// @return Pointer to the DataPiece found, or nullptr.
  /// @internal (meant for testing)
  const DataPieceString* findDataPieceString(const string& label) const;
  DataPieceString* findDataPieceString(const string& label);
  /// Iterate over the different data piece elements of a DataLayout.
  /// @param callback: a function to call for each element found.
  /// @param type: filter to select only an element type, of UNDEFINED for no filtering.
  void forEachDataPiece(
      const std::function<void(const DataPiece*)>&,
      DataPieceType type = DataPieceType::Undefined) const;
  /// Same as above, but as a non-const version.
  void forEachDataPiece(
      const std::function<void(DataPiece*)>&,
      DataPieceType type = DataPieceType::Undefined);

  /// For debugging: validate that the index for the variable size data looks valid.
  /// If you think you need this in production code, please contact the VRS team...
  /// @return True if the index for the variable size data looks valid.
  bool isVarDataIndexValid() const;
  /// For debugging: get the number of fixed size data pieces declared.
  /// @return The number of fixed size data pieces declared in this datalayout.
  size_t getDeclaredFixedDataPiecesCount() const {
    return fixedSizePieces_.size();
  }
  /// For debugging: get the number of variable size data pieces declared.
  /// @return The number of variable size data pieces declared in this datalayout.
  size_t getDeclaredVarDataPiecesCount() const {
    return varSizePieces_.size();
  }
  /// For debugging: get the number of fixed size data pieces available.
  /// When a datalayout is mapped on another one (when reading from disk),
  /// some pieces might not be available, because the datalayout read from disk might not have them.
  /// @return The number of fixed size data pieces available in this datalayout.
  size_t getAvailableFixedDataPiecesCount() const;
  /// For debugging: get the number of variable size data pieces available.
  /// When a datalayout is mapped on another one (when reading from disk),
  /// some pieces might not be available, because the datalayout read from disk might not have them.
  /// @return The number of variable size data pieces available in this datalayout.
  size_t getAvailableVarDataPiecesCount() const;

 protected:
  /// Get pointer to a section of raw memory, by offset & size.
  /// @param offset: Offset of the field in the fixed-size fields buffer.
  /// @param size: Byte count size of the field.
  /// @return Pointer, or nullptr if the whole field won't fit in the buffer.
  /// @internal
  template <class T>
  T* getFixedData(size_t offset, size_t size) {
    if (mappedDataLayout_ != nullptr) {
      return mappedDataLayout_->getFixedData<T>(offset, size);
    }
    if (offset != kNotFound && offset + size <= fixedData_.size()) {
      return reinterpret_cast<T*>(fixedData_.data() + offset);
    }
    return nullptr;
  }
  /// Get the var size index.
  /// @return The var-size index, const version.
  /// @internal
  const IndexEntry* getVarSizeIndex() const;
  /// Get the var size index.
  /// @return The var-size index, non-const version.
  /// @internal
  IndexEntry* getVarSizeIndex();
  /// Get pointer & size of a variable-size field's data.
  /// @param varPieceIndex: Index of the variable-size piece.
  /// @param outCount: Reference to set to the size in bytes of the data.
  /// @return pointer to the variable-size data found, or nullptr.
  /// @internal
  template <class T>
  T* getVarData(size_t varPieceIndex, size_t& outCount) {
    if (mappedDataLayout_ != nullptr) {
      return mappedDataLayout_->getVarData<T>(varPieceIndex, outCount);
    }
    if (varPieceIndex < varSizePieces_.size()) {
      const IndexEntry& indexEntry = getVarSizeIndex()[varPieceIndex];
      if (indexEntry.getOffset() + indexEntry.getLength() <= varData_.size()) {
        outCount = indexEntry.getLength() / sizeof(T);
        return reinterpret_cast<T*>(varData_.data() + indexEntry.getOffset());
      }
    }
    outCount = 0;
    return nullptr;
  }
  /// Get a piece by index, fixed size pieces first, then variable size pieces.
  DataPiece* getPieceByIndex(size_t pieceIndex);
  /// Get a typed piece by index in the mapped datalayout, exclusively.
  template <class T>
  T* getMappedPiece(size_t pieceIndex) const {
    return mappedDataLayout_ != nullptr
        ? static_cast<T*>(mappedDataLayout_->getPieceByIndex(pieceIndex))
        : nullptr;
  }

  /// After construction of a DataLayout, initializes/resets buffers to hold the DataPiece objects
  /// that have been registered for this DataLayout. This is generally, done automatically by VRS,
  /// but this might be useful for client code to manage a DataLayout creation manually.
  /// In common practice, this is probably only needed by the VRS code itself.
  void initLayout();

  /// Export the DataLayout as json, using a specific profile.
  /// @param jsonWrapper: Wrapper around a json type (to isolate any 3rd party library dependency).
  /// @param profile: Profile describing what information needs to be exported as json.
  void serialize(JsonWrapper& rj, const JsonFormatProfileSpec& profile) const;

  static DataPiece* findMatch(DataPiece* piece, const vector<DataPiece*>& pieces, size_t& start);
  static bool mapPieces(
      const vector<DataPiece*>& searchPieces,
      const vector<DataPiece*>& givenPieces);
  static size_t copyMappedValues(
      const vector<DataPiece*>& pieces,
      const vector<DataPiece*>& mappedPieces);

  friend class internal::DataLayouter;
  friend class DataPiece;
  template <class T>
  friend class DataPieceValue;
  template <class T>
  friend class DataPieceArray;
  template <class T>
  friend class DataPieceVector;
  template <class T>
  friend class DataPieceStringMap;
  friend class DataPieceString;

  /// Ordered fixed-size DataPieces.
  vector<DataPiece*> fixedSizePieces_;
  /// Ordered variable-size DataPieces.
  vector<DataPiece*> varSizePieces_;
  /// Buffer to hold fixed-size pieces, and the index of var size pieces (if any).
  vector<int8_t> fixedData_;
  /// Byte count for all the fixed size pieces + var size index.
  size_t fixedDataSizeNeeded_{};
  /// Buffer holding variable-size pieces, after they've been collected, or read from disk.
  vector<int8_t> varData_;
  /// Tells all the required pieces have been mapped successfully.
  bool hasAllRequiredPieces_{true};
  /// DataLayout this layout has been mapped to, if any.
  DataLayout* mappedDataLayout_{};
};

/// When you just need a placeholder for a DataLayout.
class EmptyDataLayout : public DataLayout {
 public:
  EmptyDataLayout() {
    initLayout();
  }
};

/// \brief Specialized DataLayout class to declare a DataLayout in struct format.
///
/// To create an automatically generated DataLayout class, inherit from AutoDataLayout,
/// then declare the specialized DataPiece objects as members,
/// and finalize the DataLayout by using an AutoDataLayoutEnd object as the *last* class member.
/// All the DataPiece objects will be automatically registered in the DataLayout,
/// allowing the DataLayout to references all its DataPiece objects.
///
/// Example:
///
///   class MyConfig : public AutoDataLayout {
///     DataPieceValue<uint32_t> exposureMode{"exposure_mode"};
///     DataPieceValue<int8_t> serial{"serial_number", 12}; // array of 12 int8_t values
///     DataPieceString description{"description"};
///     AutoDataLayoutEnd end;
///   };
///
/// Be very careful to always match each AutoDataLayout with an AutoDataLayoutEnd member!
class AutoDataLayout : public DataLayout {
 public:
  AutoDataLayout();
};

/// For use within an AutoDataLayout class, to end the AutoDataLayout's construction.
class AutoDataLayoutEnd {
 public:
  AutoDataLayoutEnd();
};

/// \brief Specialized DataLayout for programmatic DataLayout generation.
///
/// Helper class to build a DataLayout manually, piece-by-piece.
/// Make sure to call endLayout as soon as you're no longer adding pieces, to release a global lock.
class ManualDataLayout : public DataLayout {
 public:
  /// Build a DataLayout from a json definition.
  /// This is how VRS rebuilds DataLayout objects when reading a file.
  explicit ManualDataLayout(const string& json);

  /// For manual construction using "add()": don't forget to call endLayout() when you're done.
  ManualDataLayout();
  /// For manual construction, based on an existing layout, cloning all the pieces.
  /// Add more pieces using "add()", but don't forget to call endLayout() when you're done.
  explicit ManualDataLayout(const DataLayout& layout);

  ~ManualDataLayout() override;

  /// Transfer ownership a constructed DataPiece for the ManualDataLayout object to hold.
  /// This ensures that all the DataPiece objects of a DataLayout will be deleted when the
  /// DataLayout object they belong to is deleted.
  /// @param dataPiece: the DataPiece object to give the DataLayout.
  /// @return A straight pointer to the DataPiece object, for convenience. Do not call delete on it!
  DataPiece* add(unique_ptr<DataPiece> dataPiece);

  /// End the construction of the DataLayout. Do not call add() after calling this method.
  void endLayout();

 private:
  vector<unique_ptr<DataPiece>> manualPieces;
  bool layoutInProgress_;
};

/// \brief Helper class to include DataLayout structs containing a set of DataPieceXXX and
/// DataLayoutStruct while preserving the required uniqueness of the field names. Embedded DataPiece
/// objects will have a name automatically prefixed with the name of the DataLayoutStruct, with a
/// '/' in between.
///
/// Example:
///
/// struct PoseLayout : public DataLayoutStruct {
///   DATA_LAYOUT_STRUCT(PoseLayout)
///
///   vrs::DataPieceVector<vrs::Matrix4Dd> orientation{"orientation"};
///   vrs::DataPieceVector<vrs::Matrix3Dd> translation{"translation"};
/// };
///
/// struct Tracking : public AutoDataLayout {
///   PoseLayout leftHandPose{"left_hand"};
///   PoseLayout rightHandPose{"right_hand"};
///
///   vrs::AutoDataLayoutEnd endLayout;
/// };
///
/// Tracking will declare:
///  - 2 vrs::DataPieceVector<vrs::Matrix4Dd> fields, labelled:
///  "left_hand/orientation" and "right_hand/orientation"
///  - 2 vrs::DataPieceVector<vrs::Matrix3Dd> fields, labelled:
///  "left_hand/translation" and "right_hand/translation"
///
/// It is also possible to nest DataLayoutStruct definitions inside DataLayoutStruct definitions,
/// resulting in names growing in length, nesting namespaces.
///
/// Use the alternate macro DATA_LAYOUT_STRUCT_WITH_INIT if you need your DataLayoutStruct to be
/// initialized by an init() method, to assign default values to DataPiece fields for instance.
struct DataLayoutStruct {
  explicit DataLayoutStruct(const string& structName);
  static void dataLayoutStructEnd(const string& structName);
};

#define DATA_LAYOUT_STRUCT(DATA_LAYOUT_STRUCT_TYPE)                 \
  explicit DATA_LAYOUT_STRUCT_TYPE(const std::string& _structName_) \
      : DataLayoutStruct(_structName_) {                            \
    dataLayoutStructEnd(_structName_);                              \
  }

#define DATA_LAYOUT_STRUCT_WITH_INIT(DATA_LAYOUT_STRUCT_TYPE)       \
  explicit DATA_LAYOUT_STRUCT_TYPE(const std::string& _structName_) \
      : DataLayoutStruct(_structName_) {                            \
    dataLayoutStructEnd(_structName_);                              \
    init();                                                         \
  }

/// \brief Helper class to include DataLayout structs containing a sliced array of DataPieceXXX and
/// DataLayoutStruct while preserving the required uniqueness of the field names. Embedded DataPiece
/// objects will have a name automatically prefixed with the name of the DataLayoutStruct, with a
/// '/0'... /Size-1' in between.
///
///  DataLayoutStructArray can be nested within other DataLayoutStruct.
///  If nesting arrays is not required consider using DataPieceArray instead of
///  DataLayoutStructArray as it will be more efficient.
/// Example:
///
/// struct PoseLayout : public DataLayoutStruct {
///   DATA_LAYOUT_STRUCT(PoseLayout)
///
///   vrs::DataPieceVector<vrs::Matrix4Dd> orientation{"orientation"};
///   vrs::DataPieceVector<vrs::Matrix3Dd> translation{"translation"};
/// };
///
/// struct Tracking : public AutoDataLayout {
///   DataLayoutStructArray<PoseLayout, 2> hands{"hands"};
///
///   vrs::AutoDataLayoutEnd endLayout;
/// };
///
/// Tracking will declare:
///  - 2 vrs::DataPieceVector<vrs::Matrix4Dd> fields, labelled:
///  "0/orientation" and "1/orientation"
///  - 2 vrs::DataPieceVector<vrs::Matrix3Dd> fields, labelled:
///  "0/translation" and "1/translation"
template <typename T, size_t Size>
struct DataLayoutStructArray : public vrs::DataLayoutStruct {
  DATA_LAYOUT_STRUCT(DataLayoutStructArray)
  std::array<T, Size> array{createArrayHelper<T>(std::make_index_sequence<Size>())};

  T& operator[](const size_t index) {
    return array[index];
  }

  constexpr const T& operator[](const size_t index) const {
    return array[index];
  }

  constexpr std::size_t size() const noexcept {
    return array.size();
  }

  template <typename S, size_t... Indices>
  static constexpr auto createArrayHelper(std::index_sequence<Indices...>) {
    return std::array<S, sizeof...(Indices)>{S{std::to_string(Indices)}...};
  }
};

/// \brief Helper function to allocate optional fields only when it is enabled.
///
/// The OptionalFields must be a struct with vrs data piece fields and will be stored as unique_ptr.
template <class OptionalFields>
class OptionalDataPieces : public std::unique_ptr<OptionalFields> {
 public:
  explicit OptionalDataPieces(bool allocateFields)
      : std::unique_ptr<OptionalFields>(
            allocateFields ? std::make_unique<OptionalFields>() : nullptr) {}
};

} // namespace vrs
