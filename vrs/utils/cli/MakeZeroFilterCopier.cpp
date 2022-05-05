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

#include "CliParsing.h"

#include <vrs/helpers/Strings.h>

using namespace std;

namespace vrs::utils {

namespace {

class ZeroFilter : public RecordFilterCopier {
 public:
  ZeroFilter(
      vrs::RecordFileReader& fileReader,
      vrs::RecordFileWriter& fileWriter,
      vrs::StreamId id,
      const CopyOptions& copyOptions)
      : RecordFilterCopier(fileReader, fileWriter, id, copyOptions) {}

  bool shouldCopyVerbatim(const CurrentRecord& record) override {
    auto tupleId = tuple<Record::Type, uint32_t>(record.recordType, record.formatVersion);
    const auto& verbatimCopyIter = verbatimCopy_.find(tupleId);
    if (verbatimCopyIter != verbatimCopy_.end()) {
      return verbatimCopyIter->second;
    }
    const auto& decoders = readers_.find(tuple<StreamId, Record::Type, uint32_t>(
        record.streamId, record.recordType, record.formatVersion));
    bool verbatimCopy = decoders == readers_.end() ||
        (record.recordType == Record::Type::DATA &&
         decoders->second.recordFormat.getBlocksOfTypeCount(ContentType::IMAGE) == 0 &&
         decoders->second.recordFormat.getBlocksOfTypeCount(ContentType::AUDIO) == 0);
    verbatimCopy_[tupleId] = verbatimCopy;
    return verbatimCopy;
  }
  void filterImage(const CurrentRecord&, size_t, const ContentBlock&, vector<uint8_t>& pixels)
      override {
    if (!pixels.empty()) {
      memset(pixels.data(), 0, pixels.size());
    }
  }
  void filterAudio(const CurrentRecord&, size_t, const ContentBlock&, vector<uint8_t>& audioSamples)
      override {
    if (!audioSamples.empty()) {
      memset(audioSamples.data(), 0, audioSamples.size());
    }
  }

 protected:
  map<tuple<Record::Type, uint32_t>, bool> verbatimCopy_;
};

} // namespace

unique_ptr<StreamPlayer> makeZeroFilterCopier(
    RecordFileReader& fileReader,
    RecordFileWriter& fileWriter,
    StreamId streamId,
    const CopyOptions& copyOptions) {
  return make_unique<ZeroFilter>(fileReader, fileWriter, streamId, copyOptions);
}

} // namespace vrs::utils
