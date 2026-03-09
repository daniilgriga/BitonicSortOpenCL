#include "bitonic_runner.hpp"
#include "opencl_runtime.hpp"

#include <algorithm>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace
{

    constexpr const char* DEFAULT_KERNEL_PATH = "kernels/bitonic.cl";

    struct CpuSortResult
    {
        std::vector<int> data;
        std::uint64_t time_ns;
    };

    struct GpuSortResult
    {
        std::vector<int> data;
        bitonic::SortResult stats;
    };

    struct SortOutputs
    {
        std::size_t n;
        CpuSortResult cpu;
        GpuSortResult gpu;
    };

    void print_usage (const char* program_name)
    {
        std::cerr << "Usage: " << program_name
                  << " [--benchmark] [--kernel <path>] [--verbose] [--help]\n";
    }

    std::filesystem::path resolve_default_kernel_path()
    {
        const std::filesystem::path cwd_candidate = DEFAULT_KERNEL_PATH;
        if (std::filesystem::exists (cwd_candidate))
            return cwd_candidate;

#if defined(__linux__)
        std::error_code ec;
        const std::filesystem::path exe_path =
            std::filesystem::read_symlink ("/proc/self/exe", ec);
        if (!ec)
        {
            const std::filesystem::path exe_candidate =
                exe_path.parent_path() / DEFAULT_KERNEL_PATH;
            if (std::filesystem::exists (exe_candidate))
                return exe_candidate;
        }
#endif

        return cwd_candidate;
    }

    std::vector<int> read_input ()
    {
        int n;
        if (!(std::cin >> n) || n < 0)
        {
            throw std::runtime_error ("expected non-negative integer N");
        }

        std::vector<int> data (n);
        for (int i = 0; i < n; ++i)
        {
            if (!(std::cin >> data[i]))
            {
                throw std::runtime_error (
                    "expected " + std::to_string (n) + " integers, got " + std::to_string (i)
                );
            }
        }

        return data;
    }

    CpuSortResult run_cpu_sort (const std::vector<int>& input)
    {
        CpuSortResult out;
        out.data = input;

        const auto start = std::chrono::steady_clock::now();
        std::sort (out.data.begin(), out.data.end());
        const auto end = std::chrono::steady_clock::now();
        out.time_ns = static_cast<std::uint64_t>(
            std::chrono::duration_cast<std::chrono::nanoseconds> (end - start).count()
        );
        return out;
    }

    GpuSortResult run_gpu_sort (ocl::Runtime& rt, const std::vector<int>& input)
    {
        GpuSortResult out;
        out.data = input;
        out.stats = bitonic::bitonic_sort (rt, out.data);
        return out;
    }

    SortOutputs run_both_sorts (ocl::Runtime& rt, const std::vector<int>& input)
    {
        SortOutputs out;
        out.n = input.size();
        out.cpu = run_cpu_sort (input);
        out.gpu = run_gpu_sort (rt, input);
        return out;
    }

    int run_sort (const SortOutputs& outputs)
    {
        // Use GPU result as output of the OpenCL sort mode
        for (std::size_t i = 0; i < outputs.gpu.data.size(); ++i)
            std::cout << outputs.gpu.data[i]
                      << (i + 1 < outputs.gpu.data.size() ? ' ' : '\n');
        return 0;
    }

    double ns_to_ms (std::uint64_t ns)
    {
        return static_cast<double>(ns) / 1'000'000.0;
    }

    int run_benchmark (const SortOutputs& outputs)
    {
        std::cout << "\n"
                  << std::setw (10) << "N"
                  << std::setw (14) << "std::sort"
                  << std::setw (14) << "OCL total"
                  << std::setw (14) << "OCL kernel"
                  << std::setw (10) << "Speedup"
                  << "\n"
                  << std::string (62, '-') << "\n";

        // correctness check
        if (outputs.cpu.data != outputs.gpu.data)
        {
            std::cerr << "MISMATCH at N=" << outputs.n << "\n";
            return 1;
        }

        double speedup = (outputs.gpu.stats.total_ns > 0)
            ? static_cast<double>(outputs.cpu.time_ns) / static_cast<double>(outputs.gpu.stats.total_ns)
            : 0.0;

        std::cout << std::setw (10) << outputs.n
                  << std::setw (11) << std::fixed << std::setprecision (2) << ns_to_ms (outputs.cpu.time_ns) << " ms"
                  << std::setw (11) << std::fixed << std::setprecision (2) << ns_to_ms (outputs.gpu.stats.total_ns)  << " ms"
                  << std::setw (11) << std::fixed << std::setprecision (2) << ns_to_ms (outputs.gpu.stats.kernel_ns) << " ms"
                  << std::setw (9)  << std::fixed << std::setprecision (2) << speedup << "x"
                  << "\n";

        return 0;
    }

} // namespace

int main (int argc, char* argv[])
{
    try
    {
        std::filesystem::path kernel_path = DEFAULT_KERNEL_PATH;
        bool kernel_path_explicit = false;
        bool benchmark = false;
        bool verbose = false;

        for (int i = 1; i < argc; ++i)
        {
            if (std::strcmp (argv[i], "--help") == 0 ||
                std::strcmp (argv[i], "-h") == 0)
            {
                print_usage (argv[0]);
                return 0;
            }
            else if (std::strcmp (argv[i], "--benchmark") == 0)
            {
                benchmark = true;
            }
            else if (std::strcmp (argv[i], "--kernel") == 0)
            {
                if (i + 1 >= argc)
                {
                    print_usage (argv[0]);
                    throw std::runtime_error ("Missing value for --kernel");
                }

                kernel_path = argv[++i];
                kernel_path_explicit = true;
            }
            else if (std::strcmp (argv[i], "--verbose") == 0)
            {
                verbose = true;
            }
            else
            {
                print_usage (argv[0]);
                throw std::runtime_error ("Unknown argument: " + std::string (argv[i]));
            }
        }

        if (!kernel_path_explicit)
            kernel_path = resolve_default_kernel_path();

        ocl::Runtime rt;
        rt.init (verbose);
        rt.build_program (kernel_path);

        const std::vector<int> input = read_input();
        const SortOutputs outputs = run_both_sorts (rt, input);
        return benchmark ? run_benchmark (outputs) : run_sort (outputs);
    }
    catch (const std::exception& e)
    {
        std::cerr << "Error: " << e.what() << "\n";

        return 1;
    }
}
