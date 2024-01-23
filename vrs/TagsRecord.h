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

#include <limits>

#include "DataLayout.h"
#include "RecordFormatStreamPlayer.h"

namespace vrs {

/// \brief DataLayout definition used in tag records, which is a VRS internal record type.
struct TagsRecord : public AutoDataLayout {
  enum { kTagsVersion = 1 };

  constexpr static const double kTagsRecordTimestamp = std::numeric_limits<double>::lowest();

  DataPieceStringMap<string> vrsTags{"vrs_tags"};
  DataPieceStringMap<string> userTags{"user_tags"};

  AutoDataLayoutEnd end;
};

/// \brief StreamPlayer to decode the content of VRS tag records.
class TagsRecordPlayer : public RecordFormatStreamPlayer {
 public:
  TagsRecordPlayer(RecordFileReader* fileReader, map<StreamId, StreamTags>& streamTags);

  void prepareToReadTagsFor(StreamId id);

  bool onDataLayoutRead(const CurrentRecord& record, size_t blockIndex, DataLayout&) override;

 private:
  map<StreamId, StreamTags>& streamTags_;
  TagsRecord tags_;
};

} // namespace vrs
