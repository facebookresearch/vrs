// Copyright (c) Meta Platforms, Inc. and affiliates.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "TagConventions.h"

#include <vrs/helpers/Rapidjson.hpp>
#include <vrs/os/System.h>

using namespace std;

namespace vrs {

void tag_conventions::addCaptureTime(RecordFileWriter& writer) {
  writer.setTag(kCaptureTimeEpoch, to_string(time(nullptr)));
}

void tag_conventions::addOsFingerprint(RecordFileWriter& writer) {
  writer.setTag(kOsFingerprint, os::getOsFingerPrint());
}

void tag_conventions::addTagSet(RecordFileWriter& writer, const vector<string>& tags) {
  // Tags are saved in a json message following the model:
  // "{ "tags": [ "tag1", "tag2"... ] }"
  writer.setTag(kTagSet, makeTagSet(tags));
}

static const char* cTagsObjectName = "tags";

string tag_conventions::makeTagSet(const vector<string>& tags) {
  using namespace fb_rapidjson;
  JDocument doc;
  doc.SetObject();
  JsonWrapper wrapper{doc, doc.GetAllocator()};
  serializeVector<string>(tags, wrapper, fb_rapidjson::StringRef(cTagsObjectName));
  return jDocumentToJsonString(doc);
}

bool tag_conventions::parseTagSet(const string& jsonTagSet, vector<string>& outVectorTagSet) {
  outVectorTagSet.clear();
  using namespace fb_rapidjson;
  JDocument document;
  document.Parse(jsonTagSet.c_str());
  if (document.IsObject()) {
    getVector(outVectorTagSet, document, fb_rapidjson::StringRef(cTagsObjectName));
    return true;
  }
  return false;
}

string tag_conventions::addUniqueSessionId(RecordFileWriter& writer) {
  string sessionId = os::getUniqueSessionId();
  writer.setTag(kSessionId, sessionId);
  return sessionId;
}

} // namespace vrs
