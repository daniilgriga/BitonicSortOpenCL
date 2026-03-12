cmake_minimum_required (VERSION 3.15)

project (BitonicSort LANGUAGES CXX)

set (CMAKE_CXX_STANDARD 17)
set (CMAKE_CXX_STANDARD_REQUIRED ON)

option (SANITIZE "Enable Address and Undefined sanitizers" OFF)
option (BUILD_TESTS "Build tests" OFF)
option (BITONIC_FETCH_DEPS "Fetch missing third-party dependencies with FetchContent" ON)

list (APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include (BitonicDependencies)

if (MSVC)
    set (COMMON_COMPILE_OPTIONS /W4 /WX)
else ()
    set (COMMON_COMPILE_OPTIONS
        -Wall
        -Wextra
        -Wpedantic
        -Werror
    )
endif ()

if (SANITIZE AND NOT MSVC)
    set (SANITIZE_FLAGS -fsanitize=address,undefined)
endif ()

add_library (bitonic_build_options INTERFACE)
target_compile_options (bitonic_build_options INTERFACE
    ${COMMON_COMPILE_OPTIONS}
    ${SANITIZE_FLAGS}
)
target_link_options (bitonic_build_options INTERFACE
    ${SANITIZE_FLAGS}
)

find_package (OpenCL REQUIRED)
bitonic_resolve_opencl_cpp_headers (
    BITONIC_OPENCL_HPP_INCLUDE_DIR
    ${OpenCL_INCLUDE_DIRS}
    ${OpenCL_INCLUDE_DIR}
)
bitonic_ensure_cli11 ()

add_executable (bitonic_sort
    src/main.cpp
    src/opencl_runtime.cpp
    src/bitonic_runner.cpp
)

target_include_directories (
    bitonic_sort PRIVATE
    src
    ${BITONIC_OPENCL_HPP_INCLUDE_DIR}
)

target_link_libraries (
    bitonic_sort PRIVATE
    OpenCL::OpenCL
    CLI11::CLI11
    bitonic_build_options
)

# Kernels:
set (KERNEL_SRC ${CMAKE_CURRENT_SOURCE_DIR}/kernels/bitonic.cl)
set (KERNEL_DST ${CMAKE_CURRENT_BINARY_DIR}/kernels/bitonic.cl)

add_custom_command (
    OUTPUT  ${KERNEL_DST}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/kernels
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${KERNEL_SRC} ${KERNEL_DST}
    DEPENDS ${KERNEL_SRC}
)

add_custom_target (copy_kernels DEPENDS ${KERNEL_DST})

add_dependencies (bitonic_sort copy_kernels)

if (BUILD_TESTS)
    enable_testing ()

    add_executable (bitonic_test
        tests/unit/test_opencl_runtime.cpp
        tests/unit/test_bitonic_runner.cpp
        src/opencl_runtime.cpp
        src/bitonic_runner.cpp
    )

    target_include_directories (
        bitonic_test PRIVATE
        src
        ${BITONIC_OPENCL_HPP_INCLUDE_DIR}
    )

    target_link_libraries (
        bitonic_test PRIVATE
        OpenCL::OpenCL
        bitonic_build_options
    )

    bitonic_resolve_gtest (BITONIC_GTEST_LIB_TARGET BITONIC_GTEST_MAIN_TARGET)

    target_link_libraries (
        bitonic_test PRIVATE
        ${BITONIC_GTEST_LIB_TARGET}
        ${BITONIC_GTEST_MAIN_TARGET}
    )

    add_dependencies (bitonic_test copy_kernels)

    include (GoogleTest)
    gtest_discover_tests (bitonic_test
        DISCOVERY_TIMEOUT 120
    )
endif ()
