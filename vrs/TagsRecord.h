// Copyright (c) Facebook, Inc. and its affiliates. Confidential and proprietary.

#pragma once

#include <functional>
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
