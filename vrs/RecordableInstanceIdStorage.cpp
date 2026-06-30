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

#include <vrs/RecordableInstanceIdStorage.h>

using namespace std;

namespace vrs {

recursive_mutex& getInstanceIdMutex() {
  // NOLINTNEXTLINE(facebook-thread-safety-analysis)
  static recursive_mutex sMutex;
  return sMutex;
}

map<RecordableTypeId, uint16_t>& getInstanceIdMap() {
  static map<RecordableTypeId, uint16_t> sInstanceIds;
  return sInstanceIds;
}

} // namespace vrs
