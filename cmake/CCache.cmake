# Copyright (c) Facebook, Inc. and its affiliates. Confidential and proprietary.

if (NOT BUILD_ON_WINDOWS)
  find_program(ccache_path ccache)
  if (ccache_path)
    message(STATUS "CCACHE enabled")
    set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE ${ccache_path})
    set_property(GLOBAL PROPERTY RULE_LAUNCH_LINK ${ccache_path})
  endif (ccache_path)
endif (NOT BUILD_ON_WINDOWS)
