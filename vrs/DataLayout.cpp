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

#include "DataLayout.h"

// disabled warning C4800 in MSVC:
// This warning is about static casting values to bool in
// getFromRapidjsonValueAs(), temporarily disabling this
#ifdef _MSVC_
#pragma warning(disable : 4800)
#endif

#include <iomanip>
#include <mutex>
#include <sstream>

#define DEFAULT_LOG_CHANNEL "DataLayout"
#include <logging/Log.h>
#include <logging/Verify.h>

#include <vrs/helpers/EnumStringConverter.h>
#include <vrs/helpers/Rapidjson.hpp>
#include <vrs/helpers/Strings.h>
#include <vrs/os/CompilerAttributes.h>
#include <vrs/os/Platform.h>
#include <vrs/os/System.h>

#include "DataLayoutConventions.h"
#include "DataPieces.h"
#include "RecordFormat.h"

namespace vrs {

using namespace std;

const size_t DataLayout::kNotFound = numeric_limits<size_t>::max();
const size_t DataLayout::kVariableSize = numeric_limits<size_t>::max() - 1;

DataLayout::~DataLayout() = default;

size_t DataLayout::getVarDataSizeNeeded() const {
  size_t size = 0;
  for (const auto& piece : varSizePieces_) {
    size += piece->getVariableSize();
  }
  return size;
}

void DataLayout::collectVariableDataAndUpdateIndex() {
  varData_.resize(getVarDataSizeNeeded());
  collectVariableDataAndUpdateIndex(varData_.data());
}

void DataLayout::collectVariableDataAndUpdateIndex(void* destination) {
  int8_t* data = reinterpret_cast<int8_t*>(destination);
  DataLayout::IndexEntry* varSizeIndex = getVarSizeIndex();
  size_t offset = 0;
  for (size_t index = 0; index < varSizePieces_.size(); ++index) {
    DataPiece* piece = varSizePieces_[index];
    size_t size = piece->getVariableSize();
    size_t writtenSize = piece->collectVariableData(data, size);
    if (size != writtenSize) {
      XR_FATAL_ERROR(
          "Failed to collect DataLayout field {}/{}, {} bytes written, {} expected",
          piece->getLabel(),
          piece->getElementTypeName(),
          writtenSize,
          size);
    }
    data += size;
    IndexEntry& indexEntry = varSizeIndex[index];
    indexEntry.setOffset(offset);
    indexEntry.setLength(size);
    offset += size;
  }
}

DataPiece*
DataLayout::findMatch(DataPiece* piece, const vector<DataPiece*>& pieces, size_t& startIndex) {
  // do a linear search starting after last match
  size_t pieceCount = pieces.size();
  for (size_t index = startIndex; index < pieceCount; index++) {
    if (piece->isMatch(*pieces[index])) {
      startIndex = index + 1;
      return pieces[index];
    }
  }
  size_t stopIndex = min(startIndex, pieceCount);
  for (size_t index = 0; index < stopIndex; index++) {
    if (piece->isMatch(*pieces[index])) {
      startIndex = index + 1;
      return pieces[index];
    }
  }
  return nullptr;
}

bool DataLayout::mapPieces(
    const vector<DataPiece*>& searchPieces,
    const vector<DataPiece*>& givenPieces) {
  size_t nextMatchStartIndex = 0;
  bool allRequiredFound = true;
  for (DataPiece* piece : searchPieces) {
    DataPiece* foundPiece = findMatch(piece, givenPieces, nextMatchStartIndex);
    if (foundPiece != nullptr) {
      piece->setIndexOffset(foundPiece->getPieceIndex(), foundPiece->getOffset());
    } else {
      piece->setIndexOffset(kNotFound, kNotFound);
      if (piece->isRequired()) {
        allRequiredFound = false;
      }
    }
  }
  return allRequiredFound;
}

bool DataLayout::mapLayout(DataLayout& targetLayout) {
  mappedDataLayout_ = &targetLayout;
  hasAllRequiredPieces_ = mapPieces(fixedSizePieces_, targetLayout.fixedSizePieces_);
  hasAllRequiredPieces_ =
      mapPieces(varSizePieces_, targetLayout.varSizePieces_) && hasAllRequiredPieces_;
  return hasAllRequiredPieces_;
}

size_t DataLayout::copyMappedValues(
    const vector<DataPiece*>& pieces,
    const vector<DataPiece*>& mappedPieces) {
  // we expect the pieces to map 1:1, same number and same signature for each
  // If that's not the case, the XR_VERIFY will tell you you're doing it wrong.
  if (!XR_VERIFY(pieces.size() == mappedPieces.size())) {
    return 0;
  }
  size_t copyCount = 0;
  auto pieceIter = pieces.begin();
  auto mappedPieceIter = mappedPieces.begin();
  while (pieceIter != pieces.end()) {
    DataPiece& piece = **pieceIter;
    DataPiece* mappedPiece = *mappedPieceIter;
    if (!XR_VERIFY(piece.getPieceType() == mappedPiece->getPieceType()) ||
        !XR_VERIFY(piece.getElementTypeName() == mappedPiece->getElementTypeName())) {
      return 0;
    }
    if (mappedPiece->isMapped()) {
      piece.copyFrom(mappedPiece);
      copyCount++;
    }
    ++pieceIter, ++mappedPieceIter;
  }
  return copyCount;
}

void DataLayout::initDataPiecesToDefaultValue() {
  if (isMapped()) {
    return;
  }
  for (auto* piece : fixedSizePieces_) {
    piece->initToDefault();
  }
  for (auto* piece : varSizePieces_) {
    piece->initToDefault();
  }
}

size_t DataLayout::copyDataPieceValuesFromMappedLayout(const DataLayout& mappedLayout) {
  // This layout may not be mapped, while mappedLayout must be mapped.
  if (!XR_VERIFY(!isMapped()) && !XR_VERIFY(mappedLayout.isMapped())) {
    return 0;
  }
  // This object and the mappedLayout must have the exact same layout!
  return copyMappedValues(fixedSizePieces_, mappedLayout.fixedSizePieces_) +
      copyMappedValues(varSizePieces_, mappedLayout.varSizePieces_);
}

DataPiece* DataLayout::getPieceByIndex(size_t pieceIndex) {
  if (pieceIndex >= fixedSizePieces_.size() + varSizePieces_.size()) {
    return nullptr;
  }
  return pieceIndex < fixedSizePieces_.size()
      ? fixedSizePieces_[pieceIndex]
      : varSizePieces_[pieceIndex - fixedSizePieces_.size()];
}

void DataLayout::requireAllPieces() {
  for (auto* piece : fixedSizePieces_) {
    piece->setRequired(true);
  }
  for (auto* piece : varSizePieces_) {
    piece->setRequired(true);
  }
}

void DataLayout::printLayout(ostream& out, const string& indent) const {
  string subindent = indent + "  ";
  if (!fixedSizePieces_.empty()) {
    out << indent << fixedSizePieces_.size() << " fixed size pieces, total " << fixedData_.size()
        << " bytes.\n";
    for (auto* piece : fixedSizePieces_) {
      piece->print(out, subindent);
    }
  }
  if (!varSizePieces_.empty()) {
    out << indent << varSizePieces_.size() << " variable size pieces, total "
        << getVarDataSizeFromIndex() << " bytes.\n";
    for (auto* piece : varSizePieces_) {
      piece->print(out, subindent);
    }
  }
}

void DataLayout::printLayoutCompact(ostream& out, const string& indent) const {
  string subindent = indent + "  ";
  if (!fixedSizePieces_.empty()) {
    for (auto* piece : fixedSizePieces_) {
      piece->printCompact(out, subindent);
    }
  }
  if (!varSizePieces_.empty()) {
    for (auto* piece : varSizePieces_) {
      piece->printCompact(out, subindent);
    }
  }
}

string DataLayout::asJson(JsonFormatProfile profile) const {
  return asJson(JsonFormatProfileSpec(profile));
}

string DataLayout::asJson(const JsonFormatProfileSpec& profile) const {
  using namespace vrs_rapidjson;
  JDocument doc;
  doc.SetObject();
  JsonWrapper jw(doc);
  serialize(jw, profile);
  return profile.prettyJson ? jDocumentToJsonStringPretty(doc) : jDocumentToJsonString(doc);
}

void DataLayout::serialize(JsonWrapper& jw, const JsonFormatProfileSpec& profile) const {
  using namespace vrs_rapidjson;
  JValue jpieces(kArrayType);
  jpieces.Reserve(static_cast<SizeType>(fixedSizePieces_.size() + varSizePieces_.size()), jw.alloc);
  for (const auto& piece : fixedSizePieces_) {
    JValue jpiece(kObjectType);
    JsonWrapper rj{jpiece, jw.alloc};
    piece->serialize(rj, profile);
    jpieces.PushBack(jpiece, rj.alloc);
  }
  for (const auto& piece : varSizePieces_) {
    JValue jpiece(kObjectType);
    JsonWrapper rj{jpiece, jw.alloc};
    piece->serialize(rj, profile);
    jpieces.PushBack(jpiece, rj.alloc);
  }
  jw.value.AddMember(
      jStringRef(profile.publicNames ? "metadata" : "data_layout"), jpieces, jw.alloc);
}

string DataLayout::getListOfPiecesSpec() const {
  string list;
  list.reserve((fixedSizePieces_.size() + varSizePieces_.size()) * 50);
  for (const auto& piece : fixedSizePieces_) {
    list.append(piece->getLabel()).append(" - ").append(piece->getTypeName()).append("\n");
  }
  for (const auto& piece : varSizePieces_) {
    list.append(piece->getLabel()).append(" - ").append(piece->getTypeName()).append("\n");
  }
  return list;
}

ContentBlock DataLayout::getContentBlock() const {
  return {
      ContentType::DATA_LAYOUT,
      (varSizePieces_.empty()) ? fixedDataSizeNeeded_ : ContentBlock::kSizeUnknown};
}

void DataLayout::initLayout() {
  size_t pieceIndex = 0;
  size_t offset = 0;
  for (auto* piece : fixedSizePieces_) {
    piece->setIndexOffset(pieceIndex++, offset);
    offset += piece->getFixedSize();
  }
  // at the end of the fixed data buffer, we place the index for var size fields,
  // because we know how many we have and we want to read as much as possible in one read.
  fixedDataSizeNeeded_ = offset + varSizePieces_.size() * sizeof(IndexEntry);
  fixedData_.resize(fixedDataSizeNeeded_);
  // Var pieces do not get a buffer by default, their offset tells which OffsetAndLength to use
  offset = 0;
  for (auto* piece : varSizePieces_) {
    piece->setIndexOffset(pieceIndex++, offset++);
  }
  varData_.clear();
  hasAllRequiredPieces_ = true;
  mappedDataLayout_ = nullptr;
}

void DataLayout::getRawData(vector<int8_t>& outRawData) const {
  if (mappedDataLayout_ != nullptr) {
    mappedDataLayout_->getRawData(outRawData);
  } else {
    outRawData.resize(fixedData_.size() + varData_.size());
    if (!fixedData_.empty()) {
      memcpy(outRawData.data(), fixedData_.data(), fixedData_.size());
    }
    if (!varData_.empty()) {
      memcpy(outRawData.data() + fixedData_.size(), varData_.data(), varData_.size());
    }
  }
}

void DataLayout::stageCurrentValues() {
  // only variable size pieces need to be staged
  for (auto* piece : varSizePieces_) {
    piece->stageCurrentValue();
  }
}

bool DataLayout::copyClonedDataPieceValues(const DataLayout& originalLayout) {
  // sanity checks. Failed verifies imply that the layout isn't a derived clone of clonedLayout.
  if (!XR_VERIFY(
          fixedSizePieces_.size() >= originalLayout.fixedSizePieces_.size() &&
          varSizePieces_.size() >= originalLayout.varSizePieces_.size() &&
          fixedDataSizeNeeded_ >= originalLayout.fixedDataSizeNeeded_)) {
    return false;
  }
  const vector<int8_t>& originalFixedData = originalLayout.mappedDataLayout_ != nullptr
      ? originalLayout.mappedDataLayout_->fixedData_
      : originalLayout.fixedData_;
  if (!XR_VERIFY(fixedData_.size() >= originalFixedData.size())) {
    return false;
  }
  // If the layout was cloned from clonedLayout, then the first fixed size pieces are the same,
  // in the same order, and we can use a raw memory copy to copy all these values at once.
  if (!originalFixedData.empty()) {
    memcpy(fixedData_.data(), originalFixedData.data(), originalFixedData.size());
  }
  for (size_t k = 0; k < originalLayout.varSizePieces_.size(); k++) {
    DataPiece* original = originalLayout.varSizePieces_[k];
    DataPiece* copy = varSizePieces_[k];
    if (!XR_VERIFY(copy->getPieceType() == original->getPieceType())) {
      return false;
    }
    copy->copyFrom(original);
  }
  return true;
}

size_t DataLayout::getVarDataSizeFromIndex() const {
  if (mappedDataLayout_ != nullptr) {
    return mappedDataLayout_->getVarDataSizeFromIndex();
  }
  size_t size = 0;
  if (!varSizePieces_.empty() && fixedData_.size() == fixedDataSizeNeeded_) {
    // The last entry in the index tells us how much var data we're using overall
    const IndexEntry& lastEntry = getVarSizeIndex()[varSizePieces_.size() - 1];
    size = lastEntry.getOffset() + lastEntry.getLength();
  }
  return size;
}

unique_ptr<DataLayout> DataLayout::makeFromJson(const string& json) {
  return unique_ptr<DataLayout>(new ManualDataLayout(json));
}

bool DataLayout::isSame(const DataLayout& otherLayout) const {
  if (fixedSizePieces_.size() != otherLayout.fixedSizePieces_.size() ||
      varSizePieces_.size() != otherLayout.varSizePieces_.size()) {
    return false;
  }
  for (size_t index = 0; index < fixedSizePieces_.size(); ++index) {
    if (!fixedSizePieces_[index]->isSame(otherLayout.fixedSizePieces_[index])) {
      return false;
    }
  }
  for (size_t index = 0; index < varSizePieces_.size(); ++index) {
    if (!varSizePieces_[index]->isSame(otherLayout.varSizePieces_[index])) {
      return false;
    }
  }
  return true;
}

/// \brief DataLayout private namespace for internal helper classes & functions.
namespace internal {

/// \brief Helper class to manage the registration of DataPiece objects within a single DataLayout.
///
/// C++ doesn't support a form of introspection that would allow objects to "find their parent".
/// This technique allows the automatic registration of DataPiece objects.
///  - call dataLayoutBegin(layout) to start registering DataPiece objects with a layout object.
///  - create DataPiece objects, which will automatically call registerDataPiece(dataPiece).
///  - call dataLayoutEnd() to end the registration.
/// This class uses a global lock to prevent simultaneous construction of DataLayout objects from
/// mixing each other. This class is purely internal to VRS, and nothing outside of VRS should ever
/// rely on it. This is one of the reasons why it's only declared & defined in the same cpp file.
class DataLayouter {
 public:
  static DataLayouter& get() {
    static DataLayouter sLayouter;
    return sLayouter;
  }
  /// Start auto-registration of DataPiece objects in the provided DataLayout.
  /// Attention! This function acquires a global DataLayout creation lock to prevent mixups if two
  /// DataLayouts were to be created concurrently in two threads.
  /// You must guarantee that this call is matched by a call to data dataLayoutEnd() to release the
  /// global lock. This is called by the constructors of AutoDataLayout and ManualDataLayout.
  /// @param layout: DataLayout all DataPieces created will register themselves with.
  void dataLayoutBegin(DataLayout& layout) DISABLE_THREAD_SAFETY_ANALYSIS {
    // If your thread blocks here, an AutoDataLayout class is missing an AutoDataLayoutEnd field,
    // or a ManualDataLayout is missing a ManualDataLayout::endLayout() call...
    mutex_.lock();
    currentLayout_ = &layout;
    prefix_.clear();
  }
  /// Called by DataPiece constructor to register themselves in the DataLayout currently
  /// constructed.
  /// @param dataPiece: Reference to the DataPiece to register.
  /// @return A reference to the DataLayout being constructed.
  DataLayout& registerDataPiece(DataPiece* dataPiece) {
    // If this check fails, you're trying to create a DataPiece outside of a DataLayout.
    // Use AutoDataLayout (preferred) or ManualDataLayout if you know what you're doing.
    XR_CHECK_NOTNULL(currentLayout_);
    if (dataPiece->hasFixedSize()) {
      currentLayout_->fixedSizePieces_.push_back(dataPiece);
    } else {
      currentLayout_->varSizePieces_.push_back(dataPiece);
    }
    return *currentLayout_;
  }
  /// End the construction of a DataLayout.
  /// This is called by AutoDataLayoutEnd's constructor, and on demand by ManualDataLayout.
  void dataLayoutEnd() DISABLE_THREAD_SAFETY_ANALYSIS {
    XR_CHECK_NE(
        currentLayout_,
        nullptr,
        "DataLayouter::dataLayoutEnd() called without prior matching call to DataLayouter::dataLayoutBegin().");
    DataLayout* layout = currentLayout_;
    currentLayout_ = nullptr;
    mutex_.unlock();
    layout->initLayout();
  }
  /// Start a sub-structure within a DataLayout, with its own "namespace".
  /// The DataPiece objects within that struct will have a label prepended with the namespace.
  /// The "namespaces" may be nested, so that you may have a DataLayoutStruct within a
  /// DataLayoutStruct.
  /// @param structName: name of the sub-struct, to be added to the prefix namespace.
  void dataLayoutStructStart(const string& structName) {
    // If this check fails, you're trying to create a DataPiece outside of a DataLayout.
    // Use AutoDataLayout (preferred) or ManualDataLayout if you know what you're doing.
    // DataLayoutStruct may only live within an AutoDataLayout or a ManualDataLayout.
    XR_CHECK_NOTNULL(currentLayout_);
    if (prefix_.empty()) {
      prefix_ = structName;
    } else {
      prefix_ += '/' + structName;
    }
  }
  /// Get the actual label of a DataPiece. This method adds a prefix to the label, if the DataPiece
  /// belongs to a DataLayoutStruct.
  /// @param label: name requested for the DataPiece.
  /// @return The full label for the DataPiece, which may have been prepended with a suffix.
  string dataLayoutPieceLabel(string&& label) {
    if (prefix_.empty()) {
      return std::move(label);
    }
    return prefix_ + '/' + label;
  }
  /// End the construction of a DataPieceStruct.
  /// @param stuctName: name of the structName which construction is being ended.
  void dataLayoutStructEnd(const string& structName) {
    XR_CHECK_NOTNULL(currentLayout_);
    if (prefix_.length() > structName.length()) {
      prefix_.resize(prefix_.length() - (structName.length() + 1));
    } else {
      prefix_.clear();
    }
  }

