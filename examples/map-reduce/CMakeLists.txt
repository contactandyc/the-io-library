# SPDX-FileCopyrightText: 2019–2026 Andy Curtis <contactandyc@gmail.com>
# SPDX-FileCopyrightText: 2024–2025 Knode.ai
# SPDX-License-Identifier: Apache-2.0
#
# Maintainer: Andy Curtis <contactandyc@gmail.com>

cmake_minimum_required(VERSION 3.20)

# A dynamically generated examples project.
project(the_io_library_map-reduce LANGUAGES C)

find_library(M_LIB m)

find_package(a_memory_library CONFIG REQUIRED)
find_package(the_macro_library CONFIG REQUIRED)
find_package(the_lz4_library CONFIG REQUIRED)
find_package(ZLIB REQUIRED)

# ---- Targets ----
add_executable(dump_files_1
  src/dump_files_1.c
)

target_include_directories(dump_files_1 PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/src
)

set_target_properties(dump_files_1 PROPERTIES
  C_STANDARD 23
  C_STANDARD_REQUIRED YES
)
if("CXX" IN_LIST CMAKE_PROJECT_LANGUAGES)
  set_target_properties(dump_files_1 PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED YES
  )
endif()

# Ensure examples running in library stacks link the main library!
if(NOT TARGET the_io_library::the_io_library)
  find_package(the_io_library CONFIG REQUIRED)
endif()
target_link_libraries(dump_files_1 PRIVATE the_io_library::the_io_library)

target_link_libraries(dump_files_1 PRIVATE
  a_memory_library::a_memory_library
  the_macro_library::the_macro_library
  the_lz4_library::the_lz4_library
  ZLIB::ZLIB
  the_io_library::the_io_library
)

if(M_LIB)
  target_link_libraries(dump_files_1 PRIVATE ${M_LIB})
endif()

if(MSVC)
  target_compile_options(dump_files_1 PRIVATE /W4)
else()
  target_compile_options(dump_files_1 PRIVATE -Wall -Wextra -Wpedantic)
endif()

