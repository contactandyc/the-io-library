# SPDX-FileCopyrightText: 2019–2025 Andy Curtis <contactandyc@gmail.com>
# SPDX-FileCopyrightText: 2024–2025 Knode.ai — technical questions: contact Andy (above)
# SPDX-License-Identifier: Apache-2.0

# CMakeLists.txt for tests
cmake_minimum_required(VERSION 3.20)

project(the_io_library_tests LANGUAGES C)

set(A_BUILD_VARIANT "debug" CACHE STRING
    "Variant to link via the_io_library::the_io_library (debug|memory|static|shared)")
set_property(CACHE A_BUILD_VARIANT PROPERTY STRINGS debug memory static shared)

option(A_ENABLE_COVERAGE "Enable code coverage instrumentation" OFF)

find_library(M_LIB m)

# ---- Test executables ----
set(TEST_EXECUTABLES "")
add_executable(test_io  src/test_io.c)

list(APPEND TEST_EXECUTABLES test_io)

set_target_properties(test_io PROPERTIES
  C_STANDARD 23
  C_STANDARD_REQUIRED YES
)
if("CXX" IN_LIST CMAKE_PROJECT_LANGUAGES)
  set_target_properties(test_io PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED YES
  )
endif()

if(NOT TARGET the_io_library::the_io_library)
  find_package(the_io_library CONFIG REQUIRED)
endif()
target_link_libraries(test_io PRIVATE the_io_library::the_io_library)

if(M_LIB)
  target_link_libraries(test_io PRIVATE ${M_LIB})
endif()

if(MSVC)
  target_compile_options(test_io PRIVATE /W4)
else()
  target_compile_options(test_io PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(A_ENABLE_COVERAGE)
  if (CMAKE_C_COMPILER_ID MATCHES "Clang")
    target_compile_options(test_io PRIVATE -O0 -g -fprofile-instr-generate -fcoverage-mapping)
    target_link_options(test_io PRIVATE -fprofile-instr-generate -fcoverage-mapping)
  elseif (CMAKE_C_COMPILER_ID STREQUAL "GNU")
    target_compile_options(test_io PRIVATE -O0 -g --coverage)
    target_link_options(test_io PRIVATE --coverage)
  endif()
endif()

add_test(NAME test_io COMMAND $<TARGET_FILE:test_io>)
add_executable(test_io_in  src/test_io_in.c)

list(APPEND TEST_EXECUTABLES test_io_in)

set_target_properties(test_io_in PROPERTIES
  C_STANDARD 23
  C_STANDARD_REQUIRED YES
)
if("CXX" IN_LIST CMAKE_PROJECT_LANGUAGES)
  set_target_properties(test_io_in PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED YES
  )
endif()

if(NOT TARGET the_io_library::the_io_library)
  find_package(the_io_library CONFIG REQUIRED)
endif()
target_link_libraries(test_io_in PRIVATE the_io_library::the_io_library)

if(M_LIB)
  target_link_libraries(test_io_in PRIVATE ${M_LIB})
endif()

if(MSVC)
  target_compile_options(test_io_in PRIVATE /W4)
else()
  target_compile_options(test_io_in PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(A_ENABLE_COVERAGE)
  if (CMAKE_C_COMPILER_ID MATCHES "Clang")
    target_compile_options(test_io_in PRIVATE -O0 -g -fprofile-instr-generate -fcoverage-mapping)
    target_link_options(test_io_in PRIVATE -fprofile-instr-generate -fcoverage-mapping)
  elseif (CMAKE_C_COMPILER_ID STREQUAL "GNU")
    target_compile_options(test_io_in PRIVATE -O0 -g --coverage)
    target_link_options(test_io_in PRIVATE --coverage)
  endif()
endif()

add_test(NAME test_io_in COMMAND $<TARGET_FILE:test_io_in>)
add_executable(test_io_out  src/test_io_out.c)

list(APPEND TEST_EXECUTABLES test_io_out)

set_target_properties(test_io_out PROPERTIES
  C_STANDARD 23
  C_STANDARD_REQUIRED YES
)
if("CXX" IN_LIST CMAKE_PROJECT_LANGUAGES)
  set_target_properties(test_io_out PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED YES
  )
endif()

if(NOT TARGET the_io_library::the_io_library)
  find_package(the_io_library CONFIG REQUIRED)
endif()
target_link_libraries(test_io_out PRIVATE the_io_library::the_io_library)

if(M_LIB)
  target_link_libraries(test_io_out PRIVATE ${M_LIB})
endif()

if(MSVC)
  target_compile_options(test_io_out PRIVATE /W4)
else()
  target_compile_options(test_io_out PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(A_ENABLE_COVERAGE)
  if (CMAKE_C_COMPILER_ID MATCHES "Clang")
    target_compile_options(test_io_out PRIVATE -O0 -g -fprofile-instr-generate -fcoverage-mapping)
    target_link_options(test_io_out PRIVATE -fprofile-instr-generate -fcoverage-mapping)
  elseif (CMAKE_C_COMPILER_ID STREQUAL "GNU")
    target_compile_options(test_io_out PRIVATE -O0 -g --coverage)
    target_link_options(test_io_out PRIVATE --coverage)
  endif()
endif()

add_test(NAME test_io_out COMMAND $<TARGET_FILE:test_io_out>)

enable_testing()

# ---- Coverage aggregation ----
add_custom_target(coverage_report COMMENT "Generate coverage report")

if(A_ENABLE_COVERAGE AND CMAKE_C_COMPILER_ID MATCHES "Clang")
  include(ProcessorCount)
  ProcessorCount(NPROC)
  if(NPROC EQUAL 0)
    set(NPROC 4)
  endif()

  add_custom_command(TARGET coverage_report PRE_BUILD
    COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure -j ${NPROC}
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMENT "Running tests with coverage instrumentation"
  )

  find_program(LLVM_PROFDATA_EXECUTABLE llvm-profdata)
  find_program(LLVM_COV_EXECUTABLE llvm-cov)
  if(LLVM_PROFDATA_EXECUTABLE AND LLVM_COV_EXECUTABLE)
    add_custom_command(TARGET coverage_report POST_BUILD
      COMMAND ${LLVM_PROFDATA_EXECUTABLE} merge -sparse *.profraw -o default.profdata
      WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
      COMMENT "Merging coverage data"
    )

    # Normal compiled library path
    add_custom_command(TARGET coverage_report POST_BUILD
      COMMAND ${LLVM_COV_EXECUTABLE} show
              ../libthe_io_library_${A_BUILD_VARIANT}.a
              -instr-profile=default.profdata
              -format=html -output-dir=coverage_html
              --ignore-filename-regex='/usr/.*'
              --show-regions --show-line-counts-or-regions
      WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
      COMMENT "Generated LLVM coverage report → open coverage_html/index.html"
    )
  endif()
endif()
