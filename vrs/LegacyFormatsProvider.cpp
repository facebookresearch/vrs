// Copyright (c) Facebook, Inc. and its affiliates. Confidential and proprietary.

#include "LegacyFormatsProvider.h"

#include "DataLayout.h"
#include "DataPieces.h"

#include <algorithm>

#define DEFAULT_LOG_CHANNEL "LegacyFormatsProvider"
#include <logging/Log.h>

namespace vrs {

LegacyFormatsProvider::~LegacyFormatsProvider() = default;

void RecordFormatRegistrar::registerProvider(unique_ptr<LegacyFormatsProvider> provider) {
  getInstance().providers_.emplace_back(move(provider));
}

RecordFormatRegistrar& RecordFormatRegistrar::getInstance() {
  static RecordFormatRegistrar sInstance;
  return sInstance;
}

void RecordFormatRegistrar::getLegacyRecordFormats(
    RecordableTypeId id,
    RecordFormatMap& outFormats) {
  getRecordFormats(getLegacyRegistry(id), outFormats);
}

unique_ptr<DataLayout> RecordFormatRegistrar::getLegacyDataLayout(const ContentBlockId& blockId) {
  return getDataLayout(getLegacyRegistry(blockId.getRecordableTypeId()), blockId);
}

unique_ptr<DataLayout> RecordFormatRegistrar::getLatestDataLayout(
    RecordableTypeId typeId,
    Record::Type recordType) {
  RecordFormatMap recordFormats;
  getLegacyRecordFormats(typeId, recordFormats);
  // The newest version is assumed to have a greater version number, so we iterate backwards
  for (auto iter = recordFormats.rbegin(); iter != recordFormats.rend(); ++iter) {
    if (iter->first.first == recordType) {
      const RecordFormat& format = iter->second;
      size_t block = format.getUsedBlocksCount();
      // start from the back, as the first DataLayout blocks are deemed less relevant (arbitrary).
      while (block-- > 0) { // carefully iterate backwards with an unsigned index
        if (format.getContentBlock(block).getContentType() == ContentType::DATA_LAYOUT) {
          unique_ptr<DataLayout> layout = getDataLayout(
              legacyRecordFormats_[typeId], {typeId, recordType, iter->first.second, block});
          if (layout) {
            return layout;
          }
        }
      }
    }
  }
  return {};
}

bool RecordFormatRegistrar::addRecordFormat(
    map<std::string, std::string>& inOutRecordFormatRegister,
    Record::Type recordType,
    uint32_t formatVersion,
    const RecordFormat& format,
    const vector<const DataLayout*>& layouts) {
  inOutRecordFormatRegister[RecordFormat::getRecordFormatTagName(recordType, formatVersion)] =
      format.asString();
  for (size_t index = 0; index < layouts.size(); ++index) {
    const DataLayout* layout = layouts[index];
    if (layout != nullptr) {
      inOutRecordFormatRegister[RecordFormat::getDataLayoutTagName(
          recordType, formatVersion, index)] = layout->asJson();
    }
  }
  bool allGood = true;
  // It's too easy to tell in RecordFormat that you're using a DataLayout,
  // and not specify that DataLayout (or at the wrong index). Let's warn the VRS user!
  size_t usedBlocks = format.getUsedBlocksCount();
  size_t maxIndex = std::max<size_t>(usedBlocks, layouts.size());
  for (size_t index = 0; index < maxIndex; ++index) {
    if (index < usedBlocks &&
        format.getContentBlock(index).getContentType() == ContentType::DATA_LAYOUT) {
      if (index >= layouts.size() || layouts[index] == nullptr) {
        XR_LOGE(
            "Missing DataLayout definition for Type:{}, FormatVersion:{}, Block #{}",
            toString(recordType),
            formatVersion,
            index);
        allGood = false;
      }
    } else if (index < layouts.size() && layouts[index] != nullptr) {
      XR_LOGE(
          "DataLayout definition provided from non-DataLayout block. "
          "Type: {}, FormatVersion:{}, Layout definition index:{}",
          toString(recordType),
          formatVersion,
          index);
      allGood = false;
    }
  }
  return allGood;
}

void RecordFormatRegistrar::getRecordFormats(
    const map<std::string, std::string>& recordFormatRegister,
    RecordFormatMap& outFormats) {
  for (const auto& tag : recordFormatRegister) {
    Record::Type recordType;
    uint32_t formatVersion;
    if (RecordFormat::parseRecordFormatTagName(tag.first, recordType, formatVersion) &&
        outFormats.find({recordType, formatVersion}) == outFormats.end()) {
      outFormats[{recordType, formatVersion}].set(tag.second);
    }
  }
}

unique_ptr<DataLayout> RecordFormatRegistrar::getDataLayout(
    const map<std::string, std::string>& recordFormatRegister,
    const ContentBlockId& blockId) {
  string tagName = RecordFormat::getDataLayoutTagName(
      blockId.getRecordType(), blockId.getFormatVersion(), blockId.getBlockIndex());
  const auto iter = recordFormatRegister.find(tagName);
  if (iter != recordFormatRegister.end()) {
    return DataLayout::makeFromJson(iter->second);
  }
  return nullptr;
}

const map<std::string, std::string>& RecordFormatRegistrar::getLegacyRegistry(
    RecordableTypeId typeId) {
  if (legacyRecordFormats_.find(typeId) == legacyRecordFormats_.end()) {
    for (auto& provider : providers_) {
      provider->registerLegacyRecordFormats(typeId);
    }
  }
  return legacyRecordFormats_[typeId];
}

} // namespace vrs
