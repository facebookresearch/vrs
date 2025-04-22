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

#include <vrs/os/Platform.h>

#if IS_LINUX_PLATFORM() || IS_MAC_PLATFORM() || IS_WINDOWS_PLATFORM() || IS_ANDROID_PLATFORM()

// Starting with boost 1.88.0, the default boost/process namespace is v2,
// but the code in this file is still using v1, which is the only option ATM in
// some environments. We can still use v1, but we need to include the v1 headers.

#include <boost/version.hpp>

#if BOOST_VERSION >= 108800
#include <boost/process/v1/io.hpp>
#include <boost/process/v1/system.hpp>
#if IS_WINDOWS_PLATFORM()
#include <boost/process/v1/windows.hpp>
#endif

#else
#include <boost/process.hpp>
#include <boost/process/io.hpp>
#include <boost/process/system.hpp>
#if IS_WINDOWS_PLATFORM()
#include <boost/process/windows.hpp>
#endif
#endif

namespace vrs {
namespace os {

#if BOOST_VERSION >= 108800
namespace process = boost::process::v1;
#else
namespace process = boost::process;
#endif

} // namespace os
} // namespace vrs

#endif // IS_LINUX_PLATFORM() || IS_MAC_PLATFORM() || IS_WINDOWS_PLATFORM()
