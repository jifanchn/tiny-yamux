cmake_minimum_required(VERSION 3.10)
project(tiny-yamux C)

# Set C standard
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Set compiler warnings
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra -Werror -pedantic)
endif()

# Define library sources
set(YAMUX_SOURCES
    src/yamux_buffer.c
    src/yamux_frame.c
    src/yamux_handlers.c
    src/yamux_session.c
    src/yamux_session_utils.c
    src/yamux_stream.c
    src/yamux_stream_utils.c
    src/yamux_stream_ext.c
)

set(PORT_SOURCES
    src/yamux_port.c

)

# Define include directories
include_directories(include)

# Create main library
add_library(tiny_yamux STATIC ${YAMUX_SOURCES})

# Create portable interface library
add_library(tiny_yamux_port STATIC ${PORT_SOURCES})
target_link_libraries(tiny_yamux_port tiny_yamux)

# Examples
option(BUILD_EXAMPLES "Build examples" ON)
if(BUILD_EXAMPLES)
    # Simple demo
    add_executable(simple_demo examples/simple_demo.c)
    target_link_libraries(simple_demo tiny_yamux_port)
    if(UNIX)
        target_link_libraries(simple_demo pthread)
    endif()
endif()

# Tests
option(BUILD_TESTS "Build tests" ON)
if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# Custom target to build and run yamux tests
if(BUILD_TESTS)
    add_custom_target(yamux-test
        DEPENDS test_yamux_main test_yamux_port
        COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        COMMENT "Build and run yamux tests"
    )
endif()

# CGO tests for C-Go interoperability
option(BUILD_CGO_TESTS "Build C-Go interoperability tests" ON)
if(BUILD_CGO_TESTS)
    find_program(GO_EXECUTABLE go)
    if(GO_EXECUTABLE)
        message(STATUS "Go found at: ${GO_EXECUTABLE}")
        add_custom_target(cgo-tests
            COMMAND ${CMAKE_COMMAND} -E echo "Building CGO interoperability tests..."
            COMMAND cd ${CMAKE_SOURCE_DIR}/tests/cgo_tests/src && ${GO_EXECUTABLE} build -o ${CMAKE_BINARY_DIR}/cgo.out cgo_compatibility_runner.go
            DEPENDS tiny_yamux tiny_yamux_port
            COMMENT "Building CGO interoperability tests directly with go build"
        )
    else()
        message(WARNING "Go not found, CGO tests will not be built")
    endif()
else()
    message(STATUS "CGO tests temporarily disabled for initial C tests")
endif()

# Installation
install(TARGETS tiny_yamux tiny_yamux_port
        ARCHIVE DESTINATION lib
        LIBRARY DESTINATION lib
        RUNTIME DESTINATION bin)

install(FILES 
        include/yamux.h
        DESTINATION include/tiny-yamux)

# Option for embedded builds
option(EMBEDDED_BUILD "Build for embedded systems" OFF)
if(EMBEDDED_BUILD)
    # Remove standard library dependencies where possible
    add_definitions(-DYAMUX_EMBEDDED_BUILD)
    # Add more embedded-specific settings here
endif()
