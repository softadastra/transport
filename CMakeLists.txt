cmake_minimum_required(VERSION 3.20)

project(softadastra_transport
    VERSION 0.1.0
    DESCRIPTION "Softadastra Transport - Peer-to-peer transport layer"
    LANGUAGES CXX
)

include(GNUInstallDirs)

# ========================
# Options
# ========================
option(SOFTADASTRA_TRANSPORT_BUILD_TESTS "Build transport tests" OFF)
option(SOFTADASTRA_TRANSPORT_BUILD_EXAMPLES "Build transport examples" OFF)

# ========================
# Global settings
# ========================
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# ========================
# Dependencies
# ========================
if(NOT SOFTADASTRA_UMBRELLA_BUILD)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/.vix/vix_deps.cmake")
        include("${CMAKE_CURRENT_SOURCE_DIR}/.vix/vix_deps.cmake")
    endif()
endif()

if(NOT TARGET softadastra::sync AND NOT TARGET softadastra_sync)
    find_package(softadastra_sync CONFIG QUIET)
    find_package(sync CONFIG QUIET)
endif()

set(SOFTADASTRA_TRANSPORT_SYNC_TARGET "")
if(TARGET softadastra::sync)
    set(SOFTADASTRA_TRANSPORT_SYNC_TARGET softadastra::sync)
elseif(TARGET softadastra_sync)
    set(SOFTADASTRA_TRANSPORT_SYNC_TARGET softadastra_sync)
endif()

if(SOFTADASTRA_TRANSPORT_SYNC_TARGET STREQUAL "")
    message(FATAL_ERROR
        "softadastra sync target not found.\n"
        "Install dependencies with Vix:\n"
        "  vix add @softadastra/sync\n"
        "  vix install"
    )
endif()

# ========================
# Sources
# ========================
file(GLOB_RECURSE SOFTADASTRA_TRANSPORT_SOURCES
    CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
)

set(SOFTADASTRA_TRANSPORT_PLATFORM_SOURCES)

if(UNIX AND NOT APPLE)
    file(GLOB_RECURSE SOFTADASTRA_TRANSPORT_LINUX_SOURCES
        CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/platform/linux/*.cpp"
    )
    list(APPEND SOFTADASTRA_TRANSPORT_PLATFORM_SOURCES
        ${SOFTADASTRA_TRANSPORT_LINUX_SOURCES}
    )
endif()

if(APPLE)
    file(GLOB_RECURSE SOFTADASTRA_TRANSPORT_MAC_SOURCES
        CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/platform/mac/*.cpp"
    )
    list(APPEND SOFTADASTRA_TRANSPORT_PLATFORM_SOURCES
        ${SOFTADASTRA_TRANSPORT_MAC_SOURCES}
    )
endif()

if(WIN32)
    file(GLOB_RECURSE SOFTADASTRA_TRANSPORT_WINDOWS_SOURCES
        CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/platform/windows/*.cpp"
    )
    list(APPEND SOFTADASTRA_TRANSPORT_PLATFORM_SOURCES
        ${SOFTADASTRA_TRANSPORT_WINDOWS_SOURCES}
    )
endif()

# ========================
# Library
# ========================
add_library(softadastra_transport
    ${SOFTADASTRA_TRANSPORT_SOURCES}
    ${SOFTADASTRA_TRANSPORT_PLATFORM_SOURCES}
)

add_library(softadastra::transport ALIAS softadastra_transport)

target_compile_features(softadastra_transport PUBLIC cxx_std_20)

target_link_libraries(softadastra_transport
    PUBLIC
        ${SOFTADASTRA_TRANSPORT_SYNC_TARGET}
)

# ========================
# Include directories
# ========================
target_include_directories(softadastra_transport
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

# ========================
# Compiler options
# ========================
if(MSVC)
    target_compile_options(softadastra_transport PRIVATE /W4 /permissive-)
else()
    target_compile_options(softadastra_transport PRIVATE -Wall -Wextra -Wpedantic)
endif()

# ========================
# Platform definitions
# ========================
if(UNIX AND NOT APPLE)
    target_compile_definitions(softadastra_transport PRIVATE SOFTADASTRA_TRANSPORT_LINUX)
elseif(APPLE)
    target_compile_definitions(softadastra_transport PRIVATE SOFTADASTRA_TRANSPORT_MAC)
elseif(WIN32)
    target_compile_definitions(softadastra_transport PRIVATE SOFTADASTRA_TRANSPORT_WINDOWS)
endif()

# ========================
# Target properties
# ========================
set_target_properties(softadastra_transport PROPERTIES
    OUTPUT_NAME softadastra_transport
    VERSION ${PROJECT_VERSION}
    SOVERSION 0
    EXPORT_NAME transport
)

# ========================
# Examples
# ========================
if(SOFTADASTRA_TRANSPORT_BUILD_EXAMPLES AND NOT SOFTADASTRA_UMBRELLA_BUILD)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/examples/CMakeLists.txt")
        add_subdirectory(examples)
    else()
        message(STATUS "[transport] examples/ missing CMakeLists.txt -> skipping")
    endif()
endif()

# ========================
# Tests
# ========================
if(SOFTADASTRA_TRANSPORT_BUILD_TESTS)
    include(CTest)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/CMakeLists.txt")
        add_subdirectory(tests)
    else()
        message(STATUS "[transport] tests/ missing CMakeLists.txt -> skipping")
    endif()
endif()

# ========================
# Summary
# ========================
message(STATUS "------------------------------------------------------")
message(STATUS "softadastra::transport configured (${PROJECT_VERSION})")
message(STATUS "Sync target:     ${SOFTADASTRA_TRANSPORT_SYNC_TARGET}")
message(STATUS "Include dir:     ${CMAKE_CURRENT_SOURCE_DIR}/include")
message(STATUS "Build tests:     ${SOFTADASTRA_TRANSPORT_BUILD_TESTS}")
message(STATUS "Build examples:  ${SOFTADASTRA_TRANSPORT_BUILD_EXAMPLES}")
message(STATUS "Build type:      ${CMAKE_BUILD_TYPE}")
message(STATUS "------------------------------------------------------")
