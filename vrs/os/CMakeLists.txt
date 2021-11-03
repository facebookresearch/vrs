# Copyright (c) Facebook, Inc. and its affiliates. Confidential and proprietary.

file(GLOB VRS_OS_SRCS *.cpp *.h *.hpp)
# Remove fb-only source files (if there is still any lying around)
file(GLOB VRS_OS_FB_SRCS *_fb\.cpp *_fb\.h)
list(REMOVE_ITEM VRS_OS_SRCS ${VRS_OS_FB_SRCS})

add_library(vrs_os STATIC ${VRS_OS_SRCS})
target_include_directories(vrs_os PUBLIC ${VRS_SOURCE_DIR})
target_link_libraries(vrs_os
    PUBLIC
        vrs_platform
        vrs_logging
        Boost::system
        Boost::filesystem
        Boost::thread
        Boost::chrono
        Boost::date_time
)

add_library(vrs_platform INTERFACE)
target_include_directories(vrs_platform INTERFACE ${VRS_SOURCE_DIR})
if ("${OSS_CMAKE_OVERRIDE}")
  target_compile_definitions(vrs_platform INTERFACE OSS_CMAKE_OVERRIDE)
endif()

if (UNIT_TESTS)
  enable_testing()

  add_executable(os_test
    test/TimeTest.cpp
    test/EventUnitTest.cpp
#    test/UtilsTest.cpp
  )
  target_link_libraries(os_test
      PUBLIC
        vrs_os
        GTest::Main
  )

  gtest_discover_tests(os_test)
endif()
