cmake_minimum_required(VERSION 3.20)

set(TRANSPORT_TEST_SOURCES
    test_message.cpp
    test_encoding.cpp
    test_peer_registry.cpp
    test_dispatcher.cpp
    test_ack.cpp
    test_engine.cpp
    test_tcp_backend.cpp
    test_handshake.cpp
)

foreach(test_source IN LISTS TRANSPORT_TEST_SOURCES)
    get_filename_component(test_file ${test_source} NAME_WE)
    set(target_name softadastra_transport_${test_file})

    add_executable(${target_name}
        ${CMAKE_CURRENT_SOURCE_DIR}/${test_source}
    )

    target_compile_features(${target_name} PRIVATE cxx_std_20)

    target_link_libraries(${target_name}
        PRIVATE
            softadastra::transport
    )

    target_include_directories(${target_name}
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/../include
    )

    if(MSVC)
        target_compile_options(${target_name} PRIVATE /W4 /permissive-)
    else()
        target_compile_options(${target_name} PRIVATE -Wall -Wextra -Wpedantic)
    endif()

    add_test(NAME ${target_name} COMMAND ${target_name})
endforeach()
