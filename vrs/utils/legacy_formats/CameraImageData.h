// Copyright (c) Facebook, Inc. and its affiliates. Confidential and proprietary.

#pragma once

#include <vrs/os/Platform.h>

#if IS_WINDOWS_PLATFORM()
#include <DML/Vision/DML_CameraTypes.h>
#else // !IS_WINDOWS_PLATFORM()
#include <vrs/utils/legacy_formats/MobileCameraImageData.h>
#endif

namespace ctlegacy {

#if IS_WINDOWS_PLATFORM()

using CameraImageData = OVR::DML::CameraImageData;

#else // !IS_WINDOWS_PLATFORM()

using CameraImageData = MobileCameraImageData;

#endif // !IS_WINDOWS_PLATFORM()

} // namespace ctlegacy
