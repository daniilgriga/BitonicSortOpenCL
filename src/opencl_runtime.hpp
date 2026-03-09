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
        bool             buildable_ = false;

    public:
        void init (bool verbose = false);
        void build_program (const std::filesystem::path& kernel_path);

        const cl::Device&  device()    const { return device_;    }
        const cl::Context& context()   const { return context_;   }
        cl::CommandQueue&  queue()           { return queue_;     }
        const cl::Program& program()   const { return program_;   }
        bool               buildable() const { return buildable_; }
    };

    const char* error_string (cl_int code) noexcept;

} // namespace ocl
