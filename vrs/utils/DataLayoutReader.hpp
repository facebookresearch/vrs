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

#include <set>

#include <vrs/RecordFileReader.h>
#include <vrs/RecordFormatStreamPlayer.h>

namespace vrs::utils {

/// Helper class to read a particular DataLayout type conveniently.
/// VRS uses the callback pattern to receive content parts of a record being read.
/// While this is a powerful pattern, in practice, it makes it more complicated to do simple things,
/// like merely reading the DataLayout of a configuration record, and nothing else.
/// This helper template solves that problem and demonstrates how to turn a callback interface into
/// a function interface, albeit in a less flexible and a bit more expensive way.
/// In particular: only DataLayout parts are read with this code, because, in particular, there are
/// too many ways to handle images to make generic templates. If needed, this code can easily be
/// specialized further to handle a particular type of images.
///
/// Warning: While this helper is appropriate to fetch a few records, in particular configuration
/// records that might be required to setup playback, it won't give you everything and it assumes
/// that the first matching DataLayout that "has all the required pieces" is what's required.

template <class T>
class DataLayoutReader : public RecordFormatStreamPlayer {
 public:
  DataLayoutReader(RecordFileReader& fileReader) : fileReader_{fileReader} {}

  /// Read a record, and return a pointer to the first DataLayout that maps to the requested type
  /// @param streamId: stream to read from
  /// @param recordType: record type within that stream
  /// @param indexNumber: record index within that stream and record type
  /// @return A pointer to the DataLayout of type requested, or nullptr, if no match was found.
  const T* read(StreamId streamId, Record::Type recordType, uint32_t indexNumber = 0) {
    auto* record = fileReader_.getRecord(streamId, recordType, indexNumber);
    if (record != nullptr) {
      return read(*record);
    }
    return nullptr;
  }
  /// Read a record, and return a pointer to the first DataLayout that maps to the requested type
  /// @param recordInfo: a record to read.
  /// @return A pointer to the DataLayout of type requested, or nullptr, if no match was found.
  const T* read(const IndexRecord::RecordInfo& recordInfo) {
    datalayoutRead_ = nullptr;
    // Explicit registration is required once when calling readRecord with a specific player object
    if (attachedTo_.insert(recordInfo.streamId).second) {
      onAttachedToFileReader(fileReader_, recordInfo.streamId);
    }
    fileReader_.readRecord(recordInfo, this);
    return datalayoutRead_;
  }

 protected:
  bool onDataLayoutRead(const CurrentRecord& record, size_t index, DataLayout& dl) override {
    const T& layout = getExpectedLayout<T>(dl, index);
    if (layout.hasAllRequiredPieces()) {
      datalayoutRead_ = &layout;
      return false;
    }
    return true;
  }

  RecordFileReader& fileReader_;
  std::set<StreamId> attachedTo_;
  const T* datalayoutRead_{};
};

} // namespace vrs::utils