 private:
  DataLayouter() = default;

  mutex mutex_;
  DataLayout* currentLayout_ = nullptr;
  string prefix_;
};

namespace {

string_view sDataPieceTypeNames[] = {
    "undefined",
    "DataPieceValue",
    "DataPieceArray",
    "DataPieceVector",
    "DataPieceString",
    "DataPieceStringMap"};
ENUM_STRING_CONVERTER(DataPieceType, sDataPieceTypeNames, DataPieceType::Undefined);

} // namespace

static string makePieceName(const string_view& pieceTypeName, const string& dataType) {
  string pieceName;
  pieceName.reserve(pieceTypeName.size() + dataType.size() + 2);
  pieceName.append(pieceTypeName).append("<").append(dataType).append(">");
  return pieceName;
}

static inline string makePieceName(DataPieceType pieceType, const string& dataType) {
  return pieceType == DataPieceType::String
      ? DataPieceTypeConverter::toString(pieceType)
      : makePieceName(DataPieceTypeConverter::toStringView(pieceType), dataType);
}

using DataPieceMaker = DataPiece* (*)(const DataPiece::MakerBundle&);

/// \brief Helper factory class to create DataPiece objects.
struct DataPieceFactory {
  static void registerClass(const string& pieceName, DataPieceMaker maker) {
    DataPieceFactory::get().registry_[pieceName] = maker;
  }
  static unique_ptr<DataPiece> makeDataPiece(
      const string& pieceName,
      const DataPiece::MakerBundle& bundle) {
    const auto& entry = get().registry_.find(pieceName);
    if (entry != get().registry_.end()) {
      return unique_ptr<DataPiece>(entry->second(bundle));
    }
    return nullptr;
  }

