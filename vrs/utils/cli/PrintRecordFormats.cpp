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

#include "PrintRecordFormats.h"

#include <sstream>

using namespace std;
using namespace vrs;
using namespace vrs::utils;

namespace vrs::utils {

string printRecordFormats(FilteredFileReader& filteredReader) {
  stringstream ss;
  JsonFormatProfileSpec spec;
  spec.prettyJson = true;
  spec.shortType = true;
  spec.index = false;
  spec.defaults = false;
  spec.tags = false;
  spec.properties = false;
  spec.required = false;
  for (auto id : filteredReader.filter.streams) {
    RecordFormatMap formats;
    filteredReader.reader.getRecordFormats(id, formats);
    for (auto& iter : formats) {
      if (filteredReader.filter.types.find(iter.first.first) != filteredReader.filter.types.end()) {
        RecordFormat& format = iter.second;
        ss << id.getNumericName() << " " << id.getName() << " " << toString(iter.first.first)
           << " v" << iter.first.second << ": " << format.asString() << endl;
        for (size_t block = 0; block < format.getUsedBlocksCount(); block++) {
          if (format.getContentBlock(block).getContentType() == ContentType::DATA_LAYOUT) {
            ContentBlockId blockId(id.getTypeId(), iter.first.first, iter.first.second, block);
            unique_ptr<DataLayout> dl = filteredReader.reader.getDataLayout(id, blockId);
            if (dl) {
              ss << "Content block " << block << ": " << dl->asJson(spec) << endl;
            } else {
              ss << "<no DataLayout>" << endl;
            }
          }
        }
      }
    }
  }
  return ss.str();
}

} // namespace vrs::utils
