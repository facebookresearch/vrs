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

#include <map>
#include <mutex>

#include <qstring.h>

#include <vrs/Record.h>

namespace vrsp {

using namespace vrs;

using std::map;
using std::mutex;
using std::unique_lock;

using vrs::Record;

class MetaDataCollector {
 public:
  void clearDescription() {
    unique_lock<mutex> guard(mutex_);
    descriptions_[Record::Type::CONFIGURATION].clear();
    descriptions_[Record::Type::STATE].clear();
    descriptions_[Record::Type::DATA].clear();
  }
  void setDescription(Record::Type recordType, size_t blockIndex, const string& description) {
    QString str = QString::fromStdString(description);
    unique_lock<mutex> guard(mutex_);
    descriptions_[recordType][blockIndex] = str;
  }
  QString getDescription(Record::Type type) const {
    QString description;
    if (!descriptions_.empty()) {
      unique_lock<mutex> guard(mutex_);
      auto descriptions = descriptions_.find(type);
      if (descriptions != descriptions_.end()) {
        for (const auto& d : descriptions->second) {
          description += d.second;
        }
      }
    }
    return description;
  }
  map<size_t, QString> getDescriptions(Record::Type recordType) {
    unique_lock<mutex> guard(mutex_);
    return descriptions_[recordType];
  }
  void setDescriptions(Record::Type recordType, const map<size_t, QString>& descriptions) {
    unique_lock<mutex> guard(mutex_);
    descriptions_[recordType] = descriptions;
  }

 private:
  mutable mutex mutex_;
  // Each record type can have multiple metadata blocks
  map<Record::Type, map<size_t, QString>> descriptions_;
};

} // namespace vrsp
