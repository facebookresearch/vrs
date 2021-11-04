// Copyright (c) Facebook, Inc. and its affiliates. Confidential and proprietary.

#include "TestDataDir.h"

#include <vrs/os/Utils.h>

namespace coretech {

std::string getTestDataDir() {
  return vrs::os::pathJoin(VRS_SOURCE_DIR, "../../projects/xrtech/test_data") + "/";
}

} // namespace coretech
