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

find_package(QT NAMES Qt5 COMPONENTS Core Gui Widgets)
if (QT_FOUND)
  message(STATUS "Found Qt${QT_VERSION_MAJOR}, including vrsplayer target")
  find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core Gui Widgets)

  # We only need PortAudio for vrsplayer, so we only search for if Qt was found
  find_package(PortAudio REQUIRED)

  set(CMAKE_AUTOMOC ON)

  file(GLOB VRSPLAYER_SRCS *.cpp *.h)
  add_executable(vrsplayer ${VRSPLAYER_SRCS})
  target_include_directories(vrsplayer
    PRIVATE
      ${CMAKE_CURRENT_SOURCE_DIR}
  )
  target_link_libraries(vrsplayer
    PRIVATE
      vrslib
      vrs_utils
      fmt::fmt
      vrs_logging
      PortAudio::PortAudio
      Qt${QT_VERSION_MAJOR}::Core
      Qt${QT_VERSION_MAJOR}::Gui
      Qt${QT_VERSION_MAJOR}::Widgets
      vrs_logging
  )

  install(TARGETS vrsplayer EXPORT VRSLibTargets
    RUNTIME DESTINATION bin
  )

  set(CMAKE_AUTOMOC OFF)
else()
  message(WARNING "Qt5 or Qt6 were not found, so vrsplayer will not be included.")
endif()
