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

    void print_usage (const char* program_name)
    {
        std::cerr << "Usage: " << program_name
                  << " [--benchmark] [--kernel <path>] [--help]\n";
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

    std::vector<int> read_input()
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

    int run_sort (ocl::Runtime& rt, const std::vector<int>& input)
    {
        std::vector<int> data = input;
        bitonic::bitonic_sort (rt, data);

        for (std::size_t i = 0; i < data.size(); ++i)
            std::cout << data[i] << (i + 1 < data.size() ? ' ' : '\n');

        return 0;
    }

    double ns_to_ms (std::uint64_t ns)
    {
        return static_cast<double>(ns) / 1'000'000.0;
    }

    int run_benchmark (ocl::Runtime& rt, const std::vector<int>& input)
    {
        std::cout << "\n"
                  << std::setw (10) << "N"
                  << std::setw (14) << "std::sort"
                  << std::setw (14) << "OCL total"
                  << std::setw (14) << "OCL kernel"
                  << std::setw (10) << "Speedup"
                  << "\n"
                  << std::string (62, '-') << "\n";

        const std::size_t n = input.size();
        std::vector<int> cpu_data = input;
        auto cpu_start = std::chrono::steady_clock::now();
        std::sort (cpu_data.begin(), cpu_data.end());
        auto cpu_end = std::chrono::steady_clock::now();
        auto cpu_ns  = std::chrono::duration_cast<std::chrono::nanoseconds>(cpu_end - cpu_start).count();

        std::vector<int> ocl_data = input;
        bitonic::SortResult result = bitonic::bitonic_sort (rt, ocl_data);

        // correctness check
        if (cpu_data != ocl_data)
        {
            std::cerr << "MISMATCH at N=" << n << "\n";
            return 1;
        }

        double speedup = (result.total_ns > 0)
            ? static_cast<double>(cpu_ns) / static_cast<double>(result.total_ns)
            : 0.0;

        std::cout << std::setw (10) << n
                  << std::setw (11) << std::fixed << std::setprecision (2) << ns_to_ms (cpu_ns) << " ms"
                  << std::setw (11) << std::fixed << std::setprecision (2) << ns_to_ms (result.total_ns)  << " ms"
                  << std::setw (11) << std::fixed << std::setprecision (2) << ns_to_ms (result.kernel_ns) << " ms"
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
            else
            {
                print_usage (argv[0]);
                throw std::runtime_error ("Unknown argument: " + std::string (argv[i]));
            }
        }

        if (!kernel_path_explicit)
            kernel_path = resolve_default_kernel_path();

        ocl::Runtime rt;
        rt.init();
        rt.build_program (kernel_path);

        const std::vector<int> input = read_input();
        return benchmark ? run_benchmark (rt, input) : run_sort (rt, input);
    }
    catch (const std::exception& e)
    {
        std::cerr << "Error: " << e.what() << "\n";

        return 1;
    }
}