install(TARGETS dump_files_1 RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
add_executable(dump_files_2
  src/dump_files_2.c
)

target_include_directories(dump_files_2 PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/src
)

set_target_properties(dump_files_2 PROPERTIES
  C_STANDARD 23
  C_STANDARD_REQUIRED YES
)
if("CXX" IN_LIST CMAKE_PROJECT_LANGUAGES)
  set_target_properties(dump_files_2 PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED YES
  )
endif()

# Ensure examples running in library stacks link the main library!
if(NOT TARGET the_io_library::the_io_library)
  find_package(the_io_library CONFIG REQUIRED)
endif()
target_link_libraries(dump_files_2 PRIVATE the_io_library::the_io_library)

target_link_libraries(dump_files_2 PRIVATE
  a_memory_library::a_memory_library
  the_macro_library::the_macro_library
  the_lz4_library::the_lz4_library
  ZLIB::ZLIB
  the_io_library::the_io_library
)

if(M_LIB)
  target_link_libraries(dump_files_2 PRIVATE ${M_LIB})
endif()

if(MSVC)
  target_compile_options(dump_files_2 PRIVATE /W4)
else()
  target_compile_options(dump_files_2 PRIVATE -Wall -Wextra -Wpedantic)
endif()

install(TARGETS dump_files_2 RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
add_executable(dump_files_3
  src/dump_files_3.c
)

target_include_directories(dump_files_3 PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/src
)

set_target_properties(dump_files_3 PROPERTIES
  C_STANDARD 23
  C_STANDARD_REQUIRED YES
)
if("CXX" IN_LIST CMAKE_PROJECT_LANGUAGES)
  set_target_properties(dump_files_3 PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED YES
  )
endif()

# Ensure examples running in library stacks link the main library!
if(NOT TARGET the_io_library::the_io_library)
  find_package(the_io_library CONFIG REQUIRED)
endif()
target_link_libraries(dump_files_3 PRIVATE the_io_library::the_io_library)

target_link_libraries(dump_files_3 PRIVATE
  a_memory_library::a_memory_library
  the_macro_library::the_macro_library
  the_lz4_library::the_lz4_library
  ZLIB::ZLIB
  the_io_library::the_io_library
)

if(M_LIB)
  target_link_libraries(dump_files_3 PRIVATE ${M_LIB})
endif()

if(MSVC)
  target_compile_options(dump_files_3 PRIVATE /W4)
else()
  target_compile_options(dump_files_3 PRIVATE -Wall -Wextra -Wpedantic)
endif()

install(TARGETS dump_files_3 RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
add_executable(dump_files_4
  src/dump_files_4.c
)

target_include_directories(dump_files_4 PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/src
)

set_target_properties(dump_files_4 PROPERTIES
  C_STANDARD 23
  C_STANDARD_REQUIRED YES
)
if("CXX" IN_LIST CMAKE_PROJECT_LANGUAGES)
  set_target_properties(dump_files_4 PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED YES
  )
endif()

# Ensure examples running in library stacks link the main library!
if(NOT TARGET the_io_library::the_io_library)
  find_package(the_io_library CONFIG REQUIRED)
endif()
target_link_libraries(dump_files_4 PRIVATE the_io_library::the_io_library)

target_link_libraries(dump_files_4 PRIVATE
  a_memory_library::a_memory_library
  the_macro_library::the_macro_library
  the_lz4_library::the_lz4_library
  ZLIB::ZLIB
  the_io_library::the_io_library
)

if(M_LIB)
  target_link_libraries(dump_files_4 PRIVATE ${M_LIB})
endif()

if(MSVC)
  target_compile_options(dump_files_4 PRIVATE /W4)
else()
  target_compile_options(dump_files_4 PRIVATE -Wall -Wextra -Wpedantic)
endif()

install(TARGETS dump_files_4 RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
add_executable(dump_files_5
  src/dump_files_5.c
)

target_include_directories(dump_files_5 PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/src
)

set_target_properties(dump_files_5 PROPERTIES
  C_STANDARD 23
  C_STANDARD_REQUIRED YES
)
if("CXX" IN_LIST CMAKE_PROJECT_LANGUAGES)
  set_target_properties(dump_files_5 PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED YES
  )
endif()

# Ensure examples running in library stacks link the main library!
if(NOT TARGET the_io_library::the_io_library)
  find_package(the_io_library CONFIG REQUIRED)
endif()
target_link_libraries(dump_files_5 PRIVATE the_io_library::the_io_library)

target_link_libraries(dump_files_5 PRIVATE
  a_memory_library::a_memory_library
  the_macro_library::the_macro_library
  the_lz4_library::the_lz4_library
  ZLIB::ZLIB
  the_io_library::the_io_library
)

if(M_LIB)
  target_link_libraries(dump_files_5 PRIVATE ${M_LIB})
endif()

if(MSVC)
  target_compile_options(dump_files_5 PRIVATE /W4)
else()
  target_compile_options(dump_files_5 PRIVATE -Wall -Wextra -Wpedantic)
endif()

install(TARGETS dump_files_5 RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
add_executable(dump_files_6
  src/dump_files_6.c
)

target_include_directories(dump_files_6 PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/src
)

set_target_properties(dump_files_6 PROPERTIES
  C_STANDARD 23
  C_STANDARD_REQUIRED YES
)
if("CXX" IN_LIST CMAKE_PROJECT_LANGUAGES)
  set_target_properties(dump_files_6 PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED YES
  )
endif()

# Ensure examples running in library stacks link the main library!
if(NOT TARGET the_io_library::the_io_library)
  find_package(the_io_library CONFIG REQUIRED)
endif()
target_link_libraries(dump_files_6 PRIVATE the_io_library::the_io_library)

target_link_libraries(dump_files_6 PRIVATE
  a_memory_library::a_memory_library
  the_macro_library::the_macro_library
  the_lz4_library::the_lz4_library
  ZLIB::ZLIB
  the_io_library::the_io_library
)

if(M_LIB)
  target_link_libraries(dump_files_6 PRIVATE ${M_LIB})
endif()

if(MSVC)
  target_compile_options(dump_files_6 PRIVATE /W4)
else()
  target_compile_options(dump_files_6 PRIVATE -Wall -Wextra -Wpedantic)
endif()

install(TARGETS dump_files_6 RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
add_executable(dump_files_7
  src/dump_files_7.c
)

target_include_directories(dump_files_7 PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/src
)

set_target_properties(dump_files_7 PROPERTIES
  C_STANDARD 23
  C_STANDARD_REQUIRED YES
)
if("CXX" IN_LIST CMAKE_PROJECT_LANGUAGES)
  set_target_properties(dump_files_7 PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED YES
  )
endif()

# Ensure examples running in library stacks link the main library!
if(NOT TARGET the_io_library::the_io_library)
  find_package(the_io_library CONFIG REQUIRED)
endif()
target_link_libraries(dump_files_7 PRIVATE the_io_library::the_io_library)

target_link_libraries(dump_files_7 PRIVATE
  a_memory_library::a_memory_library
  the_macro_library::the_macro_library
  the_lz4_library::the_lz4_library
  ZLIB::ZLIB
  the_io_library::the_io_library
)

if(M_LIB)
  target_link_libraries(dump_files_7 PRIVATE ${M_LIB})
endif()

if(MSVC)
  target_compile_options(dump_files_7 PRIVATE /W4)
else()
  target_compile_options(dump_files_7 PRIVATE -Wall -Wextra -Wpedantic)
endif()

install(TARGETS dump_files_7 RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
add_executable(list_files
  src/list_files.c
)

target_include_directories(list_files PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/src
)

set_target_properties(list_files PROPERTIES
  C_STANDARD 23
  C_STANDARD_REQUIRED YES
)
if("CXX" IN_LIST CMAKE_PROJECT_LANGUAGES)
  set_target_properties(list_files PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED YES
  )
endif()

# Ensure examples running in library stacks link the main library!
if(NOT TARGET the_io_library::the_io_library)
  find_package(the_io_library CONFIG REQUIRED)
endif()
target_link_libraries(list_files PRIVATE the_io_library::the_io_library)

target_link_libraries(list_files PRIVATE
  a_memory_library::a_memory_library
  the_macro_library::the_macro_library
  the_lz4_library::the_lz4_library
  ZLIB::ZLIB
  the_io_library::the_io_library
)

if(M_LIB)
  target_link_libraries(list_files PRIVATE ${M_LIB})
endif()

if(MSVC)
  target_compile_options(list_files PRIVATE /W4)
else()
  target_compile_options(list_files PRIVATE -Wall -Wextra -Wpedantic)
endif()

install(TARGETS list_files RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
