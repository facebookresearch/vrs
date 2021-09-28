// Copyright (c) Facebook, Inc. and its affiliates. Confidential and proprietary.

#include "Recordable.h"
#include "DataLayout.h"
#include "LegacyFormatsProvider.h"

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
  return RecordFormatRegistrar::addRecordFormat(
      tags_.vrs, recordType, formatVersion, format, layouts);
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
  static std::mutex sMutex_;
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
