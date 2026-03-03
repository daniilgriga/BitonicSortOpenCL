#include "opencl_runtime.hpp"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>

#include <gtest/gtest.h>

class RuntimeTest : public ::testing::Test
{
protected:
    ocl::Runtime rt;

    void SetUp() override
    {
        rt.init();
    }
};

TEST_F (RuntimeTest, InitDoesNotThrow)
{
    SUCCEED();
}

TEST_F (RuntimeTest, DeviceNameNotEmpty)
{
    std::string name = rt.device().getInfo<CL_DEVICE_NAME>();

    EXPECT_FALSE (name.empty());
}

TEST_F (RuntimeTest, ContextIsValid)
{
    cl_uint ref_count = rt.context().getInfo<CL_CONTEXT_REFERENCE_COUNT>();

    EXPECT_GT (ref_count, 0u);
}

TEST_F (RuntimeTest, QueueProfilingEnabled)
{
    cl_command_queue_properties props =
        rt.queue().getInfo<CL_QUEUE_PROPERTIES>();

    EXPECT_TRUE (props & CL_QUEUE_PROFILING_ENABLE);
}

// helper: check if the runtime can actually compile a trivial kernel
bool can_compile_kernels (ocl::Runtime& rt)
{
    try
    {
        auto prog = cl::Program (rt.context(), "__kernel void probe() {}\n");
        prog.build ({rt.device()});
        return true;
    }
    catch (...)
    {
        return false;
    }
}

// helper: create a temporary file with given contents
struct TempFile
{
    std::filesystem::path path;

    TempFile (const std::string& suffix, const std::string& content)
    {
        path = std::filesystem::temp_directory_path() / ("bitonic_test_" + suffix);
        std::ofstream out (path, std::ios::binary);
        out << content;
    }

    ~TempFile()
    {
        std::filesystem::remove (path);
    }
};

TEST_F (RuntimeTest, BuildProgramValidKernel)
{
    if (!can_compile_kernels (rt))
        GTEST_SKIP() << "OpenCL runtime cannot compile kernels";

    TempFile tmp ("valid.cl", "__kernel void dummy() {}\n");

    EXPECT_NO_THROW (rt.build_program (tmp.path));
}

TEST_F (RuntimeTest, BuildProgramNonexistentFile)
{
    try
    {
        rt.build_program ("/nonexistent/path/to/kernel.cl");

        FAIL() << "Expected std::runtime_error";
    }
    catch (const std::runtime_error& e)
    {
        std::string msg = e.what();

        EXPECT_NE (msg.find ("kernel.cl"), std::string::npos)
            << "Error should contain the file path: " << msg;
    }
}

TEST_F (RuntimeTest, BuildProgramInvalidKernel)
{
    if (!can_compile_kernels (rt))
        GTEST_SKIP() << "OpenCL runtime cannot compile kernels";

    TempFile tmp ("invalid.cl", "__kernel void broken( { SYNTAX ERROR }\n");

    try
    {
        rt.build_program (tmp.path);

        FAIL() << "Expected std::runtime_error";
    }
    catch (const std::runtime_error& e)
    {
        std::string msg = e.what();

        SCOPED_TRACE (msg);

        EXPECT_NE (msg.find ("Kernel build failed"), std::string::npos);
        EXPECT_NE (msg.find ("kernel_path"), std::string::npos);
        EXPECT_NE (msg.find ("platform"), std::string::npos);
        EXPECT_NE (msg.find ("device"), std::string::npos);
        EXPECT_NE (msg.find ("error_code"), std::string::npos);
        EXPECT_NE (msg.find ("build_status"), std::string::npos);
        EXPECT_NE (msg.find ("build_log"), std::string::npos);
    }
}

TEST_F (RuntimeTest, BuildProgramRealKernel)
{
    if (!can_compile_kernels (rt))
        GTEST_SKIP() << "OpenCL runtime cannot compile kernels";

    std::filesystem::path kernel_path = "kernels/bitonic.cl";
    if (!std::filesystem::exists (kernel_path))
        GTEST_SKIP() << "kernels/bitonic.cl not found in working directory";

    EXPECT_NO_THROW (rt.build_program (kernel_path));
}
