# SPDX-FileCopyrightText: 2019–2025 Andy Curtis <contactandyc@gmail.com>
# SPDX-FileCopyrightText: 2024–2025 Knode.ai — technical questions: contact Andy (above)
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20)

# A small "apps" project that builds example/demo binaries.
project(the-io-library_map-reduce LANGUAGES C)

# Optional math lib on POSIX
find_library(M_LIB m)

# Bring in suite-level dependencies derived by the renderer
find_package(the_io_library CONFIG REQUIRED)

# ---- Binaries ----
add_executable(dump_files_1  src/dump_files_1.c)

# Language standards
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

# Link the union of deps computed for this app context
target_link_libraries(dump_files_1 PRIVATE  the_io_library::the_io_library)

# Optional libm
if(M_LIB)
  target_link_libraries(dump_files_1 PRIVATE ${M_LIB})
endif()

# Warnings
if(MSVC)
  target_compile_options(dump_files_1 PRIVATE /W4)
else()
  target_compile_options(dump_files_1 PRIVATE -Wall -Wextra -Wpedantic)
endif()

# Install each app binary (handy for packaging examples)
install(TARGETS dump_files_1 RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
add_executable(dump_files_2  src/dump_files_2.c)

# Language standards
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

# Link the union of deps computed for this app context
target_link_libraries(dump_files_2 PRIVATE  the_io_library::the_io_library)

# Optional libm
if(M_LIB)
  target_link_libraries(dump_files_2 PRIVATE ${M_LIB})
endif()

# Warnings
if(MSVC)
  target_compile_options(dump_files_2 PRIVATE /W4)
else()
  target_compile_options(dump_files_2 PRIVATE -Wall -Wextra -Wpedantic)
endif()

# Install each app binary (handy for packaging examples)
install(TARGETS dump_files_2 RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
add_executable(dump_files_3  src/dump_files_3.c)

# Language standards
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

# Link the union of deps computed for this app context
target_link_libraries(dump_files_3 PRIVATE  the_io_library::the_io_library)

# Optional libm
if(M_LIB)
  target_link_libraries(dump_files_3 PRIVATE ${M_LIB})
endif()

# Warnings
if(MSVC)
  target_compile_options(dump_files_3 PRIVATE /W4)
else()
  target_compile_options(dump_files_3 PRIVATE -Wall -Wextra -Wpedantic)
endif()

# Install each app binary (handy for packaging examples)
install(TARGETS dump_files_3 RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
add_executable(dump_files_4  src/dump_files_4.c)

# Language standards
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

# Link the union of deps computed for this app context
target_link_libraries(dump_files_4 PRIVATE  the_io_library::the_io_library)

# Optional libm
if(M_LIB)
  target_link_libraries(dump_files_4 PRIVATE ${M_LIB})
endif()

# Warnings
if(MSVC)
  target_compile_options(dump_files_4 PRIVATE /W4)
else()
  target_compile_options(dump_files_4 PRIVATE -Wall -Wextra -Wpedantic)
endif()

# Install each app binary (handy for packaging examples)
install(TARGETS dump_files_4 RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
add_executable(dump_files_5  src/dump_files_5.c)

# Language standards
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

# Link the union of deps computed for this app context
target_link_libraries(dump_files_5 PRIVATE  the_io_library::the_io_library)

# Optional libm
if(M_LIB)
  target_link_libraries(dump_files_5 PRIVATE ${M_LIB})
endif()

# Warnings
if(MSVC)
  target_compile_options(dump_files_5 PRIVATE /W4)
else()
  target_compile_options(dump_files_5 PRIVATE -Wall -Wextra -Wpedantic)
endif()

# Install each app binary (handy for packaging examples)
install(TARGETS dump_files_5 RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
add_executable(dump_files_6  src/dump_files_6.c)

# Language standards
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

# Link the union of deps computed for this app context
target_link_libraries(dump_files_6 PRIVATE  the_io_library::the_io_library)

# Optional libm
if(M_LIB)
  target_link_libraries(dump_files_6 PRIVATE ${M_LIB})
endif()

# Warnings
if(MSVC)
  target_compile_options(dump_files_6 PRIVATE /W4)
else()
  target_compile_options(dump_files_6 PRIVATE -Wall -Wextra -Wpedantic)
endif()

# Install each app binary (handy for packaging examples)
install(TARGETS dump_files_6 RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
add_executable(dump_files_7  src/dump_files_7.c)

# Language standards
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

# Link the union of deps computed for this app context
target_link_libraries(dump_files_7 PRIVATE  the_io_library::the_io_library)

# Optional libm
if(M_LIB)
  target_link_libraries(dump_files_7 PRIVATE ${M_LIB})
endif()

# Warnings
if(MSVC)
  target_compile_options(dump_files_7 PRIVATE /W4)
else()
  target_compile_options(dump_files_7 PRIVATE -Wall -Wextra -Wpedantic)
endif()

# Install each app binary (handy for packaging examples)
install(TARGETS dump_files_7 RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
add_executable(list_files  src/list_files.c)

# Language standards
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

# Link the union of deps computed for this app context
target_link_libraries(list_files PRIVATE  the_io_library::the_io_library)

# Optional libm
if(M_LIB)
  target_link_libraries(list_files PRIVATE ${M_LIB})
endif()

# Warnings
if(MSVC)
  target_compile_options(list_files PRIVATE /W4)
else()
  target_compile_options(list_files PRIVATE -Wall -Wextra -Wpedantic)
endif()

# Install each app binary (handy for packaging examples)
install(TARGETS list_files RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
