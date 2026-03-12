cmake_minimum_required (VERSION 3.15)

project (BitonicSort LANGUAGES CXX)

set (CMAKE_CXX_STANDARD 17)
set (CMAKE_CXX_STANDARD_REQUIRED ON)

option (SANITIZE "Enable Address and Undefined sanitizers" OFF)
option (BUILD_TESTS "Build tests" ON)

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

find_package (OpenCL REQUIRED)
find_package (CLI11 CONFIG QUIET)
if (NOT TARGET CLI11::CLI11)
    find_path (CLI11_INCLUDE_DIR NAMES CLI/CLI.hpp)
    if (CLI11_INCLUDE_DIR)
        add_library (CLI11::CLI11 INTERFACE IMPORTED)
        set_target_properties (CLI11::CLI11 PROPERTIES
            INTERFACE_INCLUDE_DIRECTORIES "${CLI11_INCLUDE_DIR}"
        )
    else ()
        include (FetchContent)
        FetchContent_Declare (
            CLI11
            GIT_REPOSITORY https://github.com/CLIUtils/CLI11.git
            GIT_TAG        v2.4.2
            GIT_SHALLOW    TRUE
        )
        FetchContent_MakeAvailable (CLI11)
    endif ()
endif ()

add_executable (bitonic_sort
    src/main.cpp
    src/opencl_runtime.cpp
    src/bitonic_runner.cpp
)

target_include_directories (bitonic_sort PRIVATE src)

target_link_libraries (bitonic_sort PRIVATE OpenCL::OpenCL CLI11::CLI11)

target_compile_options (bitonic_sort PRIVATE
    ${COMMON_COMPILE_OPTIONS}
    ${SANITIZE_FLAGS}
)

target_link_options (bitonic_sort PRIVATE
    ${SANITIZE_FLAGS}
)

# Kernels:
set (KERNEL_SRC ${CMAKE_SOURCE_DIR}/kernels/bitonic.cl)
set (KERNEL_DST ${CMAKE_BINARY_DIR}/kernels/bitonic.cl)

add_custom_command (
    OUTPUT  ${KERNEL_DST}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/kernels
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

    target_include_directories (bitonic_test PRIVATE src)

    target_link_libraries (bitonic_test PRIVATE OpenCL::OpenCL)

    target_compile_options (bitonic_test PRIVATE
        ${COMMON_COMPILE_OPTIONS}
        ${SANITIZE_FLAGS}
    )

    target_link_options (bitonic_test PRIVATE
        ${SANITIZE_FLAGS}
    )

    find_package (GTest REQUIRED)
    target_link_libraries (bitonic_test PRIVATE GTest::gtest GTest::gtest_main)

    add_dependencies (bitonic_test copy_kernels)

    include (GoogleTest)
    gtest_discover_tests (bitonic_test
        DISCOVERY_TIMEOUT 120
    )
endif ()
