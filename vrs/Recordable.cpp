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

#include <vrs/os/System.h>

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
  tags_.vrs[getSerialNumberTagName()] = os::getUniqueSessionId();
}

Recordable::~Recordable() = default;

void Recordable::setCompression(CompressionPreset preset) {
  recordManager_.setCompression(preset);
}

bool Recordable::addRecordFormat(
    Record::Type recordType,
    uint32_t formatVersion,
    const RecordFormat& format,
    const vector<const DataLayout*>& layouts) {
  return RecordFormat::addRecordFormat(tags_.vrs, recordType, formatVersion, format, layouts);
}

void Recordable::setTag(const string& tagName, const string& tagValue) {
  tags_.user[tagName] = tagValue;
}

void Recordable::addTags(const map<string, string>& newTags) {
  for (const auto& tag : newTags) {
    tags_.user[tag.first] = tag.second;
  }
}

void Recordable::addTags(const StreamTags& tags) {
  for (const auto& tag : tags.user) {
    tags_.user[tag.first] = tag.second;
  }
  for (const auto& tag : tags.vrs) {
    tags_.vrs[tag.first] = tag.second;
  }
}

static recursive_mutex& getInstanceIdMutex() {
  static recursive_mutex sMutex;
  return sMutex;
}

static map<RecordableTypeId, uint16_t>& getInstanceIds() {
  static map<RecordableTypeId, uint16_t> sInstanceIds;
  return sInstanceIds;
}

void Recordable::resetNewInstanceIds() {
  unique_lock<recursive_mutex> guard{getInstanceIdMutex()};
  getInstanceIds().clear();
}

uint16_t Recordable::getNewInstanceId(RecordableTypeId typeId) {
  unique_lock<recursive_mutex> guard{getInstanceIdMutex()};
  map<RecordableTypeId, uint16_t>& instanceIds = getInstanceIds();
  uint16_t instanceId = 1; // default instance Id
  auto newId = instanceIds.find(typeId);
  if (newId == instanceIds.end()) {
    instanceIds[typeId] = instanceId;
  } else {
    instanceId = ++(newId->second);
  }
  return instanceId;
}

const string& Recordable::getTag(const map<string, string>& tags, const string& name) {
  auto iter = tags.find(name);
  if (iter != tags.end()) {
    return iter->second;
  }
  static const string sEmptyString;
  return sEmptyString;
}

TemporaryRecordableInstanceIdsResetter::TemporaryRecordableInstanceIdsResetter()
    : lock_{getInstanceIdMutex()} {
  preservedState_.swap(getInstanceIds());
}

TemporaryRecordableInstanceIdsResetter::~TemporaryRecordableInstanceIdsResetter() {
  preservedState_.swap(getInstanceIds());
}

} // namespace vrs
