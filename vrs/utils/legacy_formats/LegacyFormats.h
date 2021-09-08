// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <vrs/LegacyFormatsProvider.h>

namespace OVR {
namespace Vision {

class LegacyFormats : public ::vrs::LegacyFormatsProvider {
 public:
  static void install();

  void registerLegacyRecordFormats(::vrs::RecordableTypeId typeId) override;
};

} // namespace Vision
} // namespace OVR
