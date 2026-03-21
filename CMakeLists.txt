cmake_minimum_required(VERSION 3.20)

project(softadastra_transport
    VERSION 0.1.0
    DESCRIPTION "Softadastra Transport - Peer-to-peer transport layer"
    LANGUAGES CXX
)

# ========================
# Options
# ========================
option(SOFTADASTRA_TRANSPORT_BUILD_TESTS "Build transport tests" ON)
option(SOFTADASTRA_TRANSPORT_BUILD_EXAMPLES "Build transport examples" ON)

# ========================
# Library
# ========================
add_library(softadastra_transport)

target_compile_features(softadastra_transport PUBLIC cxx_std_20)

# ========================
# Dependencies
# ========================
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/.vix/vix_deps.cmake)
    include(${CMAKE_CURRENT_SOURCE_DIR}/.vix/vix_deps.cmake)
endif()

if(NOT TARGET softadastra::sync)
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/../sync/CMakeLists.txt)
        add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../sync build_sync)
    else()
        message(FATAL_ERROR
            "softadastra::sync not found. "
            "Install dependencies with Vix or provide ../sync in the monorepo."
        )
    endif()
endif()

target_link_libraries(softadastra_transport
    PUBLIC
        softadastra::sync
)

# ========================
# Include directories
# ========================
target_include_directories(softadastra_transport
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

# ========================
# Sources
# ========================
file(GLOB_RECURSE TRANSPORT_SOURCES
    CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/platform/*.cpp
)

target_sources(softadastra_transport
    PRIVATE
        ${TRANSPORT_SOURCES}
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
# Position Independent Code
# ========================
set_target_properties(softadastra_transport PROPERTIES
    POSITION_INDEPENDENT_CODE ON
)

# ========================
# Namespacing
# ========================
add_library(softadastra::transport ALIAS softadastra_transport)

# ========================
# Examples
# ========================
if(SOFTADASTRA_TRANSPORT_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

# ========================
# Tests
# ========================
if(SOFTADASTRA_TRANSPORT_BUILD_TESTS)
    include(CTest)
    add_subdirectory(tests)
endif()
