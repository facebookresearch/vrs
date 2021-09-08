// Facebook Technologies, LLC Proprietary and Confidential.

#include "TagsRecord.h"

#define DEFAULT_LOG_CHANNEL "TagsRecord"
#include <logging/Log.h>

#include "DescriptionRecord.h"
#include "LegacyFormatsProvider.h"
#include "Recordable.h"

namespace vrs {

TagsRecordPlayer::TagsRecordPlayer(
    RecordFileReader* fileReader,
    map<StreamId, StreamTags>& streamTags)
    : streamTags_{streamTags} {
  recordFileReader_ = fileReader;
}

void TagsRecordPlayer::prepareToReadTagsFor(StreamId id) {
  // The TagsRecords format isn't known, and obviously, isn't written in the stream's tags, as we're
  // trying to read the tags of the stream right now!
  // So we inject the TagsRecords definition manually here!
  // If we ever change the TagsRecords definition, we will need to add *all* the versions here,
  // being extra careful to always change kTagsVersion each time!
  readers_[tuple<StreamId, Record::Type, uint32_t>(
               id, Record::Type::TAGS, TagsRecord::kTagsVersion)]
      .recordFormat = tags_.getContentBlock();
  RecordFormatRegistrar::addRecordFormat(
      streamTags_[id].vrs,
      Record::Type::TAGS,
      TagsRecord::kTagsVersion,
      tags_.getContentBlock(),
      {&tags_});
}

bool TagsRecordPlayer::onDataLayoutRead(const CurrentRecord& record, size_t, DataLayout& layout) {
  if (record.recordType == Record::Type::TAGS && tags_.mapLayout(layout)) {
    StreamTags& thisRecordableTags = streamTags_[record.streamId];
    tags_.userTags.get(thisRecordableTags.user);
    tags_.vrsTags.get(thisRecordableTags.vrs);
    XR_LOGD(
        "Read {} VRS tags and {} user tags for {}",
        thisRecordableTags.vrs.size(),
        thisRecordableTags.user.size(),
        record.streamId.getName());
    DescriptionRecord::upgradeStreamTags(thisRecordableTags);
  }
  return true;
}

} // namespace vrs
