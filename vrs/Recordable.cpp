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

#include "Recordable.h"
#include "DataLayout.h"
#include "RecordFormat.h"

#define DEFAULT_LOG_CHANNEL "Recordable"
#include <logging/Checks.h>
#include <logging/Log.h>

using namespace std;

namespace vrs {

Recordable::Recordable(RecordableTypeId typeId, const string& flavor)
    : typeId_(typeId), instanceId_(getNewInstanceId(typeId)), isActive_(true) {
  XR_CHECK(
      !(isARecordableClass(typeId) && flavor.empty()),
      "Recordable flavor required when using Recordable Class Ids such as {}",
      toString(typeId));
  if (!flavor.empty()) {
    tags_.vrs[getFlavorTagName()] = flavor;
  }
  tags_.vrs[getOriginalNameTagName()] = toString(typeId);
}

Recordable::~Recordable() = default;

void Recordable::setCompression(CompressionPreset preset) {
  recordManager_.setCompression(preset);
}

bool Recordable::addRecordFormat(
    Record::Type recordType,
    uint32_t formatVersion,
    const RecordFormat& format,
    vector<const DataLayout*> layouts) {
  return RecordFormat::addRecordFormat(tags_.vrs, recordType, formatVersion, format, layouts);
}

void Recordable::setTag(const string& tagName, const string& tagValue) {
  tags_.user[tagName] = tagValue;
}

void Recordable::addTags(const map<string, string>& newTags) {
  for (auto tag : newTags) {
    tags_.user[tag.first] = tag.second;
  }
}

void Recordable::addTags(const StreamTags& tags) {
  for (auto tag : tags.user) {
    tags_.user[tag.first] = tag.second;
  }
  for (auto tag : tags.vrs) {
    tags_.vrs[tag.first] = tag.second;
  }
}

void Recordable::resetNewInstanceIds() {
  getNewInstanceId(static_cast<RecordableTypeId>(0)); // Magic value
}

uint16_t Recordable::getNewInstanceId(RecordableTypeId typeId) {
  static mutex sMutex_;
  static map<RecordableTypeId, uint16_t> sInstanceIds;

  unique_lock<mutex> guard{sMutex_};
  // Magic to reset instance id generation
  if (typeId == static_cast<RecordableTypeId>(0)) {
    sInstanceIds.clear();
    return 0;
  }
  uint16_t instanceId = 1; // default instance Id
  auto newId = sInstanceIds.find(typeId);
  if (newId == sInstanceIds.end()) {
    sInstanceIds[typeId] = instanceId;
  } else {
    instanceId = ++(newId->second);
  }
  return instanceId;
}

} // namespace vrs
