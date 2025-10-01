cmake_minimum_required(VERSION 3.16)
project(librevna-headless-cli LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(LIBREVNA_HEADLESS_ENABLE_REAL_DRIVER "Enable building against the official LibreVNA host stack" OFF)

add_library(librevna_host_core STATIC)

if(LIBREVNA_HEADLESS_ENABLE_REAL_DRIVER)
    find_package(Qt6 COMPONENTS Core Widgets REQUIRED)
    find_package(libusb-1.0 REQUIRED)
    find_package(nlohmann_json CONFIG REQUIRED)

    target_sources(librevna_host_core
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/../third_party/librevna_protocol/calibration.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/../third_party/librevna_protocol/calkit.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/../third_party/librevna_protocol/calstandard.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/../third_party/librevna_protocol/devicedriver.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/../third_party/librevna_protocol/librevnadriver.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/../third_party/librevna_protocol/librevnausbdriver.cpp
    )

    target_include_directories(librevna_host_core
        PUBLIC
            ${CMAKE_CURRENT_SOURCE_DIR}/../third_party/librevna_protocol
    )

    target_link_libraries(librevna_host_core
        PUBLIC
            Qt6::Core
            Qt6::Widgets
            libusb-1.0
            nlohmann_json::nlohmann_json
    )

    target_compile_definitions(librevna_host_core PUBLIC LIBREVNA_HEADLESS_HAS_REAL_DRIVER=1)
else()
    target_sources(librevna_host_core
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src/stub_host_core.cpp
    )

    target_include_directories(librevna_host_core
        PUBLIC
            ${CMAKE_CURRENT_SOURCE_DIR}/include
    )

    target_compile_definitions(librevna_host_core PUBLIC LIBREVNA_HEADLESS_HAS_REAL_DRIVER=0)
endif()

if(NOT TARGET nlohmann_json::nlohmann_json)
    add_library(nlohmann_json::nlohmann_json INTERFACE IMPORTED)
    set_target_properties(nlohmann_json::nlohmann_json PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_CURRENT_SOURCE_DIR}/include"
    )
endif()

add_executable(librevna-cli
    src/main.cpp
    src/cli_runner.cpp
    src/cli_options.cpp
    src/json_writer.cpp
    src/sweep_result.cpp
)

target_include_directories(librevna-cli
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

find_package(Threads REQUIRED)

target_link_libraries(librevna-cli
    PRIVATE
        librevna_host_core
        Threads::Threads
)

install(TARGETS librevna-cli RUNTIME DESTINATION bin)
