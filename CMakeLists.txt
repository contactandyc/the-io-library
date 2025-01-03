cmake_minimum_required(VERSION 3.10)

# Project Configuration
project(theiolibrary VERSION 0.1.1)
# Variables
set(INCLUDE_DIR_NAME "the-io-library")
set(EXTRA_FILES README.md AUTHORS NEWS.md CHANGELOG.md LICENSE NOTICE)
set(CUSTOM_PACKAGES amemorylibrary thelz4library themacrolibrary)
set(THIRD_PARTY_PACKAGES ZLIB)

# Source files
file(GLOB SOURCE_FILES src/*.c)

find_package(a-cmake-library REQUIRED)

include(LibraryConfig)
include(LibraryBuild)

# Testing
if(BUILD_TESTING)
    enable_testing()
    add_subdirectory(tests)
endif()
