#include "opencl_runtime.hpp"

#include <fstream>
#include <iostream>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace ocl
{

    namespace
    {
        const char* device_type_string (cl_device_type dtype) noexcept
        {
            if (dtype & CL_DEVICE_TYPE_GPU)
                return "GPU";
            if (dtype & CL_DEVICE_TYPE_CPU)
                return "CPU";
            if (dtype & CL_DEVICE_TYPE_ACCELERATOR)
                return "Accelerator";

            return "Unknown";
        }

        const char* build_status_string (cl_build_status status) noexcept
        {
            switch (status)
            {
                case CL_BUILD_NONE:        return "CL_BUILD_NONE";
                case CL_BUILD_ERROR:       return "CL_BUILD_ERROR";
                case CL_BUILD_SUCCESS:     return "CL_BUILD_SUCCESS";
                case CL_BUILD_IN_PROGRESS: return "CL_BUILD_IN_PROGRESS";
                default:                   return "UNKNOWN_BUILD_STATUS";
            }
        }

        // probe-compile a trivial kernel to verify the device can actually
        // build programs. CL_DEVICE_COMPILER_AVAILABLE alone is not sufficient —
        // some runtimes report it as true but fail to compile anything.
        bool probe_buildable (const cl::Device& device) noexcept
        {
            try
            {
                cl::Context ctx (device);
                cl::Program probe (ctx, "__kernel void _probe() {}\n");
                try
                {
                    probe.build ({device});
                }
                catch (...)
                {
                    probe = cl::Program (ctx, "__kernel void _probe() {}\n");
                    probe.build ({device}, "-cl-std=CL1.2");
                }
                return true;
            }
            catch (...)
            {
                return false;
            }
        }

        std::string collapse_build_logs (const cl::BuildError& e)
        {
            const auto logs = e.getBuildLog();
            if (logs.empty())
                return {};

            std::ostringstream out;
            for (const auto& [dev, log] : logs)
            {
                out << "---- " << dev.getInfo<CL_DEVICE_NAME>() << " ----\n";
                out << (log.empty() ? "<empty>\n" : log);
                if (!log.empty() && log.back() != '\n')
                    out << '\n';
            }
            return out.str();
        }
    } // namespace

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
        struct Candidate
        {
            cl::Platform platform;
            cl::Device device;
            bool is_gpu;
            bool compiler_available;
            bool buildable;
        };

        std::vector<cl::Platform> platforms;
        cl::Platform::get (&platforms);

        if (platforms.empty())
            throw std::runtime_error ("No OpenCL platforms found");

        std::optional<Candidate> gpu_buildable;
        std::optional<Candidate> any_buildable;
        std::optional<Candidate> gpu_compiler;
        std::optional<Candidate> any_compiler;
        std::optional<Candidate> any_device;

        for (auto& p : platforms)
        {
            std::vector<cl::Device> devices;
            try
            {
                p.getDevices (CL_DEVICE_TYPE_ALL, &devices);
            }
            catch (const cl::Error&)
            {
                continue;
            }

            for (auto& d : devices)
            {
                const cl_device_type dtype = d.getInfo<CL_DEVICE_TYPE>();
                const bool is_gpu = (dtype & CL_DEVICE_TYPE_GPU) != 0;
                const bool compiler = d.getInfo<CL_DEVICE_COMPILER_AVAILABLE>() != CL_FALSE;
                const bool buildable = compiler && probe_buildable (d);

                Candidate c{p, d, is_gpu, compiler, buildable};

                if (!any_device.has_value())
                    any_device = c;
                if (compiler && !any_compiler.has_value())
                    any_compiler = c;
                if (compiler && is_gpu && !gpu_compiler.has_value())
                    gpu_compiler = c;
                if (buildable && !any_buildable.has_value())
                    any_buildable = c;
                if (buildable && is_gpu && !gpu_buildable.has_value())
                    gpu_buildable = c;
            }
        }

        if (!any_device.has_value())
            throw std::runtime_error ("No OpenCL devices found on any platform");

        const Candidate selected =
            gpu_buildable.value_or (
                any_buildable.value_or (
                    gpu_compiler.value_or (
                        any_compiler.value_or (
                            any_device.value()))));

        if (!selected.buildable)
        {
            std::cerr << "[OpenCL] Warning: build probe failed for all devices;"
                         " using fallback\n";
        }

        platform_  = selected.platform;
        device_    = selected.device;
        buildable_ = selected.buildable;

        const cl_device_type dtype = device_.getInfo<CL_DEVICE_TYPE>();

        std::cerr << "[OpenCL] Platform : " << platform_.getInfo<CL_PLATFORM_NAME>() << "\n"
                  << "[OpenCL] Device   : " << device_.getInfo<CL_DEVICE_NAME>() << "\n"
                  << "[OpenCL] Type     : " << device_type_string (dtype) << "\n"
                  << "[OpenCL] Version  : " << device_.getInfo<CL_DEVICE_VERSION>() << "\n";

        context_ = cl::Context (device_);
        queue_   = cl::CommandQueue (context_, device_, CL_QUEUE_PROFILING_ENABLE);
    }

    void Runtime::build_program (const std::filesystem::path& kernel_path)
    {
        const std::filesystem::path abs_kernel_path = std::filesystem::absolute (kernel_path);

        std::ifstream file (abs_kernel_path, std::ios::binary);
        if (!file.is_open())
        {
            throw std::runtime_error (
                "Cannot open kernel file: " + abs_kernel_path.string()
            );
        }

        std::ostringstream oss;
        oss << file.rdbuf();

        const std::string source = oss.str();

        auto make_error_message =
            [&](cl_int code, const std::string& options, const std::string& log)
            {
                std::ostringstream msg;
                msg << "Kernel build failed\n"
                    << "  kernel_path : " << abs_kernel_path.string() << "\n"
                    << "  platform    : " << platform_.getInfo<CL_PLATFORM_NAME>() << "\n"
                    << "  device      : " << device_.getInfo<CL_DEVICE_NAME>() << "\n"
                    << "  options     : " << (options.empty() ? "<none>" : options) << "\n"
                    << "  error_code  : " << code << " (" << error_string (code) << ")\n";

                try
                {
                    const cl_build_status status =
                        program_.getBuildInfo<CL_PROGRAM_BUILD_STATUS> (device_);
                    msg << "  build_status: " << build_status_string (status) << "\n";
                }
                catch (...)
                {
                    msg << "  build_status: <unavailable>\n";
                }

                msg << "  build_log:\n";
                msg << (log.empty() ? "<empty>\n" : log);
                return msg.str();
            };

        const std::vector<std::string> build_options = {"", "-cl-std=CL1.2"};
        cl_int last_err = CL_SUCCESS;
        std::string last_log;
        std::string last_options;

        for (const std::string& options : build_options)
        {
            try
            {
                program_ = cl::Program (context_, source);

                if (options.empty())
                    program_.build ({device_});
                else
                    program_.build ({device_}, options.c_str());
                return;
            }
            catch (const cl::BuildError& e)
            {
                last_err = e.err();
                last_log = collapse_build_logs (e);
                if (last_log.empty())
                {
                    try
                    {
                        last_log = program_.getBuildInfo<CL_PROGRAM_BUILD_LOG> (device_);
                    }
                    catch (...)
                    {
                        last_log.clear();
                    }
                }
                last_options = options;
            }
            catch (const cl::Error& e)
            {
                last_err = e.err();
                last_log.clear();
                last_options = options;
            }
        }

        throw std::runtime_error (make_error_message (last_err, last_options, last_log));
    }

} // namespace ocl
