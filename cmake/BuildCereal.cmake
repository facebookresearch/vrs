# Copyright (c) Facebook, Inc. and its affiliates. Confidential and proprietary.

include(ExternalProject)

set(cereal_URL https://github.com/USCiLab/cereal.git)
set(cereal_BUILD ${CMAKE_CURRENT_BINARY_DIR}/cereal)
set(cereal_TAG 02eace19a99ce3cd564ca4e379753d69af08c2c8) # version 1.3.0

# Download Cereal
ExternalProject_Add(
  cereal
  PREFIX cereal
  GIT_REPOSITORY ${cereal_URL}
  GIT_TAG ${cereal_TAG}
  BUILD_IN_SOURCE 1
  BUILD_COMMAND ${CMAKE_COMMAND} --build . --config Release
  INSTALL_COMMAND ""
  CMAKE_CACHE_ARGS
    -DCMAKE_BUILD_TYPE:STRING=Release
    -DJUST_INSTALL_CEREAL:BOOL=ON
)

ExternalProject_Get_Property(cereal binary_dir)
set(CEREAL_BINARY_DIR ${binary_dir})
set(CEREAL_INCLUDE_DIRS "${CEREAL_BINARY_DIR}/include")
file(MAKE_DIRECTORY ${CEREAL_INCLUDE_DIRS})

add_library(Cereal::Cereal INTERFACE IMPORTED)
target_compile_definitions(Cereal::Cereal
  INTERFACE
    CEREAL_THREAD_SAFE=1
    CEREAL_RAPIDJSON_NAMESPACE=fb_rapidjson
    CEREAL_RAPIDJSON_PARSE_DEFAULT_FLAGS=kParseFullPrecisionFlag|kParseNanAndInfFlag
    CEREAL_RAPIDJSON_WRITE_DEFAULT_FLAGS=kWriteNoFlags)
add_dependencies(Cereal::Cereal cereal)

set_target_properties(Cereal::Cereal PROPERTIES
  INTERFACE_INCLUDE_DIRECTORIES ${CEREAL_INCLUDE_DIRS}
)
target_include_directories(Cereal::Cereal INTERFACE ${CEREAL_INCLUDE_DIRS})

message(STATUS "${CEREAL_INCLUDE_DIRS}")
