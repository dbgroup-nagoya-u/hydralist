cmake_minimum_required(VERSION 3.11)

# decline in-source build
if("${CMAKE_SOURCE_DIR}" STREQUAL "${CMAKE_BINARY_DIR}")
  message(FATAL_ERROR "In-source builds are not allowed.")
endif()

# project information
project(
  hydralist
  VERSION "0.1.0"
  DESCRIPTION "A forked library of cosmoss-jigu/hydralist to use it as a C++ library."
  LANGUAGES "CXX"
)

# define source directories
set(HYDRALIST_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")

#--------------------------------------------------------------------------------------#
# Configure external libraries
#--------------------------------------------------------------------------------------#

add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/external/art")

#--------------------------------------------------------------------------------------#
# Configure CPU information if needed
#--------------------------------------------------------------------------------------#

if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/include/numa-config.h")
  message(NOTICE "[hydralist] The CPUs' NUMA setting is collected.")
  execute_process(
    COMMAND python3 "${CMAKE_CURRENT_SOURCE_DIR}/tools/cpu-topology.py"
    OUTPUT_FILE "${CMAKE_CURRENT_SOURCE_DIR}/include/numa-config.h"
  )
endif()

#--------------------------------------------------------------------------------------#
# Build targets
#--------------------------------------------------------------------------------------#

if(NOT TARGET hydralist::hydralist)
  add_library(hydralist STATIC)
  add_library(hydralist::hydralist ALIAS hydralist)
  target_compile_features(hydralist PUBLIC
    "cxx_std_17"
  )
  target_compile_options(hydralist PUBLIC
    -mavx512f
    -mavx512bw
    $<$<STREQUAL:${CMAKE_BUILD_TYPE},"Release">:"-O2 -march=native">
    $<$<STREQUAL:${CMAKE_BUILD_TYPE},"RelWithDebInfo">:"-g3 -Og -pg">
    $<$<STREQUAL:${CMAKE_BUILD_TYPE},"Debug">:"-g3 -O0 -pg">
  )
  target_include_directories(hydralist PUBLIC
    "${HYDRALIST_SOURCE_DIR}/include"
  )
  target_link_libraries(hydralist PUBLIC
    pthread
    numa
    art
  )

  add_subdirectory(src)
endif()

#--------------------------------------------------------------------------------------#
# Build unit tests if required
#--------------------------------------------------------------------------------------#

option(HYDRALIST_BUILD_TESTS, "Building unit tests for this library if ON." OFF)
if(${HYDRALIST_BUILD_TESTS})
  # enable CTest
  enable_testing()
  # include unit tests
  add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/test")
endif()
