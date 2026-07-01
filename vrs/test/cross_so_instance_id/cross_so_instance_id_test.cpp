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

#include <gtest/gtest.h>

#include <vrs/Recordable.h>
#include <vrs/test/cross_so_instance_id/repro_shared_lib.h>

namespace vrs::test {
namespace {

class MainRecordable : public ::vrs::Recordable {
 public:
  MainRecordable()
      : ::vrs::Recordable(::vrs::RecordableTypeId::SensorRecordableClass, "cross_so_main") {}

  const ::vrs::Record* createConfigurationRecord() override {
    return nullptr;
  }
  const ::vrs::Record* createStateRecord() override {
    return nullptr;
  }
};

} // namespace

// MainRecordable is built in the test binary; makeSharedLibRecordable() builds its Recordable in a
// separate .so. Both modules dynamically link the single shared VRS library (vrs_shared), so the
// loader provides one copy of VRS's instance-id counter and the two Recordables' StreamIds do not
// collide — purely by linking one shared VRS, with no static absorption and no runtime setup.
TEST(CrossSoInstanceId, StreamIdsAreUniqueAcrossSharedLibraries) {
  MainRecordable mainRecordable;
  auto sharedLibRecordable = makeSharedLibRecordable();
  EXPECT_NE(mainRecordable.getStreamId().getName(), sharedLibRecordable->getStreamId().getName());
}

} // namespace vrs::test
