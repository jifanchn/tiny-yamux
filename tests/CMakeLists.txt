# Tests CMakeLists.txt for tiny-yamux

# C tests subdirectory
add_subdirectory(c_tests)

# CGO tests - temporarily disabled
# We'll create the proper CMakeLists.txt in cgo_tests directory later
message(STATUS "CGO tests temporarily disabled for initial C tests")
