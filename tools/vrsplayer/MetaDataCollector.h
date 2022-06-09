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

using ::vrs::Record;

class MetaDataCollector {
 public:
  void clearDescription() {
    std::unique_lock<std::mutex> guard(mutex_);
    descriptions_[Record::Type::CONFIGURATION].clear();
    descriptions_[Record::Type::STATE].clear();
    descriptions_[Record::Type::DATA].clear();
  }
  void setDescription(Record::Type recordType, size_t blockIndex, const std::string& description) {
    QString str = QString::fromStdString(description);
    std::unique_lock<std::mutex> guard(mutex_);
    descriptions_[recordType][blockIndex] = str;
  }
  QString getDescription(Record::Type type) const {
    QString description;
    if (!descriptions_.empty()) {
      std::unique_lock<std::mutex> guard(mutex_);
      auto descriptions = descriptions_.find(type);
      if (descriptions != descriptions_.end()) {
        for (const auto& d : descriptions->second) {
          description += d.second;
        }
      }
    }
    return description;
  }
  std::map<size_t, QString> getDescriptions(Record::Type recordType) {
    std::unique_lock<std::mutex> guard(mutex_);
    return descriptions_[recordType];
  }
  void setDescriptions(Record::Type recordType, const std::map<size_t, QString>& descriptions) {
    std::unique_lock<std::mutex> guard(mutex_);
    descriptions_[recordType] = descriptions;
  }

 private:
  mutable std::mutex mutex_;
  // Each record type can have multiple metadata blocks
  std::map<Record::Type, std::map<size_t, QString>> descriptions_;
};

} // namespace vrsp
