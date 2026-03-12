include (FetchContent)

# Optional fallback sources. Default workflow remains find_package/system packages.
set (
    BITONIC_CLI11_URL
    "https://github.com/CLIUtils/CLI11/archive/refs/tags/v2.4.2.tar.gz"
    CACHE STRING
    "URL used to fetch CLI11 when BITONIC_FETCH_DEPS=ON and CLI11 is missing"
)
set (
    BITONIC_GTEST_URL
    "https://github.com/google/googletest/archive/refs/tags/v1.14.0.tar.gz"
    CACHE STRING
    "URL used to fetch GoogleTest when BITONIC_FETCH_DEPS=ON and GTest is missing"
)
set (
    BITONIC_OPENCL_CLHPP_URL
    "https://github.com/KhronosGroup/OpenCL-CLHPP/archive/refs/heads/main.tar.gz"
    CACHE STRING
    "URL used to fetch OpenCL C++ headers when BITONIC_FETCH_DEPS=ON and CL/opencl.hpp is missing"
)
mark_as_advanced (
    BITONIC_CLI11_URL
    BITONIC_GTEST_URL
    BITONIC_OPENCL_CLHPP_URL
)

function (bitonic_ensure_opencl_headers_target opencl_c_include_dir)
    if (TARGET OpenCL::Headers)
        return ()
    endif ()
    if (NOT opencl_c_include_dir)
        return ()
    endif ()

    add_library (bitonic_opencl_headers INTERFACE IMPORTED)
    add_library (OpenCL::Headers ALIAS bitonic_opencl_headers)
    set_target_properties (bitonic_opencl_headers PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES "${opencl_c_include_dir}"
    )
endfunction ()

function (bitonic_resolve_opencl_cpp_headers out_var)
    set (opencl_cpp_hints ${ARGN})
    if (DEFINED BITONIC_OPENCL_HPP_INCLUDE_DIR AND BITONIC_OPENCL_HPP_INCLUDE_DIR)
        list (APPEND opencl_cpp_hints "${BITONIC_OPENCL_HPP_INCLUDE_DIR}")
    endif ()

    find_path (
        OPENCL_CPP_INCLUDE_DIR
        NAMES CL/opencl.hpp CL/cl2.hpp
        HINTS ${opencl_cpp_hints}
        PATH_SUFFIXES "" include
    )

    if (NOT OPENCL_CPP_INCLUDE_DIR AND BITONIC_FETCH_DEPS)
        # Keep OpenCL-CLHPP configuration minimal; we only need headers.
        set (BUILD_DOCS OFF CACHE BOOL "" FORCE)
        set (BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
        set (OPENCL_CLHPP_BUILD_TESTING OFF CACHE BOOL "" FORCE)

        find_path (
            OPENCL_C_INCLUDE_DIR
            NAMES CL/cl.h OpenCL/cl.h
            HINTS ${opencl_cpp_hints}
            PATH_SUFFIXES "" include
        )
        if (OPENCL_C_INCLUDE_DIR)
            set (OPENCL_INCLUDE_DIR "${OPENCL_C_INCLUDE_DIR}" CACHE PATH "" FORCE)
            set (OPENCL_CLHPP_HEADERS_DIR "${OPENCL_C_INCLUDE_DIR}" CACHE PATH "" FORCE)
            bitonic_ensure_opencl_headers_target ("${OPENCL_C_INCLUDE_DIR}")
        endif ()

        FetchContent_Declare (
            opencl_clhpp
            URL "${BITONIC_OPENCL_CLHPP_URL}"
            DOWNLOAD_EXTRACT_TIMESTAMP TRUE
        )
        FetchContent_MakeAvailable (opencl_clhpp)

        find_path (
            OPENCL_CPP_INCLUDE_DIR
            NAMES CL/opencl.hpp CL/cl2.hpp
            HINTS
                ${opencl_cpp_hints}
                "${opencl_clhpp_SOURCE_DIR}"
                "${opencl_clhpp_SOURCE_DIR}/include"
            PATH_SUFFIXES "" include
        )
    endif ()

    if (NOT OPENCL_CPP_INCLUDE_DIR)
        message (FATAL_ERROR
            "OpenCL C++ headers not found: expected CL/opencl.hpp or CL/cl2.hpp. "
            "Install OpenCL C++ headers (system/vcpkg) "
            "or configure with -DBITONIC_FETCH_DEPS=ON."
        )
    endif ()

    set (${out_var} "${OPENCL_CPP_INCLUDE_DIR}" PARENT_SCOPE)
endfunction ()

function (bitonic_ensure_cli11)
    find_package (CLI11 CONFIG QUIET)
    if (TARGET CLI11::CLI11)
        return ()
    endif ()

    find_path (CLI11_INCLUDE_DIR NAMES CLI/CLI.hpp)
    if (CLI11_INCLUDE_DIR)
        add_library (CLI11::CLI11 INTERFACE IMPORTED)
        set_target_properties (CLI11::CLI11 PROPERTIES
            INTERFACE_INCLUDE_DIRECTORIES "${CLI11_INCLUDE_DIR}"
        )
        return ()
    endif ()

    if (BITONIC_FETCH_DEPS)
        FetchContent_Declare (
            cli11
            URL "${BITONIC_CLI11_URL}"
            DOWNLOAD_EXTRACT_TIMESTAMP TRUE
        )
        FetchContent_MakeAvailable (cli11)
    endif ()

    if (NOT TARGET CLI11::CLI11)
        message (FATAL_ERROR
            "CLI11 not found. Install libcli11-dev (Linux) or cli11:x64-windows (vcpkg), "
            "or configure with -DBITONIC_FETCH_DEPS=ON."
        )
    endif ()
endfunction ()

function (bitonic_resolve_gtest out_lib out_main)
    set (gtest_lib "")
    set (gtest_main "")

    find_package (GTest QUIET)
    if (TARGET GTest::gtest AND TARGET GTest::gtest_main)
        set (gtest_lib GTest::gtest)
        set (gtest_main GTest::gtest_main)
    elseif (TARGET GTest::GTest AND TARGET GTest::Main)
        set (gtest_lib GTest::GTest)
        set (gtest_main GTest::Main)
    elseif (BITONIC_FETCH_DEPS)
        set (gtest_force_shared_crt ON CACHE BOOL "" FORCE)
        FetchContent_Declare (
            googletest
            URL "${BITONIC_GTEST_URL}"
            DOWNLOAD_EXTRACT_TIMESTAMP TRUE
        )
        FetchContent_MakeAvailable (googletest)

        if (TARGET GTest::gtest AND TARGET GTest::gtest_main)
            set (gtest_lib GTest::gtest)
            set (gtest_main GTest::gtest_main)
        elseif (TARGET GTest::GTest AND TARGET GTest::Main)
            set (gtest_lib GTest::GTest)
            set (gtest_main GTest::Main)
        endif ()
    endif ()

    if (NOT gtest_lib)
        message (FATAL_ERROR
            "GTest not found. Install libgtest-dev (Linux) or gtest:x64-windows (vcpkg), "
            "or configure with -DBITONIC_FETCH_DEPS=ON."
        )
    endif ()

    set (${out_lib} "${gtest_lib}" PARENT_SCOPE)
    set (${out_main} "${gtest_main}" PARENT_SCOPE)
endfunction ()
