cmake_minimum_required(VERSION 3.20)

set(TRANSPORT_EXAMPLES
    basic_server
    basic_client
    sync_bridge
)

foreach(example_name IN LISTS TRANSPORT_EXAMPLES)
    add_executable(${example_name}
        ${CMAKE_CURRENT_SOURCE_DIR}/${example_name}.cpp
    )

    target_compile_features(${example_name} PRIVATE cxx_std_20)

    target_link_libraries(${example_name}
        PRIVATE
            softadastra::transport
    )

    target_include_directories(${example_name}
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/../include
    )

    if(MSVC)
        target_compile_options(${example_name} PRIVATE /W4 /permissive-)
    else()
        target_compile_options(${example_name} PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endforeach()