  template <class T>
  struct Registerer {
    explicit Registerer(const char* pieceTypeName, const string& dataType) {
      registerClass(makePieceName(pieceTypeName, dataType), makeDataPiece);
    }
    explicit Registerer(const string& pieceName) {
      registerClass(pieceName, makeDataPiece);
    }
    static DataPiece* makeDataPiece(const DataPiece::MakerBundle& bundle) {
      return new T(bundle);
    }
  };

 private:
  static DataPieceFactory& get() {
    static DataPieceFactory sDataPieceValueFactory;
    return sDataPieceValueFactory;
  }
  map<string, DataPieceMaker> registry_;
};

static DataPieceFactory::Registerer<DataPieceString> Registerer_DataPieceString(
    DataPieceTypeConverter::toString(DataPieceType::String));

static DataPieceFactory::Registerer<DataPieceVector<string>> Registerer_DataPieceVector(
    makePieceName(DataPieceType::Vector, getTypeName<string>()));

static DataPieceFactory::Registerer<DataPieceStringMap<string>> Registerer_DataPieceStringMap(
    makePieceName(DataPieceType::StringMap, getTypeName<string>()));

} // namespace internal

template <typename T>
const DataPieceValue<T>* DataLayout::findDataPieceValue(const string& label) const {
  const string& typeName = vrs::getTypeName<T>();
  for (DataPiece* piece : fixedSizePieces_) {
    if (piece->getPieceType() == DataPieceType::Value && piece->getLabel() == label &&
        typeName == piece->getElementTypeName()) {
      return static_cast<DataPieceValue<T>*>(piece);
    }
  }
  return nullptr;
}

template <typename T>
DataPieceValue<T>* DataLayout::findDataPieceValue(const string& label) {
  return const_cast<DataPieceValue<T>*>(
      const_cast<const DataLayout*>(this)->findDataPieceValue<T>(label));
}

template <typename T>
const DataPieceArray<T>* DataLayout::findDataPieceArray(const string& label, size_t arraySize)
    const {
  const string& typeName = vrs::getTypeName<T>();
  size_t size = arraySize * sizeof(T);
  for (DataPiece* piece : fixedSizePieces_) {
    if (piece->getPieceType() == DataPieceType::Array && piece->getFixedSize() == size &&
        piece->getLabel() == label && typeName == piece->getElementTypeName()) {
      return static_cast<DataPieceArray<T>*>(piece);
    }
  }
  return nullptr;
}

template <typename T>
DataPieceArray<T>* DataLayout::findDataPieceArray(const string& label, size_t arraySize) {
  return const_cast<DataPieceArray<T>*>(
      const_cast<const DataLayout*>(this)->findDataPieceArray<T>(label, arraySize));
}

template <typename T>
const DataPieceVector<T>* DataLayout::findDataPieceVector(const string& label) const {
  const string& typeName = vrs::getTypeName<T>();
  for (DataPiece* piece : varSizePieces_) {
    if (piece->getPieceType() == DataPieceType::Vector && piece->getLabel() == label &&
        typeName == piece->getElementTypeName()) {
      return static_cast<DataPieceVector<T>*>(piece);
    }
  }
  return nullptr;
}

template <typename T>
DataPieceVector<T>* DataLayout::findDataPieceVector(const string& label) {
  return const_cast<DataPieceVector<T>*>(
      const_cast<const DataLayout*>(this)->findDataPieceVector<T>(label));
}

template const DataPieceVector<string>* DataLayout::findDataPieceVector<string>(
    const string& label) const;
template DataPieceVector<string>* DataLayout::findDataPieceVector<string>(const string& label);

template <typename T>
const DataPieceStringMap<T>* DataLayout::findDataPieceStringMap(const string& label) const {
  const string& typeName = vrs::getTypeName<T>();
  for (DataPiece* piece : varSizePieces_) {
    if (piece->getPieceType() == DataPieceType::StringMap && piece->getLabel() == label &&
        typeName == piece->getElementTypeName()) {
      return static_cast<DataPieceStringMap<T>*>(piece);
    }
  }
  return nullptr;
}

template <typename T>
DataPieceStringMap<T>* DataLayout::findDataPieceStringMap(const string& label) {
  return const_cast<DataPieceStringMap<T>*>(
      const_cast<const DataLayout*>(this)->findDataPieceStringMap<T>(label));
}

template const DataPieceStringMap<string>* DataLayout::findDataPieceStringMap<string>(
    const string& label) const;
template DataPieceStringMap<string>* DataLayout::findDataPieceStringMap<string>(
    const string& label);

const DataPieceString* DataLayout::findDataPieceString(const string& label) const {
  for (DataPiece* piece : varSizePieces_) {
    if (piece->getPieceType() == DataPieceType::String && piece->getLabel() == label) {
      return static_cast<DataPieceString*>(piece);
    }
  }
  return nullptr;
}

DataPieceString* DataLayout::findDataPieceString(const string& label) {
  return const_cast<DataPieceString*>(
      const_cast<const DataLayout*>(this)->findDataPieceString(label));
}

void DataLayout::forEachDataPiece(
    const function<void(const DataPiece*)>& callback,
    DataPieceType type) const {
  if (type == DataPieceType::Undefined || type == DataPieceType::Value ||
      type == DataPieceType::Array) {
    for (const DataPiece* piece : fixedSizePieces_) {
      if (type == DataPieceType::Undefined || piece->getPieceType() == type) {
        callback(piece);
      }
    }
  }
  if (type != DataPieceType::Value && type != DataPieceType::Array) {
    for (const DataPiece* piece : varSizePieces_) {
      if (type == DataPieceType::Undefined || piece->getPieceType() == type) {
        callback(piece);
      }
    }
  }
}

void DataLayout::forEachDataPiece(const function<void(DataPiece*)>& callback, DataPieceType type) {
  if (type == DataPieceType::Undefined || type == DataPieceType::Value ||
      type == DataPieceType::Array) {
    for (DataPiece* piece : fixedSizePieces_) {
      if (type == DataPieceType::Undefined || piece->getPieceType() == type) {
        callback(piece);
      }
    }
  }
  if (type != DataPieceType::Value && type != DataPieceType::Array) {
    for (DataPiece* piece : varSizePieces_) {
      if (type == DataPieceType::Undefined || piece->getPieceType() == type) {
        callback(piece);
      }
    }
  }
}

const DataLayout::IndexEntry* DataLayout::getVarSizeIndex() const {
  if (mappedDataLayout_ != nullptr) {
    return mappedDataLayout_->getVarSizeIndex();
  }
  const int8_t* fixedSizeDataEnd = fixedData_.data() + fixedData_.size();
  return reinterpret_cast<const IndexEntry*>(fixedSizeDataEnd) - varSizePieces_.size();
}

DataLayout::IndexEntry* DataLayout::getVarSizeIndex() {
  if (mappedDataLayout_ != nullptr) {
    return mappedDataLayout_->getVarSizeIndex();
  }
  int8_t* fixedSizeDataEnd = fixedData_.data() + fixedData_.size();
  return reinterpret_cast<IndexEntry*>(fixedSizeDataEnd) - varSizePieces_.size();
}

bool DataLayout::isVarDataIndexValid() const {
  if (mappedDataLayout_ != nullptr) {
    return mappedDataLayout_->isVarDataIndexValid();
  }
  if (fixedDataSizeNeeded_ != fixedData_.size()) {
    XR_LOGW(
        "Fixed data size mismatch: expected {} bytes, but only found {} bytes",
        fixedDataSizeNeeded_,
        fixedData_.size());
    return false;
  }
  if (varSizePieces_.size() * sizeof(IndexEntry) > fixedData_.size()) {
    XR_LOGW(
        "Fixed data too small for the var data index: Needed {} bytes, but only found {} bytes",
        varSizePieces_.size() * sizeof(IndexEntry),
        fixedData_.size());
    return false;
  }
  const IndexEntry* varSizeIndex = getVarSizeIndex();
  size_t currentOffset = 0;
  size_t maxOffset = varData_.size();
  bool allGood = true;
  for (size_t index = 0; index < varSizePieces_.size(); index++) {
    const IndexEntry& indexEntry = varSizeIndex[index];
    if (indexEntry.getOffset() != currentOffset) {
      XR_LOGW(
          "Offset of var piece #{} '{}' is {} instead of {}",
          index,
          varSizePieces_[index]->getLabel(),
          indexEntry.getOffset(),
          currentOffset);
      allGood = false;
    } else if (indexEntry.getOffset() + indexEntry.getLength() > maxOffset) {
      XR_LOGW(
          "Size of var piece #{} '{}' is too large, {} bytes instead of {} bytes max.",
          index,
          varSizePieces_[index]->getLabel(),
          indexEntry.getLength(),
          maxOffset - currentOffset);
      allGood = false;
    }
    currentOffset += indexEntry.getLength();
  }
  if (currentOffset != maxOffset) {
    XR_LOGW(
        "Cummulated size of var pieces isn't lining up. The index references {} bytes, "
        "but found {} bytes of var data.",
        currentOffset,
        maxOffset);
    allGood = false;
  }
  return allGood;
}

size_t DataLayout::getAvailableFixedDataPiecesCount() const {
  size_t count = 0;
  for (auto* piece : fixedSizePieces_) {
    if (piece->isAvailable()) {
      count++;
    }
  }
  return count;
}

size_t DataLayout::getAvailableVarDataPiecesCount() const {
  size_t count = 0;
  for (auto* piece : varSizePieces_) {
    if (piece->isAvailable()) {
      count++;
    }
  }
  return count;
}

bool DataPiece::isSame(const DataPiece* rhs) const {
  return isMatch(*rhs) && isRequired() == rhs->isRequired() && vrs::isSame(tags_, rhs->tags_);
}

namespace {

#define FIELDINDENT "  "
#define SUBINDENT "    "
const string_view kTruncated = "  [ ... ]  ";
const size_t kPrintCompactMaxVectorValues = 400; // arbitrary

string truncatedString(const string& str) {
  const size_t kMaxLength = os::getTerminalWidth() / 2;
  if (str.size() < kMaxLength) {
    return helpers::make_printable(str);
  }
  const size_t kSplitLength = kMaxLength / 5;
  string front(str.c_str(), kMaxLength - kSplitLength);
  string tail(str.c_str() + str.size() - kSplitLength, kSplitLength);
  return helpers::make_printable(front).append(kTruncated).append(helpers::make_printable(tail));
}

string printable(const char* str, size_t size, bool front) {
  string result(str, size);
  result = helpers::make_printable(result);
  if (front) {
    result.resize(size);
  } else if (result.size() > size) {
    result = result.substr(result.size() - size);
  }
  return result;
}

// Print a string with a prefix and a suffix, carefully truncating it if it's too long to fit a
// width, but only within reason. The string is made printable before printing, which can change its
// length...
void printStringFitted(
    const string& prefix,
    const string& str,
    const string& suffix,
    ostream& os,
    size_t kMax = 0) {
  if (kMax == 0) {
    kMax = os::getTerminalWidth();
  }
  size_t overhead = prefix.size() + suffix.size() + 2;
  if (overhead + str.size() <= kMax) {
    string printableStr = helpers::make_printable(str);
    if (overhead + printableStr.size() <= kMax) {
      os << prefix << '"' << printableStr << '"' << suffix << '\n';
      return;
    }
  }
  overhead += kTruncated.size();
  size_t frontSize = 6;
  size_t tailSize = 2;
  if (overhead + frontSize + tailSize < kMax) {
    size_t rest = kMax - overhead;
    frontSize = rest / 4 * 3;
    tailSize = rest - frontSize;
  }
  string front = printable(str.c_str(), frontSize, true);
  string tail = printable(str.c_str() + str.size() - tailSize, tailSize, false);
  os << prefix << '"' << front << kTruncated << tail << '"' << suffix << '\n';
}

void printTextFitted(
    const string& prefix,
    const string& text,
    const string& suffix,
    ostream& os,
    size_t kMax = 0) {
  if (kMax == 0) {
    kMax = os::getTerminalWidth();
  }
  size_t overhead = prefix.size() + suffix.size();
  if (overhead + text.size() <= kMax) {
    os << prefix << text << suffix << '\n';
    return;
  }
  overhead += kTruncated.size();
  size_t frontSize = 6;
  size_t tailSize = 2;
  if (overhead + frontSize + tailSize < kMax) {
    size_t rest = kMax - overhead;
    frontSize = rest / 4 * 3;
    tailSize = rest - frontSize;
  }
  string front(text.c_str(), frontSize);
  string tail(text.c_str() + text.size() - tailSize, tailSize);
  os << prefix << front << kTruncated << tail << suffix << '\n';
}

void printTextWrapped(
    const string& prefix,
    const string& text,
    const string& indent,
    ostream& os,
    size_t kMax = 0) {
  if (kMax == 0) {
    kMax = os::getTerminalWidth();
  }
  kMax = max<size_t>(kMax, prefix.size() + 20);
  if (prefix.size() + text.size() <= kMax) {
    os << prefix << text << '\n';
    return;
  }
  os << prefix << text.substr(0, kMax - prefix.size()) << '\n';
  size_t offset = kMax - prefix.size();
  while (offset < text.size()) {
    size_t nextOffset = min<size_t>(offset + kMax - indent.size(), text.size());
    os << indent << text.substr(offset, nextOffset - offset) << '\n';
    offset = nextOffset;
  }
}

// Print char/int8/uint8 as numbers
// https://stackoverflow.com/questions/19562103/uint8-t-cant-be-printed-with-cout
namespace special_chars {
ostream& operator<<(ostream& os, char c) {
  return is_signed<char>::value ? os << static_cast<int>(c) : os << static_cast<unsigned int>(c);
}

ostream& operator<<(ostream& os, signed char c) {
  return os << static_cast<int>(c);
}

ostream& operator<<(ostream& os, unsigned char c) {
  return os << static_cast<unsigned int>(c);
}

ostream& operator<<(ostream& os, const string& s) {
  return std::operator<<(os, helpers::make_printable(s));
}

} // namespace special_chars

template <typename T, size_t N>
ostream& operator<<(ostream& os, const PointND<T, N>& point) {
  os << '[' << point.x();
  for (size_t s = 1; s < N; ++s) {
    os << ", " << point.dim[s];
  }
  os << ']';
  return os;
}

template <typename T, size_t N>
ostream& operator<<(ostream& os, const MatrixND<T, N>& matrix) {
  os << '[' << matrix.points[0];
  for (size_t s = 1; s < N; ++s) {
    os << ", " << matrix.points[s];
  }
  os << ']';
  return os;
}

/*
 * Helper classes to size/store/load POD & string values to/from a byte array.
 * There is a generic trivial implementation for POD types. There are specialized implementations
 * for non-POD types, such as string (maybe more in the future).
 */

/// Get the number of bytes needed to store an objects of type T.
/// @param object: a reference to that object (the value might be needed for non-POD types).
/// @return Number of bytes needed.
template <class T>
inline size_t elementSize(const T&) {
  return sizeof(T);
}

// Storing strings as a uint32_t for char count + the characters of the strings.
// Not using 0 as terminator, as if a string has been manually created with an internal 0,
// this would break the ability to read the data (and possibly crash the code).
template <>
inline size_t elementSize<string>(const string& s) {
  return sizeof(uint32_t) + s.size(); // size as uint32_t + string bytes
}

/// Store an object in a buffer, updating the number of bytes used in that buffer.
/// This operation is designed to be safe: if there is not enough space in the buffer, the copy
/// will fail.
/// @param dest: pointer to the beginning of the buffer.
/// @param sourceValue: value to copy in the buffer.
/// @param writtenSize: number of bytes of the buffer already used. Will be updated if the data was
/// copied in the buffer. Writing will happen at the address dest + writtenSize.
/// @param maxSize: size of the buffer. Will never write past dest + maxSize.
/// @return True if the copy was done & writtenSize updated. False, if the buffer was too small.
template <class T>
bool storeElement(int8_t* dest, const T& sourceValue, size_t& writtenSize, size_t maxSize) {
  if (writtenSize + sizeof(T) > maxSize) {
    return false;
  }
  writeUnaligned<T>(dest + writtenSize, sourceValue);
  writtenSize += sizeof(T);
  return true;
}

template <>
bool storeElement<string>(
    int8_t* dest,
    const string& sourceStr,
    size_t& writtenSize,
    size_t maxSize) {
  uint32_t byteCount = static_cast<uint32_t>(sourceStr.size());
  if (writtenSize + sizeof(uint32_t) + byteCount > maxSize) {
    return false;
  }
  writeUnaligned<uint32_t>(dest + writtenSize, byteCount);
  if (byteCount > 0) {
    memcpy(dest + writtenSize + sizeof(uint32_t), sourceStr.c_str(), byteCount);
  }
  writtenSize += sizeof(uint32_t) + byteCount;
  return true;
}

/// Load a typed value from a buffer, updating the number of bytes read.
/// This operations is designed to always be safe: if the buffer is too small, the read will fail.
/// @param destValue: value to read.
/// @param source: pointer to the beginning of the buffer.
/// @param readSize: number of buffer bytes already read.
/// The data will be read from source + readSize, and readSize will be updated to reflect that.
/// @param maxSize: size of the buffer. Read won't happen past source + maxSize.
/// @return True if the read was done & readSize updated. False, if the buffer was too small.
template <class T>
bool loadElement(T& destValue, const int8_t* source, size_t& readSize, size_t maxSize) {
  if (readSize + sizeof(T) > maxSize) {
    return false;
  }
  destValue = readUnaligned<T>(source + readSize);
  readSize += sizeof(T);
  return true;
}

template <>
bool loadElement<string>(string& destStr, const int8_t* source, size_t& readSize, size_t maxSize) {
  if (readSize + sizeof(uint32_t) > maxSize) {
    destStr.clear();
    return false;
  }
  uint32_t byteCount = readUnaligned<uint32_t>(source + readSize);
  readSize += sizeof(uint32_t);
  if (readSize + byteCount > maxSize) {
    destStr.clear();
    return false;
  }
  if (byteCount > 0) {
    destStr.resize(byteCount);
    memcpy(&destStr.front(), source + readSize, byteCount);
    readSize += byteCount;
  } else {
    destStr.clear();
  }
  return true;
}

template <typename T>
void adjustPrecision(const T&, ostream&) {}

template <>
void adjustPrecision<double>(const double& v, ostream& str) {
  // if the value looks like a count of seconds since EPOCH between 2015 and 2035, then
  // use fixed 3 digit precision, otherwise let the C++ library format the number...
  const double kEpoch2015 = 1420070400; // Jan 1, 2015
  const double kEpoch2040 = 2051222400; // Jan 1, 2035
  if (v >= kEpoch2015 && v < kEpoch2040) {
    str << fixed << setprecision(3);
  } else {
    str << defaultfloat;
  }
}

// Adjust the formatting for a value of type T of the stream named "out", then return value as is...
// For use in template classes using the type T and printing out to "out"...
#define FORMAT(PARAM_T, out) (adjustPrecision<T>(PARAM_T, out), PARAM_T)

template <typename T>
void printValue(ostream& out, const T& value, const string&) {
  using namespace special_chars;
  out << FORMAT(value, out);
}

template <typename T>
string sprintValue(const T& value, const string&) {
  stringstream ss;
  printValue<T>(ss, value, {});
  return ss.str();
}

template <>
void printValue<datalayout_conventions::ImageSpecType>(
    ostream& out,
    const datalayout_conventions::ImageSpecType& value,
    const string& label) {
  using namespace special_chars;
  if (label == datalayout_conventions::kImagePixelFormat) {
    out << toString(static_cast<PixelFormat>(value)) << " (" << value << ")";
  } else {
    out << value;
  }
}

template <>
string sprintValue<datalayout_conventions::ImageSpecType>(
    const datalayout_conventions::ImageSpecType& value,
    const string& label) {
  stringstream ss;
  printValue<datalayout_conventions::ImageSpecType>(ss, value, label);
  return ss.str();
}

template <>
void printValue<uint8_t>(ostream& out, const uint8_t& value, const string& label) {
  using namespace special_chars;
  if (label == datalayout_conventions::kAudioFormat) {
    out << toString(static_cast<AudioFormat>(value)) << " (" << value << ")";
  } else if (label == datalayout_conventions::kAudioSampleFormat) {
    out << toString(static_cast<AudioSampleFormat>(value)) << " (" << value << ")";
  } else {
    out << value;
  }
}

template <>
string sprintValue<uint8_t>(const uint8_t& value, const string& label) {
  stringstream ss;
  printValue<uint8_t>(ss, value, label);
  return ss.str();
}

} // namespace

/*
 * DataPiece<T> definitions
 */

DataPiece::DataPiece(string label, DataPieceType type, size_t size)
    : label_{internal::DataLayouter::get().dataLayoutPieceLabel(std::move(label))},
      pieceType_{type},
      fixedSize_{size},
      pieceIndex_{DataLayout::kNotFound},
      offset_{DataLayout::kNotFound},
      layout_{internal::DataLayouter::get().registerDataPiece(this)},
      required_{false} {}

DataPiece::~DataPiece() = default;

bool DataPiece::getTag(const string& tagName, string& outTag) const {
  const auto& iter = tags_.find(tagName);
  if (iter != tags_.end()) {
    outTag = iter->second;
    return true;
  }
  return false;
}

bool DataPiece::isMatch(const DataPiece& rhs) const {
  return pieceType_ == rhs.pieceType_ && fixedSize_ == rhs.fixedSize_ && label_ == rhs.label_ &&
      getElementTypeName() == rhs.getElementTypeName();
}

const string DataPiece::kUnit = "unit";
const string DataPiece::kDescription = "description";
const string DataPiece::kMinValue = "min";
const string DataPiece::kMaxValue = "max";
const string DataPiece::kMinIncrement = "min_increment";
const string DataPiece::kMaxIncrement = "max_increment";

struct DataPiece::MakerBundle {
  MakerBundle(const char* l, const JValue& p) : label(l), piece(p) {}

