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

#include <vrs/test/cross_so_instance_id/repro_shared_lib.h>

namespace vrs::test {
namespace {

class SharedLibRecordable : public ::vrs::Recordable {
 public:
  SharedLibRecordable()
      : ::vrs::Recordable(::vrs::RecordableTypeId::SensorRecordableClass, "cross_so_shared_lib") {}

  const ::vrs::Record* createConfigurationRecord() override {
    return nullptr;
  }
  const ::vrs::Record* createStateRecord() override {
    return nullptr;
  }
};

} // namespace

std::shared_ptr<::vrs::Recordable> makeSharedLibRecordable() {
  return std::make_shared<SharedLibRecordable>();
}

} // namespace vrs::test
