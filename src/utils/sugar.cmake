# This file generated automatically by:
#   generate_sugar_files.py
# see wiki for more info:
#   https://github.com/ruslo/sugar/wiki/Collecting-sources

if(DEFINED COMM_SCOPE_SRC_UTILS_SUGAR_CMAKE_)
  return()
else()
  set(COMM_SCOPE_SRC_UTILS_SUGAR_CMAKE_ 1)
endif()

include(sugar_files)

sugar_files(
    comm_HEADERS
    cache_control.hpp
    numa.hpp
    omp.hpp
)

sugar_files(
    comm_SOURCES
    omp.cpp
)

