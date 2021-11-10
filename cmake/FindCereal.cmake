# Copyright (c) Facebook, Inc. and its affiliates.

find_path(CEREAL_INCLUDE_DIR cereal/cereal.hpp)
mark_as_advanced(CEREAL_INCLUDE_DIR)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(
    Cereal
    DEFAULT_MSG
    CEREAL_INCLUDE_DIR)

if (Cereal_FOUND)
  add_library(Cereal::Cereal INTERFACE IMPORTED)
  set_target_properties(
    Cereal::Cereal PROPERTIES
    INTERFACE_INCLUDE_DIRECTORIES "${CEREAL_INCLUDE_DIR}")
  target_compile_definitions(Cereal::Cereal
    INTERFACE
      CEREAL_THREAD_SAFE=1
      CEREAL_RAPIDJSON_NAMESPACE=fb_rapidjson
      CEREAL_RAPIDJSON_PARSE_DEFAULT_FLAGS=kParseFullPrecisionFlag|kParseNanAndInfFlag
      CEREAL_RAPIDJSON_WRITE_DEFAULT_FLAGS=kWriteNoFlags)
endif()
