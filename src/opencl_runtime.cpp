#include "opencl_runtime.hpp"

#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <vector>

namespace ocl
{

    const char* error_string (cl_int code) noexcept
    {
        switch (code)
        {
            case CL_SUCCESS:                        return "CL_SUCCESS";
            case CL_DEVICE_NOT_FOUND:               return "CL_DEVICE_NOT_FOUND";
            case CL_DEVICE_NOT_AVAILABLE:           return "CL_DEVICE_NOT_AVAILABLE";
            case CL_COMPILER_NOT_AVAILABLE:         return "CL_COMPILER_NOT_AVAILABLE";
            case CL_MEM_OBJECT_ALLOCATION_FAILURE:  return "CL_MEM_OBJECT_ALLOCATION_FAILURE";
            case CL_OUT_OF_RESOURCES:               return "CL_OUT_OF_RESOURCES";
            case CL_OUT_OF_HOST_MEMORY:             return "CL_OUT_OF_HOST_MEMORY";
            case CL_BUILD_PROGRAM_FAILURE:          return "CL_BUILD_PROGRAM_FAILURE";
            case CL_INVALID_VALUE:                  return "CL_INVALID_VALUE";
            case CL_INVALID_DEVICE:                 return "CL_INVALID_DEVICE";
            case CL_INVALID_CONTEXT:                return "CL_INVALID_CONTEXT";
            case CL_INVALID_COMMAND_QUEUE:          return "CL_INVALID_COMMAND_QUEUE";
            case CL_INVALID_MEM_OBJECT:             return "CL_INVALID_MEM_OBJECT";
            case CL_INVALID_PROGRAM:                return "CL_INVALID_PROGRAM";
            case CL_INVALID_PROGRAM_EXECUTABLE:     return "CL_INVALID_PROGRAM_EXECUTABLE";
            case CL_INVALID_KERNEL_NAME:            return "CL_INVALID_KERNEL_NAME";
            case CL_INVALID_KERNEL_ARGS:            return "CL_INVALID_KERNEL_ARGS";
            case CL_INVALID_WORK_GROUP_SIZE:        return "CL_INVALID_WORK_GROUP_SIZE";
            case CL_INVALID_GLOBAL_WORK_SIZE:       return "CL_INVALID_GLOBAL_WORK_SIZE";
            case CL_INVALID_BUFFER_SIZE:            return "CL_INVALID_BUFFER_SIZE";
            default:                                return "UNKNOWN_CL_ERROR";
        }
    }

    void Runtime::init()
    {
        std::vector<cl::Platform> platforms;
        cl::Platform::get (&platforms);

        if (platforms.empty())
            throw std::runtime_error ("No OpenCL platforms found");

        cl::Device fallback_device;
        cl::Platform fallback_platform;
        bool found_any = false;
        bool found_gpu = false;

        for (auto& p : platforms)
        {
            std::vector<cl::Device> devices;
            try
            {
                p.getDevices (CL_DEVICE_TYPE_ALL, &devices);
            }
            catch (const cl::Error&)
            {
                continue; // broken platform - try the next one
            }

            for (auto& d : devices)
            {
                if (!found_any)
                {
                    fallback_device   = d;
                    fallback_platform = p;
                    found_any         = true;
                }

                if (d.getInfo<CL_DEVICE_TYPE>() & CL_DEVICE_TYPE_GPU)
                {
                    platform_  = p;
                    device_    = d;
                    found_gpu  = true;
                    break;
                }
            }

            if (found_gpu)
                break;
        }

        if (!found_any)
            throw std::runtime_error ("No OpenCL devices found on any platform");

        if (!found_gpu)
        {
            platform_ = fallback_platform;
            device_   = fallback_device;
        }

        cl_device_type dtype = device_.getInfo<CL_DEVICE_TYPE>();
        const char*    dtype_str =
            (dtype & CL_DEVICE_TYPE_GPU)         ? "GPU"         :
            (dtype & CL_DEVICE_TYPE_CPU)         ? "CPU"         :
            (dtype & CL_DEVICE_TYPE_ACCELERATOR) ? "Accelerator" : "Unknown";

        std::cout << "[OpenCL] Platform : " << platform_.getInfo<CL_PLATFORM_NAME>() << "\n"
                  << "[OpenCL] Device   : " << device_.getInfo<CL_DEVICE_NAME>()     << "\n"
                  << "[OpenCL] Type     : " << dtype_str                             << "\n"
                  << "[OpenCL] Version  : " << device_.getInfo<CL_DEVICE_VERSION>()  << "\n";

        context_ = cl::Context (device_);
        queue_   = cl::CommandQueue (context_, device_, CL_QUEUE_PROFILING_ENABLE);
    }

    void Runtime::build_program (const std::filesystem::path& kernel_path)
    {
        std::ifstream file (kernel_path, std::ios::binary);
        if (!file.is_open())
        {
            throw std::runtime_error (
                "Cannot open kernel file: " + std::filesystem::absolute (kernel_path).string()
            );
        }

        std::ostringstream oss;
        oss << file.rdbuf();

        program_ = cl::Program (context_, oss.str());

        try
        {
            program_.build ({device_});
        }
        catch (const cl::Error& e)
        {
            std::string log = program_.getBuildInfo<CL_PROGRAM_BUILD_LOG> (device_);

            throw std::runtime_error (
                std::string ("Kernel build failed (") + error_string (e.err()) + "):\n" + log );
        }
    }

} // namespace ocl
