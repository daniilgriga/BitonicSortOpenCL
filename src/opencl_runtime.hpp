#pragma once

#include "ocl_config.hpp"

#include <filesystem>

namespace ocl
{

class Runtime
{
private:
    cl::Platform     platform_;
    cl::Device       device_;
    cl::Context      context_;
    cl::CommandQueue queue_;
    cl::Program      program_;

public:
    void init();
    void build_program (const std::filesystem::path& kernel_path);

    const cl::Device&  device()  const { return device_;  }
    const cl::Context& context() const { return context_; }
    cl::CommandQueue&  queue()         { return queue_;   }
    const cl::Program& program() const { return program_; }
};

const char* error_string (cl_int code) noexcept;

} // namespace ocl
