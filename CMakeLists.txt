cmake_minimum_required(VERSION 3.20)

if(CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
    project(softadastra_transport
        VERSION 0.1.0
        DESCRIPTION "Softadastra Transport - Peer-to-peer transport layer"
        LANGUAGES CXX
    )
endif()

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

# ========================
# Standalone fallback options
# ========================
if(NOT DEFINED SOFTADASTRA_WARNINGS_AS_ERRORS)
    option(SOFTADASTRA_WARNINGS_AS_ERRORS "Treat warnings as errors" OFF)
endif()

if(SOFTADASTRA_UMBRELLA_BUILD)
    set(SOFTADASTRA_TRANSPORT_BUILD_TESTS ${SOFTADASTRA_BUILD_TESTS})
else()
    option(SOFTADASTRA_TRANSPORT_BUILD_TESTS "Build transport tests" OFF)
endif()

option(SOFTADASTRA_TRANSPORT_BUILD_EXAMPLES "Build transport examples" OFF)

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
    if(SOFTADASTRA_WARNINGS_AS_ERRORS)
        target_compile_options(softadastra_transport PRIVATE /WX)
    endif()
else()
    target_compile_options(softadastra_transport PRIVATE -Wall -Wextra -Wpedantic)
    if(SOFTADASTRA_WARNINGS_AS_ERRORS)
        target_compile_options(softadastra_transport PRIVATE -Werror)
    endif()
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
    VERSION 0.1.0
    SOVERSION 0
    EXPORT_NAME transport
)

# ========================
# Install
# ========================
install(
    TARGETS softadastra_transport
    EXPORT softadastraTransportTargets
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(
    DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/include/"
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(
    EXPORT softadastraTransportTargets
    FILE softadastraTransportTargets.cmake
    NAMESPACE softadastra::
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/softadastra_transport"
)

write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/softadastra_transportConfigVersion.cmake"
    VERSION 0.1.0
    COMPATIBILITY SameMajorVersion
)

configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/softadastra_transportConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/softadastra_transportConfig.cmake"
    INSTALL_DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/softadastra_transport"
)

install(
    FILES
        "${CMAKE_CURRENT_BINARY_DIR}/softadastra_transportConfig.cmake"
        "${CMAKE_CURRENT_BINARY_DIR}/softadastra_transportConfigVersion.cmake"
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/softadastra_transport"
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
message(STATUS "softadastra::transport configured (0.1.0)")
message(STATUS "Sync target:     ${SOFTADASTRA_TRANSPORT_SYNC_TARGET}")
message(STATUS "Include dir:     ${CMAKE_CURRENT_SOURCE_DIR}/include")
message(STATUS "Build tests:     ${SOFTADASTRA_TRANSPORT_BUILD_TESTS}")
message(STATUS "Build examples:  ${SOFTADASTRA_TRANSPORT_BUILD_EXAMPLES}")
message(STATUS "Warnings=error:  ${SOFTADASTRA_WARNINGS_AS_ERRORS}")
message(STATUS "Build type:      ${CMAKE_BUILD_TYPE}")
message(STATUS "Install prefix:  ${CMAKE_INSTALL_PREFIX}")
message(STATUS "------------------------------------------------------")
