// Copyright (c) Facebook, Inc. and its affiliates. Confidential and proprietary.

#pragma once

#include <memory>

#include "RecordFormat.h"

namespace vrs {

using std::map;
using std::string;
using std::unique_ptr;
using std::vector;

class DataLayout;
class LegacyFormatsProvider;

/// Utility class to handle record format registry manipulations
class RecordFormatRegistrar {
  /// Private constructor, since there must be only one instance.
  RecordFormatRegistrar() {}

 public:
  // Public interface

  /// Register a provider. Do this before reading a VRS file.
  /// @param provider: The provider to register.
  static void registerProvider(unique_ptr<LegacyFormatsProvider> provider);

  // VRS internal interface

  /// Access the legacy format registry's singleton.
  /// @return RecordFormatRegistrar singleton.
  static RecordFormatRegistrar& getInstance();

  /// Use all the registered providers to get the RecordFormat definitions for a specific
  /// RecordableTypeId.
  /// @param id: The RecordableTypeId to provider the formats for.
  /// @param outFormats: Map to be set the RecordFormat definitions.
  void getLegacyRecordFormats(RecordableTypeId id, RecordFormatMap& outFormats);

  /// Provide DataLayout definitions for a specific ContentBlockId.
  /// @param blockId: Specific content block. @see ContentBlockId.
  /// @return DataLayout definition, or nullptr, if none can be provided.
  unique_ptr<DataLayout> getLegacyDataLayout(const ContentBlockId& blockId);

  /// Get the newest legacy DataLayout definition for a recordable type id & record type.
  /// "Latest" makes the assumption, generally true but not garanteed, that record versions are
  /// numeric values increasing over time.
  /// Also, if the "most recent" RecordFormat definition includes multiple DataLayout blocks,
  /// the last one is returned, which is also arbitrary, but generaly what's needed.
  /// These approximations are OK, because this API is meant to dig out legacy DataLayout
  /// definitions that include metadata definitions, such as unit & description, mix & max values,
  /// to be used for UI purposes when the DataLayout definition found in a file doesn't provide
  /// that information. Therefore, approximate matches are better than nothing.
  unique_ptr<DataLayout> getLatestDataLayout(RecordableTypeId typeId, Record::Type recordType);

  /// VRS internal utility to add record format definitions to a container.
  /// This container might be the VRS tags for a Recordable, or a legacy registry.
  static bool addRecordFormat(
      map<string, string>& inOutRecordFormatRegister,
      Record::Type recordType,
      uint32_t formatVersion,
      const RecordFormat& format,
      const vector<const DataLayout*>& layouts);

  static void getRecordFormats(
      const map<string, string>& recordFormatRegister,
      RecordFormatMap& outFormats);

  static unique_ptr<DataLayout> getDataLayout(
      const map<string, string>& recordFormatRegister,
      const ContentBlockId& blockId);

  /// VRS internal method to register a legacy record format. Do not call directly.
  /// @internal
  bool addLegacyRecordFormat(
      RecordableTypeId typeId,
      Record::Type recordType,
      uint32_t formatVersion,
      const RecordFormat& format,
      const vector<const DataLayout*>& layouts) {
    return addRecordFormat(
        legacyRecordFormats_[typeId], recordType, formatVersion, format, layouts);
  }

 private:
  const map<string, string>& getLegacyRegistry(RecordableTypeId typeId);

  vector<unique_ptr<LegacyFormatsProvider>> providers_;
  map<RecordableTypeId, map<string, string>> legacyRecordFormats_;
};

/// Older VRS files written before DataLayout do not include RecordFormat & DataLayout definitions,
/// of course, but often enough, records can easily be described with the right DataLayout
/// definition, which allows an easier transition to DataLayout, as client code can be updated to
/// use RecordFormatStreamPlayer to read older and newer VRS files alike.
///
/// For this, you need to inject RecordFormat and DataLayout definitions using this class.
class LegacyFormatsProvider {
 public:
  virtual ~LegacyFormatsProvider();

  /// Method to implement to provide legacy definitions for a specific device type.
  /// When this method is called, call the addRecordFormat() method with the definitions.
  /// @param typeId: the recordable type to provide record format definitions for.
  virtual void registerLegacyRecordFormats(RecordableTypeId typeId) = 0;

 protected:
  /// Method a LegacyFormatsProvider calls to add a legacy record format definition.
  /// The signature is identical to Recordable::addRecordFormat(), except for the RecordableTypeId,
  /// which is implicit in the context of a Recordable.
  /// Attention! when you provide a RecordFormat for a record type & formatVersion,
  /// all of the streams' records of that type & formatVersion must comply with that RecordFormat.
  /// @param recordType: The Record::Type to define.
  /// @param formatVersion: The format version to define.
  /// @param format: The RecordFormat for the records of the type and format version.
  /// @param layouts: A vector of pointers to DataLayouts and nullptr. For each DataLayout content
  /// block of the RecordFormat, a pointer to a DataLayout must be provided for the matching index.
  /// @return True if the RecordFormat and the layouts match as expected. Otherwise, false is
  /// returned and errors will be logged to help debug the problem.
  bool addLegacyRecordFormat(
      RecordableTypeId typeId,
      Record::Type recordType,
      uint32_t formatVersion,
      const RecordFormat& format,
      const vector<const DataLayout*>& layouts) {
    return RecordFormatRegistrar::getInstance().addLegacyRecordFormat(
        typeId, recordType, formatVersion, format, layouts);
  }
};

} // namespace vrs
