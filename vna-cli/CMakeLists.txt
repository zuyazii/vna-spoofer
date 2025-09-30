cmake_minimum_required(VERSION 3.16)
project(vna-cli VERSION 0.1.0 LANGUAGES CXX)

# C++17 standard required
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# Find required packages
find_package(Qt5 COMPONENTS Core REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBUSB REQUIRED libusb-1.0)

# nlohmann/json (header-only library)
# For now, we'll use a simple include from system or add it as a submodule
# add_subdirectory(third_party/json EXCLUDE_FROM_ALL)

# Include directories
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party
    ${LIBUSB_INCLUDE_DIRS}
)

# Source files
set(VNA_CLI_SOURCES
    src/main.cpp
    src/controller.cpp
)

# Create executable
add_executable(vna-cli ${VNA_CLI_SOURCES})

# Link libraries
target_link_libraries(vna-cli
    Qt5::Core
    ${LIBUSB_LIBRARIES}
)

# Compiler options
target_compile_options(vna-cli PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    $<$<CONFIG:Debug>:-g>
    $<$<CONFIG:Release>:-O3>
)

# Install targets
install(TARGETS vna-cli
    RUNTIME DESTINATION bin
)

# Documentation
install(DIRECTORY docs/
    DESTINATION share/doc/vna-cli
    OPTIONAL
)