  const char* label;
  const JValue& piece;
  size_t arraySize{}; // for DataPieceArray only
};

string DataPiece::getTypeName() const {
  return vrs::internal::makePieceName(pieceType_, getElementTypeName());
}

void DataPiece::serialize(JsonWrapper& rj, const JsonFormatProfileSpec& profile) {
  using namespace vrs_rapidjson;
  if (profile.name) {
    rj.addMember("name", jStringRef(getLabel()));
  }
  if (profile.type) {
    string typeName = getTypeName();
    // Remove the "DataPiece" prefix that's not pretty...
    string_view prefix = "DataPiece";
    if (profile.shortType && strncmp(typeName.c_str(), prefix.data(), prefix.size()) == 0) {
      typeName = typeName.substr(prefix.size());
    }
    rj.addMember("type", typeName);
  }
  if (profile.index) {
    if (hasFixedSize()) {
      if (isAvailable()) {
        rj.addMember("offset", static_cast<SizeType>(getOffset()));
      }
    } else if (offset_ != DataLayout::kNotFound) {
      rj.addMember("index", static_cast<SizeType>(getOffset()));
    }
  }
  if (profile.tags) {
    serializeStringRefMap(tags_, rj, "tags");
  }
  if (profile.required && isRequired()) {
    rj.addMember("required", true);
  }
}

/*
 * DataPieceValue<T> definitions
 */

template <typename T>
DataPieceValue<T>::DataPieceValue(const DataPiece::MakerBundle& bundle)
    : DataPieceValue(bundle.label) {
  using namespace vrs_rapidjson;
  const JValue::ConstMemberIterator defaultV = bundle.piece.FindMember("default");
  if (defaultV != bundle.piece.MemberEnd()) {
    T defaultValue;
    if (getFromJValue(defaultV->value, defaultValue)) {
      setDefault(defaultValue);
    }
  }
  getJMap(properties_, bundle.piece, "properties");
}

template <typename T>
void DataPieceValue<T>::print(ostream& out, const string& indent) const {
  string str;
  str.reserve(400);
  str.append(getLabel()).append(" (").append(getElementTypeName()).append(") @ ");
  if (getOffset() == DataLayout::kNotFound) {
    str.append("<unavailable>");
  } else {
    str.append(std::to_string(getOffset()));
  }
  str.append("+").append(std::to_string(getFixedSize()));
  if (isRequired()) {
    str.append(" required");
  }
  T value;
  bool hasValue = get(value);
  str.append(!hasValue ? " (default): " : ": ").append(sprintValue<T>(value, getLabel()));
  printTextWrapped(indent, str, indent + SUBINDENT, out);
  for (const auto& prop : properties_) {
    using namespace special_chars;
    out << indent << FIELDINDENT << prop.first << ": " << FORMAT(prop.second, out) << "\n";
  }
}

template <typename T>
void DataPieceValue<T>::printCompact(ostream& out, const string& indent) const {
  printTextFitted(
      fmt::format("{}{}: ", indent, getLabel()),
      sprintValue<T>(get(), getLabel()),
      (getOffset() == DataLayout::kNotFound) ? " *" : "",
      out);
}

template <typename T>
bool DataPieceValue<T>::isSame(const DataPiece* rhs) const {
  if (!DataPiece::isSame(rhs)) {
    return false;
  }
  const auto* other = reinterpret_cast<const DataPieceValue<T>*>(rhs);
  return vrs::isSame(this->defaultValue_, other->defaultValue_) &&
      vrs::isSame(this->properties_, other->properties_);
}

template <typename T>
void DataPieceValue<T>::serialize(JsonWrapper& rj, const JsonFormatProfileSpec& profile) {
  if (profile.value) {
    T value;
    if (get(value)) {
      rj.addMember("value", value);
    }
  }
  DataPiece::serialize(rj, profile);
  if (profile.defaults) {
    if (getDefault() != T{}) {
      rj.addMember("default", getDefault());
    }
  }
  if (profile.properties) {
    serializeMap(properties_, rj, "properties");
  }
}

/*
 * DataPieceArray<T> definitions
 */

template <typename T>
DataPieceArray<T>::DataPieceArray(const DataPiece::MakerBundle& bundle)
    : DataPieceArray(bundle.label, bundle.arraySize) {
  getJVector(defaultValues_, bundle.piece, "default");
  getJMap(properties_, bundle.piece, "properties");
}

template <typename T>
void DataPieceArray<T>::print(ostream& out, const string& indent) const {
  vector<T> values;
  bool hasValue = get(values);
  bool isDefault = !hasValue && !getDefault().empty();
  string str;
  str.reserve(200 + values.size() * 40);
  str.append(getLabel()).append(" (").append(getElementTypeName());
  str.append("[").append(to_string(count_)).append("]) @ ");
  if (getOffset() == DataLayout::kNotFound) {
    str.append("<unavailable>");
  } else {
    str.append(to_string(getOffset()));
  }
  str.append("+").append(to_string(getFixedSize()));
  if (isRequired()) {
    str.append(" required");
  }
  str.append(isDefault ? " (default): " : ": ");
  if (hasValue && !values.empty()) {
    for (const auto& value : values) {
      str.append(sprintValue<T>(value, {})).append(", ");
    }
    str.resize(str.size() - 2);
  }
  printTextWrapped(indent, str, indent + SUBINDENT, out);
  for (const auto& prop : properties_) {
    using namespace special_chars;
    out << indent << FIELDINDENT << prop.first << ": " << FORMAT(prop.second, out) << "\n";
  }
}

template <typename T>
void DataPieceArray<T>::printCompact(ostream& out, const string& indent) const {
  vector<T> values;
  bool hasValue = get(values);
  stringstream ss;
  for (const auto& value : values) {
    using namespace special_chars;
    ss << FORMAT(value, ss) << ", ";
  }
  string valuesStr = ss.str();
  if (!values.empty()) {
    valuesStr.resize(valuesStr.size() - 2);
  }
  string suffix = hasValue ? "" : " *";
  printTextFitted(
      fmt::format("{}{}[{}]: ", indent, getLabel(), getArraySize()), valuesStr, suffix, out);
}

template <typename T>
bool DataPieceArray<T>::isSame(const DataPiece* rhs) const {
  if (!DataPiece::isSame(rhs)) {
    return false;
  }
  const auto* other = reinterpret_cast<const DataPieceArray<T>*>(rhs);
  return vrs::isSame(this->defaultValues_, other->defaultValues_) &&
      vrs::isSame(this->properties_, other->properties_);
}

template <typename T>
void DataPieceArray<T>::serialize(JsonWrapper& rj, const JsonFormatProfileSpec& profile) {
  if (profile.value) {
    vector<T> values;
    if (get(values)) {
      serializeVector(values, rj, "value");
    }
  }
  DataPiece::serialize(rj, profile);
  if (profile.index) {
    rj.addMember("size", static_cast<vrs_rapidjson::SizeType>(count_));
  }
  if (profile.defaults) {
    serializeVector(defaultValues_, rj, "default");
  }
  if (profile.properties) {
    serializeMap(properties_, rj, "properties");
  }
}

/*
 * DataPieceVector<T> definitions
 */

template <typename T>
DataPieceVector<T>::DataPieceVector(const DataPiece::MakerBundle& bundle)
    : DataPieceVector(bundle.label) {
  vrs::getJVector(defaultValues_, bundle.piece, "default");
}

template <>
void DataPieceVector<string>::stage(const string* values, size_t count) {
  stagedValues_.resize(count);
  for (size_t s = 0; s < count; s++) {
    stagedValues_[s] = values[s];
  }
}

// Binary format:
// uint32_t number of strings (0 possible)
// For each string:
//   uint32_t number of char
//   char[number of char] string characters, not null-terminated
template <>
size_t DataPieceVector<string>::getVariableSize() const {
  size_t size = elementSize<uint32_t>(0);
  for (const string& s : stagedValues_) {
    size += elementSize<string>(s);
  }
  return size;
}

template <>
size_t DataPieceVector<string>::collectVariableData(int8_t* data, size_t bufferSize) {
  size_t writtenSize = 0;
  if (storeElement<uint32_t>(
          data, static_cast<uint32_t>(stagedValues_.size()), writtenSize, bufferSize)) {
    for (const string& s : stagedValues_) {
      if (!storeElement<string>(data, s, writtenSize, bufferSize)) {
        return writtenSize;
      }
    }
  }
  return writtenSize;
}

template <>
bool DataPieceVector<string>::get(vector<string>& outValues) const {
  size_t byteCount = 0;
  const int8_t* source = layout_.getVarData<int8_t>(offset_, byteCount);
  uint32_t vectorSize = 0;
  size_t readSize = 0;
  if (loadElement<uint32_t>(vectorSize, source, readSize, byteCount)) {
    if ((vectorSize + 1) * sizeof(uint32_t) <= byteCount) {
      outValues.resize(vectorSize);
      for (string& str : outValues) {
        if (!loadElement<string>(str, source, readSize, byteCount)) {
          outValues = defaultValues_;
          return false;
        }
      }
      return true;
    }
    XR_LOGW(
        "The size of the DataPieceVector<string> piece '{}' must be bogus: {} entries declared, "
        "which requires {} bytes at least, but we have only {} bytes...",
        getLabel(),
        vectorSize,
        (vectorSize + 1) * sizeof(uint32_t),
        byteCount);
  }
  outValues = defaultValues_;
  return false;
}

template <typename T>
void DataPieceVector<T>::print(ostream& out, const string& indent) const {
  vector<T> values;
  bool isDefault = !get(values);
  string str;
  str.reserve(200 + values.size() * 40);
  str.append(getLabel()).append(" (vector<").append(getElementTypeName()).append(">) @ ");
  if (getOffset() == DataLayout::kNotFound) {
    str.append("<unavailable>");
  } else {
    str.append(to_string(getOffset())).append("x").append(to_string(values.size()));
  }
  if (isRequired()) {
    str.append(" required");
  }
  str.append(isDefault ? " (default): " : ": ");
  if (!values.empty()) {
    for (const auto& value : values) {
      str.append(sprintValue<T>(value, {})).append(", ");
    }
    str.resize(str.size() - 2);
  }
  printTextWrapped(indent, str, indent + SUBINDENT, out);
}

template <>
void DataPieceVector<string>::print(ostream& out, const string& indent) const {
  vector<string> values;
  bool isDefault = !get(values);
  out << indent << getLabel() << " (vector<string>) @ ";
  if (getOffset() == DataLayout::kNotFound) {
    out << "<unavailable>";
  } else {
    out << getOffset() << "x" << values.size();
  }
  if (isRequired()) {
    out << " required";
  }
  out << (isDefault ? " (default):\n" : ":\n");
  if (!values.empty()) {
    string valuesStr;
    valuesStr.reserve(values.size() * 20);
    valuesStr.append(SUBINDENT);
    for (const auto& value : values) {
      valuesStr.append("\"").append(helpers::make_printable(value)).append("\", ");
    }
    if (!valuesStr.empty()) {
      valuesStr.resize(valuesStr.size() - 2);
    }
    printTextWrapped(indent, valuesStr, indent + SUBINDENT, out);
  }
}

template <typename T>
void DataPieceVector<T>::printCompact(ostream& out, const string& indent) const {
  if (getOffset() == DataLayout::kNotFound) {
    out << indent << getLabel() << ": " << "<unavailable>\n";
  } else {
    vector<T> values;
    get(values);
    stringstream ss;
    for (size_t k = 0; k < min<size_t>(kPrintCompactMaxVectorValues, values.size()); k++) {
      using namespace special_chars;
      ss << FORMAT(values[k], ss) << ", ";
    }
    string valuesStr = ss.str();
    if (!valuesStr.empty()) {
      valuesStr.resize(valuesStr.size() - 2);
    }
    printTextWrapped(
        fmt::format("{}{}[{}]: ", indent, getLabel(), values.size()),
        valuesStr,
        indent + SUBINDENT,
        out);
    if (values.size() > kPrintCompactMaxVectorValues) {
      out << indent << SUBINDENT "...and " << values.size() - kPrintCompactMaxVectorValues
          << " more values.";
    }
  }
}

template <>
void DataPieceVector<string>::printCompact(ostream& out, const string& indent) const {
  if (getOffset() == DataLayout::kNotFound) {
    out << indent << getLabel() << ": " << "<unavailable>\n";
  } else {
    vector<string> values;
    get(values);
    stringstream ss;
    for (size_t k = 0; k < min<size_t>(kPrintCompactMaxVectorValues, values.size()); k++) {
      ss << '"' << truncatedString(values[k]) << "\", ";
    }
    string valuesStr = ss.str();
    if (!valuesStr.empty()) {
      valuesStr.resize(valuesStr.size() - 2);
    }
    printTextWrapped(
        fmt::format("{}{}[{}]: ", indent, getLabel(), values.size()),
        valuesStr,
        indent + SUBINDENT,
        out);
    if (values.size() > kPrintCompactMaxVectorValues) {
      out << indent << SUBINDENT "...and " << values.size() - kPrintCompactMaxVectorValues
          << " more values.";
    }
  }
}

template <typename T>
bool DataPieceVector<T>::isSame(const DataPiece* rhs) const {
  if (!DataPiece::isSame(rhs)) {
    return false;
  }
  const DataPieceVector<T>& other = reinterpret_cast<const DataPieceVector<T>&>(*rhs);
  return vrs::isSame(this->defaultValues_, other.defaultValues_);
}

template <typename T>
void DataPieceVector<T>::serialize(JsonWrapper& rj, const JsonFormatProfileSpec& profile) {
  if (profile.value) {
    vector<T> values;
    if (get(values)) {
      serializeVector(values, rj, "value");
    }
  }
  DataPiece::serialize(rj, profile);
  if (profile.defaults) {
    serializeVector(defaultValues_, rj, "default");
  }
}

/*
 * DataPieceStringMap<T> definitions
 */

template <typename T>
DataPieceStringMap<T>::DataPieceStringMap(const DataPiece::MakerBundle& bundle)
    : DataPieceStringMap(bundle.label) {
  vrs::getJMap(defaultValues_, bundle.piece, "default");
}

template <typename T>
size_t DataPieceStringMap<T>::getVariableSize() const {
  size_t size = 0;
  for (const auto& iter : stagedValues_) {
    size += elementSize<string>(iter.first) + elementSize<T>(iter.second);
  }
  return size;
}

template <typename T>
size_t DataPieceStringMap<T>::collectVariableData(int8_t* data, size_t bufferSize) {
  size_t writtenSize = 0;
  for (const auto& iter : stagedValues_) {
    if (!storeElement<string>(data, iter.first, writtenSize, bufferSize) ||
        !storeElement<T>(data, iter.second, writtenSize, bufferSize)) {
      break;
    }
  }
  return writtenSize;
}

template <typename T>
bool DataPieceStringMap<T>::get(map<string, T>& outValues) const {
  outValues.clear();
  size_t dataSize = 0;
  const int8_t* ptr = layout_.getVarData<int8_t>(offset_, dataSize);
  size_t readSize = 0;
  if (ptr != nullptr && dataSize > 0) {
    while (readSize < dataSize) {
      string key;
      T value;
      if (loadElement<string>(key, ptr, readSize, dataSize) &&
          loadElement<T>(value, ptr, readSize, dataSize)) {
        outValues[key] = value;
      } else {
        // some reading error occurred: stop reading...
        if (pieceIndex_ != DataLayout::kNotFound) {
          return true;
        }
        outValues = defaultValues_;
        return false;
      }
    }
    return true;
  }
  if (pieceIndex_ != DataLayout::kNotFound) {
    return true;
  }
  outValues = defaultValues_;
  return false;
}

template <typename T>
void DataPieceStringMap<T>::print(ostream& out, const string& indent) const {
  out << indent << getLabel() << " (stringMap<" << getElementTypeName() << ">) @ ";
  map<string, T> values;
  bool isDefault = !get(values);
  if (getOffset() == DataLayout::kNotFound) {
    out << "<unavailable>";
  } else {
    out << getOffset() << "x" << values.size();
  }
  if (isRequired()) {
    out << " required";
  }
  if (values.empty()) {
    out << "\n";
  } else {
    out << (isDefault ? " (default):\n" : ":\n");
    string valuesStr;
    valuesStr.reserve(200);
    string indent2 = indent + SUBINDENT;
    string indent3 = indent2 + SUBINDENT;
    for (auto& iter : values) {
      valuesStr.clear();
      valuesStr.append("\"").append(iter.first).append("\": ");
      if (std::is_same_v<T, string>) {
        valuesStr.append("\"").append(sprintValue(iter.second, {})).append("\"");
      } else {
        valuesStr.append(sprintValue(iter.second, getLabel()));
      }
      printTextWrapped(indent2, valuesStr, indent3, out);
    }
  }
}

template <typename T>
void DataPieceStringMap<T>::printCompact(ostream& out, const string& indent) const {
  out << indent << getLabel();
  map<string, T> values;
  bool isDefault = !get(values);
  out << "[" << values.size() << "]" << (isDefault ? " default" : "") << ":\n";
  for (auto& iter : values) {
    printTextFitted(
        fmt::format("{}    \"{}\": ", indent, iter.first),
        sprintValue(iter.second, getLabel()),
        {},
        out);
  }
}

template <>
void DataPieceStringMap<string>::printCompact(ostream& out, const string& indent) const {
  out << indent << getLabel();
  map<string, string> values;
  bool isDefault = !get(values);
  out << "[" << values.size() << "]" << (isDefault ? " default" : "") << ":\n";
  const size_t width = os::getTerminalWidth();
  for (auto& iter : values) {
    printStringFitted(
        fmt::format("{}    \"{}\": ", indent, iter.first), iter.second, {}, out, width);
  }
}

template <typename T>
bool DataPieceStringMap<T>::isSame(const DataPiece* rhs) const {
  if (!DataPiece::isSame(rhs)) {
    return false;
  }
  const auto* other = reinterpret_cast<const DataPieceStringMap<T>*>(rhs);
  return vrs::isSame(this->defaultValues_, other->defaultValues_);
}

template <typename T>
void DataPieceStringMap<T>::serialize(JsonWrapper& rj, const JsonFormatProfileSpec& profile) {
  if (profile.value) {
    map<string, T> values;
    if (get(values)) {
      serializeMap(values, rj, "value");
    }
  }
  DataPiece::serialize(rj, profile);
  if (profile.defaults) {
    serializeMap(defaultValues_, rj, "default");
  }
}

/*
 * DataPieceString definitions
 */

DataPieceString::DataPieceString(const DataPiece::MakerBundle& bundle)
    : DataPieceString(bundle.label) {
  using namespace vrs_rapidjson;
  JValue::ConstMemberIterator defaultString = bundle.piece.FindMember("default");
  if (defaultString != bundle.piece.MemberEnd() && defaultString->value.IsString()) {
    defaultString_ = defaultString->value.GetString();
  }
}

const string& DataPieceString::getElementTypeName() const {
  static const string sName("string");
  return sName;
}

size_t DataPieceString::collectVariableData(int8_t* data, size_t bufferSize) {
  const size_t writtenSize = min(bufferSize, getVariableSize());
  if (writtenSize > 0) {
    memcpy(data, stagedString_.data(), writtenSize);
  }
  return writtenSize;
}

string DataPieceString::get() const {
  size_t size = 0;
  const char* ptr = layout_.getVarData<char>(offset_, size);
  return ptr != nullptr ? string(ptr, size) : defaultString_;
}

bool DataPieceString::get(string& outString) const {
  size_t size = 0;
  const char* ptr = layout_.getVarData<char>(offset_, size);
  if (ptr != nullptr) {
    outString.resize(0);
    outString.append(ptr, size);
    return true;
  }
  if (pieceIndex_ != DataLayout::kNotFound) {
    outString.clear();
    return true;
  }
  outString = defaultString_;
  return false;
}

bool DataPieceString::isAvailable() const {
  size_t count = 0;
  return layout_.getVarData<char>(offset_, count) != nullptr;
}

void DataPieceString::print(ostream& out, const string& indent) const {
  string value = helpers::make_printable(get());
  string str;
  str.reserve(100 + value.size());
  str.append(getLabel()).append(" (string) @ ");
  if (getOffset() == DataLayout::kNotFound) {
    str.append("<unavailable>");
  } else {
    str.append(to_string(getOffset()));
  }
  if (isRequired()) {
    str.append(" required");
  }
  str.append((isAvailable() ? "" : " (default)")).append(" = \"").append(value).append("\"");
  printTextWrapped(indent, str, indent + SUBINDENT, out);
}

void DataPieceString::printCompact(ostream& out, const string& indent) const {
  const string suffix = (getOffset() == DataLayout::kNotFound) ? "<unavailable>" : "";
  printStringFitted(fmt::format("{}{}: ", indent, getLabel()), get(), suffix, out);
}

bool DataPieceString::isSame(const DataPiece* rhs) const {
  if (!DataPiece::isSame(rhs)) {
    return false;
  }
  const DataPieceString& other = reinterpret_cast<const DataPieceString&>(*rhs);
  return defaultString_ == other.defaultString_;
}

void DataPieceString::serialize(JsonWrapper& rj, const JsonFormatProfileSpec& profile) {
  DataPiece::serialize(rj, profile);
  if (profile.value) {
    string value;
    if (get(value)) {
      rj.addMember("value", value);
    }
  }
  if (profile.defaults && !defaultString_.empty()) {
    rj.addMember("default", defaultString_);
  }
}

AutoDataLayout::AutoDataLayout() {
  internal::DataLayouter::get().dataLayoutBegin(*this);
}

AutoDataLayoutEnd::AutoDataLayoutEnd() {
  internal::DataLayouter::get().dataLayoutEnd();
}

ManualDataLayout::ManualDataLayout() {
  internal::DataLayouter::get().dataLayoutBegin(*this);
  layoutInProgress_ = true;
}

ManualDataLayout::ManualDataLayout(const DataLayout& existingLayout) {
  internal::DataLayouter::get().dataLayoutBegin(*this);
  layoutInProgress_ = true;
  existingLayout.forEachDataPiece([this](const DataPiece* piece) { this->add(piece->clone()); });
}

ManualDataLayout::~ManualDataLayout() {
  endLayout();
}

ManualDataLayout::ManualDataLayout(const string& json) : ManualDataLayout() {
  using namespace vrs_rapidjson;
  JDocument document;
  jParse(document, json);
  // We need to assume that everything might be missing, and never crash
  if (XR_VERIFY(document.IsObject(), "Not a valid datalayout: '{}'", json)) {
    JValue::ConstMemberIterator node = document.FindMember("data_layout");
    if (XR_VERIFY(
            node != document.MemberEnd() && node->value.IsArray(), "Missing data_layout object")) {
      for (const JValue& piece : node->value.GetArray()) {
        JValue::ConstMemberIterator name = piece.FindMember("name");
        JValue::ConstMemberIterator type = piece.FindMember("type");
        if (XR_VERIFY(name != piece.MemberEnd() && name->value.IsString(), "name missing") &&
            XR_VERIFY(type != piece.MemberEnd() && type->value.IsString(), "type missing")) {
          DataPiece::MakerBundle bundle(name->value.GetString(), piece);
          JValue::ConstMemberIterator count = piece.FindMember("size");
          if (count != piece.MemberEnd() && count->value.IsUint()) {
            bundle.arraySize = count->value.GetUint();
          }
          // This is where the magic happens!
          DataPiece* pc =
              add(vrs::internal::DataPieceFactory::makeDataPiece(type->value.GetString(), bundle));
          if (XR_VERIFY(pc, "Could not build DataLayout type {}", type->value.GetString())) {
            // every piece type supports tags & a required flag: do it here for all of them
            const JValue::ConstMemberIterator tags = piece.FindMember("tags");
            if (tags != piece.MemberEnd() && tags->value.IsObject()) {
              for (JValue::ConstMemberIterator itr = tags->value.MemberBegin();
                   itr != tags->value.MemberEnd();
                   ++itr) {
                if (itr->name.IsString() && itr->value.IsString()) {
                  pc->setTag(itr->name.GetString(), itr->value.GetString());
                }
              }
            }
            JValue::ConstMemberIterator required = piece.FindMember("required");
            if (required != piece.MemberEnd() && required->value.IsBool()) {
              pc->setRequired(required->value.GetBool());
            }
          }
        }
      }
    }
  }
  endLayout();
  initLayout();
}

DataPiece* ManualDataLayout::add(unique_ptr<DataPiece> piece) {
  DataPiece* dataPiece = piece.get();
  if (dataPiece != nullptr) {
    manualPieces.emplace_back(std::move(piece));
  }
  return dataPiece;
}

void ManualDataLayout::endLayout() {
  if (layoutInProgress_) {
    internal::DataLayouter::get().dataLayoutEnd();
    layoutInProgress_ = false;
  }
}

DataLayoutStruct::DataLayoutStruct(const string& structName) {
  internal::DataLayouter::get().dataLayoutStructStart(structName);
}

void DataLayoutStruct::dataLayoutStructEnd(const string& structName) {
  internal::DataLayouter::get().dataLayoutStructEnd(structName);
}

JsonFormatProfileSpec::JsonFormatProfileSpec(JsonFormatProfile profile) {
  if (profile == JsonFormatProfile::ExternalCompact ||
      profile == JsonFormatProfile::ExternalPretty || profile == JsonFormatProfile::Public) {
    publicNames = profile == JsonFormatProfile::Public;
    prettyJson = profile == JsonFormatProfile::ExternalPretty;
    value = true;
    name = true;
    type = profile != JsonFormatProfile::Public;
    shortType = true;
    index = false;
    defaults = false;
    tags = false;
    properties = false;
    required = false;
  }
}

#define STR(x) #x
#define XSTR(x) STR(x)
#define REGISTER_TEMPLATE(TEMPLATE_CLASS, TEMPLATE_TYPE)                            \
  static vrs::internal::DataPieceFactory::Registerer<TEMPLATE_CLASS<TEMPLATE_TYPE>> \
      Registerer_##TEMPLATE_CLASS##_##TEMPLATE_TYPE(                                \
          STR(TEMPLATE_CLASS), getTypeName<TEMPLATE_TYPE>());

