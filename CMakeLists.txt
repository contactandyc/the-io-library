cmake_minimum_required(VERSION 3.10)
project(theiolibrary VERSION 0.1.1)

# Options
option(DEBUG "Enable debugging" OFF)
option(ADDRESS_SANITIZER "Enable Address Sanitizer" OFF)

set(CMAKE_INSTALL_INCLUDEDIR include)
set(CMAKE_INSTALL_BINDIR bin)
set(CMAKE_INSTALL_DOCDIR share/doc/theiolibrary)
set(CMAKE_C_STANDARD 99)

# Set C standard
set(CMAKE_C_STANDARD 99)

# Find the required libraries
find_package(amemorylibrary REQUIRED)
find_package(themacrolibrary REQUIRED)
find_package(thelz4library REQUIRED)
find_package(ZLIB REQUIRED)

# Compiler options
if(ADDRESS_SANITIZER)
    add_compile_options(-fsanitize=address)
endif()

# Source files
set(SOURCE_FILES src/io.c src/io_in.c src/io_in_base.c src/io_out.c)

# Debug library
add_library(theiolibrary_debug STATIC ${SOURCE_FILES})
target_include_directories(theiolibrary_debug PUBLIC ${themacrolibrary_INCLUDE_DIRS})
target_include_directories(theiolibrary_debug PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
target_compile_definitions(theiolibrary_debug PUBLIC -D_AML_DEBUG_)
target_compile_options(theiolibrary_debug PRIVATE -g -O0)
set_target_properties(theiolibrary_debug PROPERTIES OUTPUT_NAME "theiolibrary_debug")
target_compile_options(theiolibrary_debug PRIVATE -g -O0)


# Release library
add_library(theiolibrary_static STATIC ${SOURCE_FILES})
target_include_directories(theiolibrary_static PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
target_include_directories(theiolibrary_static PUBLIC ${themacrolibrary_INCLUDE_DIRS})
target_compile_options(theiolibrary_static PRIVATE -O3)
set_target_properties(theiolibrary_static PROPERTIES OUTPUT_NAME "theiolibrary_static")
target_compile_options(theiolibrary_static PRIVATE -O3)

# Shared library
add_library(theiolibrary SHARED ${SOURCE_FILES})
target_include_directories(theiolibrary PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
target_include_directories(theiolibrary PUBLIC ${themacrolibrary_INCLUDE_DIRS})
target_compile_options(theiolibrary PRIVATE -O3)
set_target_properties(theiolibrary PROPERTIES OUTPUT_NAME "theiolibrary")
target_compile_options(theiolibrary PRIVATE -O3)

# Link libraries
target_link_libraries(theiolibrary_debug PUBLIC amemorylibrary thelz4library ZLIB::ZLIB)
target_link_libraries(theiolibrary_static PUBLIC amemorylibrary thelz4library ZLIB::ZLIB)
target_link_libraries(theiolibrary PUBLIC amemorylibrary thelz4library ZLIB::ZLIB)

# Installation of the library
install(TARGETS theiolibrary theiolibrary_static theiolibrary_debug
        EXPORT theiolibraryTargets
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib
        RUNTIME DESTINATION bin
        PUBLIC_HEADER DESTINATION include)

# Export the targets
install(EXPORT theiolibraryTargets
        FILE theiolibraryTargets.cmake
        DESTINATION lib/cmake/theiolibrary)

# Create and install the config file
include(CMakePackageConfigHelpers)
write_basic_package_version_file(
    "theiolibraryConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion)

configure_file(theiolibraryConfig.cmake.in theiolibraryConfig.cmake @ONLY)

install(FILES "${CMAKE_CURRENT_BINARY_DIR}/theiolibraryConfig.cmake"
              "${CMAKE_CURRENT_BINARY_DIR}/theiolibraryConfigVersion.cmake"
        DESTINATION lib/cmake/theiolibrary)

# Installation of other files
install(DIRECTORY ${CMAKE_SOURCE_DIR}/include/the-io-library DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(FILES README.md AUTHORS NEWS.md CHANGELOG.md LICENSE NOTICE DESTINATION ${CMAKE_INSTALL_DOCDIR})

# Uninstall command
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/bin/cmake_uninstall.cmake.in"
               "${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake"
               IMMEDIATE @ONLY)

add_custom_target(uninstall
    COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake)
