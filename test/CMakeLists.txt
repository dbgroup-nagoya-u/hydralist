#--------------------------------------------------------------------------------------#
# Configure GoogleTest
#--------------------------------------------------------------------------------------#

configure_file(CMakeLists-googletest.txt.in googletest-download/CMakeLists.txt)
execute_process(COMMAND ${CMAKE_COMMAND} -G "${CMAKE_GENERATOR}" .
  RESULT_VARIABLE result
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/googletest-download )
if(result)
  message(FATAL_ERROR "CMake step for googletest failed: ${result}")
endif()
execute_process(COMMAND ${CMAKE_COMMAND} --build .
  RESULT_VARIABLE result
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/googletest-download )
if(result)
  message(FATAL_ERROR "Build step for googletest failed: ${result}")
endif()

# Prevent overriding the parent project's compiler/linker
# settings on Windows
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)

# Add googletest directly to our build. This defines
# the gtest and gtest_main targets.
add_subdirectory(${CMAKE_CURRENT_BINARY_DIR}/googletest-src
                 ${CMAKE_CURRENT_BINARY_DIR}/googletest-build
                 EXCLUDE_FROM_ALL)

#--------------------------------------------------------------------------------------#
# Configurations for testing
#--------------------------------------------------------------------------------------#

set(HYDRALIST_TEST_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
set(HYDRALIST_TEST_THREAD_NUM "8" CACHE STRING "The number of worker threads for testing.")
set(HYDRALIST_TEST_RANDOM_SEED "10" CACHE STRING "A fixed seed value to reproduce unit tests.")
set(HYDRALIST_TEST_EXEC_NUM "1E5" CACHE STRING "The number of executions per a thread.")
option(HYDRALIST_TEST_BUILD_MULTI_THREAD_TESTS "Building multi-threading tests if ON." ON)
option(HYDRALIST_TEST_OVERRIDE_MIMALLOC "Override entire memory allocation with mimalloc" OFF)

#--------------------------------------------------------------------------------------#
# Build Unit Tests
#--------------------------------------------------------------------------------------#

if(${HYDRALIST_TEST_OVERRIDE_MIMALLOC})
  find_package(mimalloc 1.7 REQUIRED)
endif()

# define function to add unit tests in the same format
function(ADD_HYDRALIST_TEST HYDRALIST_TEST_TARGET)
  add_executable(${HYDRALIST_TEST_TARGET}
    "${CMAKE_CURRENT_SOURCE_DIR}/${HYDRALIST_TEST_TARGET}.cpp"
  )
  target_compile_features(${HYDRALIST_TEST_TARGET} PRIVATE
    "cxx_std_17"
  )
  target_compile_definitions(${HYDRALIST_TEST_TARGET} PRIVATE
    HYDRALIST_TEST_THREAD_NUM=${HYDRALIST_TEST_THREAD_NUM}
    HYDRALIST_TEST_RANDOM_SEED=${HYDRALIST_TEST_RANDOM_SEED}
    HYDRALIST_TEST_EXEC_NUM=${HYDRALIST_TEST_EXEC_NUM}
    INDEX_FIXTURE_THREAD_NUM=${HYDRALIST_TEST_THREAD_NUM}
    INDEX_FIXTURE_RANDOM_SEED=${HYDRALIST_TEST_RANDOM_SEED}
    INDEX_FIXTURE_EXEC_NUM_PER_THREAD=${HYDRALIST_TEST_EXEC_NUM}
  )
  target_compile_options(${HYDRALIST_TEST_TARGET} PRIVATE
    -Wall
    -Wextra
    $<$<STREQUAL:${CMAKE_BUILD_TYPE},"Release">:"-O2 -march=native">
    $<$<STREQUAL:${CMAKE_BUILD_TYPE},"RelWithDebInfo">:"-g3 -Og -pg">
    $<$<STREQUAL:${CMAKE_BUILD_TYPE},"Debug">:"-g3 -O0 -pg">
  )
  # target_include_directories(${HYDRALIST_TEST_TARGET} PRIVATE
  #   "${HYDRALIST_SOURCE_DIR}/src"
  # )
  target_link_libraries(${HYDRALIST_TEST_TARGET} PRIVATE
    hydralist
    gtest_main
    $<$<BOOL:${HYDRALIST_TEST_OVERRIDE_MIMALLOC}>:mimalloc-static>
  )
  add_test(NAME ${HYDRALIST_TEST_TARGET} COMMAND $<TARGET_FILE:${HYDRALIST_TEST_TARGET}>)
endfunction()

# add unit tests to build targets
ADD_HYDRALIST_TEST("linked_list_test")
ADD_HYDRALIST_TEST("hydralist_test")
