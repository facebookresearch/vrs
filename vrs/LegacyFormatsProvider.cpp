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

#include "LegacyFormatsProvider.h"

#include "DataLayout.h"
#include "DataPieces.h"

#include <algorithm>

#define DEFAULT_LOG_CHANNEL "LegacyFormatsProvider"
#include <logging/Log.h>

using namespace std;

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
  RecordFormat::getRecordFormats(getLegacyRegistry(id), outFormats);
}

unique_ptr<DataLayout> RecordFormatRegistrar::getLegacyDataLayout(const ContentBlockId& blockId) {
  return RecordFormat::getDataLayout(getLegacyRegistry(blockId.getRecordableTypeId()), blockId);
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
          unique_ptr<DataLayout> layout = RecordFormat::getDataLayout(
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

const map<string, string>& RecordFormatRegistrar::getLegacyRegistry(RecordableTypeId typeId) {
  if (legacyRecordFormats_.find(typeId) == legacyRecordFormats_.end()) {
    for (auto& provider : providers_) {
      provider->registerLegacyRecordFormats(typeId);
    }
  }
  return legacyRecordFormats_[typeId];
}

} // namespace vrs