#define DEFINE_FIND_DATA_PIECE(T)                                                                 \
  template const DataPieceValue<T>* DataLayout::findDataPieceValue<T>(const string& label) const; \
  template DataPieceValue<T>* DataLayout::findDataPieceValue<T>(const string& label);             \
  template const DataPieceArray<T>* DataLayout::findDataPieceArray(                               \
      const string& label, size_t arraySize) const;                                               \
  template DataPieceArray<T>* DataLayout::findDataPieceArray(                                     \
      const string& label, size_t arraySize);                                                     \
  template const DataPieceVector<T>* DataLayout::findDataPieceVector(const string& label) const;  \
  template DataPieceVector<T>* DataLayout::findDataPieceVector(const string& label);              \
  template const DataPieceStringMap<T>* DataLayout::findDataPieceStringMap(const string& label)   \
      const;                                                                                      \
  template DataPieceStringMap<T>* DataLayout::findDataPieceStringMap(const string& label);

#define DEFINE_DATA_PIECE_TYPE(x)          \
  template <>                              \
  const string& getTypeName<x>() {         \
    static const string sName = XSTR(x);   \
    return sName;                          \
  }                                        \
  REGISTER_TEMPLATE(DataPieceValue, x)     \
  REGISTER_TEMPLATE(DataPieceArray, x)     \
  REGISTER_TEMPLATE(DataPieceVector, x)    \
  REGISTER_TEMPLATE(DataPieceStringMap, x) \
  DEFINE_FIND_DATA_PIECE(x)                \
  template class DataPieceValue<x>;        \
  template class DataPieceArray<x>;        \
  template class DataPieceVector<x>;       \
  template class DataPieceStringMap<x>;

// Define & generate the code for each POD type supported.
#define POD_MACRO DEFINE_DATA_PIECE_TYPE
#include <vrs/helpers/PODMacro.inc>

template class DataPieceVector<string>;
template class DataPieceStringMap<string>;

} // namespace vrs
