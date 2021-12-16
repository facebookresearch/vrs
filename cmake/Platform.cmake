# Copyright (c) Meta Platforms, Inc. and affiliates.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

# Support external Android toolchain file
if ("${CMAKE_SYSTEM_NAME}" STREQUAL "Android")
  set(ANDROID true)
endif()

# Cache the ANDROID flag
set(ANDROID "${ANDROID}" CACHE BOOL "Build for Android. (DO NOT EDIT).")
mark_as_advanced(FORCE ANDROID) # Hide this flag in ccmake & qtcreator: it can't be toogled from the UI

# Cache the IOS flag
set(IOS "${IOS}" CACHE BOOL "Build for iOS. (DO NOT EDIT).")
mark_as_advanced(FORCE IOS) # Hide this flag in ccmake & qtcreator: it can't be toogled from the UI

if (${CMAKE_HOST_WIN32})
  set(BUILD_ON_WINDOWS true)
  set(BUILD_ON_NAME Windows)
  if (ANDROID)
    set(TARGET_ANDROID true)
    message(FATAL_ERROR "Targetting Android from Windows not supported")
  else()
    set(TARGET_WINDOWS true)
  endif()

elseif (${CMAKE_HOST_UNIX})
  if (${CMAKE_HOST_APPLE})
    set(BUILD_ON_MACOS true)
    set(BUILD_ON_NAME MacOS)
    if (ANDROID)
      set(TARGET_ANDROID true)
    else()
      set(TARGET_MACOS true)
    endif()

  else()
    set(BUILD_ON_LINUX true)
    set(BUILD_ON_NAME Linux)
    if (ANDROID)
      set(TARGET_ANDROID true)
    else()
      set(TARGET_LINUX true)
    endif()
  endif()

else()
  message(FATAL_ERROR "Build platform not recognized.")
endif()

if (TARGET_ANDROID)
  set(TARGET_NAME Android)
else()
  set(TARGET_NAME "${BUILD_ON_NAME}")
endif()
