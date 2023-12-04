cmake_minimum_required(VERSION 3.10)
project(MyProject)

find_package(amemorylibrary REQUIRED)
find_package(themacrolibrary REQUIRED)
find_package(thelz4library REQUIRED)
find_package(ZLIB REQUIRED)
find_package(theiolibrary REQUIRED)

# List of executables
set(EXECUTABLES convert_interactions)

# Loop through each executable
foreach(EXECUTABLE IN LISTS EXECUTABLES)
    # Create executable
    add_executable(${EXECUTABLE} src/${EXECUTABLE}.c)

    # Set compile definitions
    # target_compile_definitions(${EXECUTABLE} PUBLIC -D_AML_DEBUG_)

    # Set compile options
    target_compile_options(${EXECUTABLE} PRIVATE -O3)

    # Add the macro library
    target_include_directories(${EXECUTABLE} PRIVATE ${themacrolibrary_INCLUDE_DIRS})

    # Link libraries
    target_link_libraries(${EXECUTABLE} PRIVATE amemorylibrary_debug thelz4library_static theiolibrary_static ZLIB::ZLIB)
endforeach()