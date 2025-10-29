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

#include <vrs/utils/cli/MakeZeroFilterCopier.h>

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
    const auto& decoders = readers_.find(
        tuple<StreamId, Record::Type, uint32_t>(
            record.streamId, record.recordType, record.formatVersion));
    bool verbatimCopy = decoders == readers_.end() ||
        (record.recordType == Record::Type::DATA &&
         decoders->second.recordFormat.getBlocksOfTypeCount(ContentType::IMAGE) == 0 &&
         decoders->second.recordFormat.getBlocksOfTypeCount(ContentType::AUDIO) == 0);
    verbatimCopy_[tupleId] = verbatimCopy;
    return verbatimCopy;
  }

  bool onImageRead(const CurrentRecord& rec, size_t idx, const ContentBlock& cb) override {
    return handleImageOrAudio(rec, idx, cb);
  }

  bool onAudioRead(const CurrentRecord& rec, size_t idx, const ContentBlock& cb) override {
    return handleImageOrAudio(rec, idx, cb);
  }

  bool handleImageOrAudio(const CurrentRecord& rec, size_t idx, const ContentBlock& cb) {
    size_t blockSize = cb.getBlockSize();
    if (blockSize == ContentBlock::kSizeUnknown) {
      return onUnsupportedBlock(rec, idx, cb);
    }
    if (blockSize == rec.reader->getUnreadBytes()) {
      // This is the last content block: we can avoid reading/decoding it
      unique_ptr<ContentBlockChunk> chunk =
          make_unique<ContentBlockChunk>(cb, vector<uint8_t>(blockSize));
      chunks_.emplace_back(std::move(chunk));
      return false;
    }
    // very rare in practice: we need to read the data, so we can read what's after it
    unique_ptr<ContentBlockChunk> chunk = make_unique<ContentBlockChunk>(cb, rec);
    auto& buffer = chunk->getBuffer();
    if (!buffer.empty()) {
      memset(buffer.data(), 0, buffer.size());
    }
    chunks_.emplace_back(std::move(chunk));
    return true;
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
