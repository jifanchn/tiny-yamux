# C Tests CMakeLists.txt for tiny-yamux

# Main test executable
add_executable(test_yamux_main
    test_main.c
    test_buffer.c
    test_frame.c
    test_frame_decoding.c
    test_stream_io.c
    test_session.c
    test_flow_control.c
    test_stream_lifecycle.c
    test_concurrent_streams.c
    test_error_handling.c
)

target_include_directories(test_yamux_main PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(test_yamux_main PRIVATE tiny_yamux_port)

# Add test
add_test(
    NAME test_yamux_main
    COMMAND test_yamux_main
)

# New Port API test
add_executable(test_yamux_port
    test_yamux_port.c
)

target_include_directories(test_yamux_port PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(test_yamux_port PRIVATE tiny_yamux_port pthread)

# Add port test
add_test(
    NAME test_yamux_port
    COMMAND test_yamux_port
)

# Individual test executables have been consolidated into test_yamux_main
# No longer creating separate executables for each test file

# Performance test
add_executable(performance_test
    performance_test.c
)

target_include_directories(performance_test PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(performance_test PRIVATE tiny_yamux)

# Enable testing
enable_testing()
