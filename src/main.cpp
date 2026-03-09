#include "bitonic_runner.hpp"
#include "opencl_runtime.hpp"

#include <CLI/CLI.hpp>

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <random>
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

    void print_benchmark_header ()
    {
        std::cout << "\n"
                  << std::setw (10) << "N"
                  << std::setw (14) << "std::sort"
                  << std::setw (14) << "OCL total"
                  << std::setw (14) << "OCL kernel"
                  << std::setw (10) << "Speedup"
                  << "\n"
                  << std::string (62, '-') << "\n";
    }

    int print_benchmark_row (const SortOutputs& outputs)
    {
        if (outputs.cpu.data != outputs.gpu.data)
        {
            std::cerr << "MISMATCH at N=" << outputs.n << "\n";
            return 1;
        }

        const double speedup = (outputs.gpu.stats.total_ns > 0)
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

    int run_benchmark (const SortOutputs& outputs)
    {
        print_benchmark_header();
        return print_benchmark_row (outputs);
    }

    int run_benchmark_random (ocl::Runtime& rt)
    {
        constexpr int seed = 13;
        const std::vector<int> sizes = {1024, 8192, 65536, 262144, 524288, 1048576};

        std::mt19937 gen (seed);
        print_benchmark_header();

        for (const int n : sizes)
        {
            std::uniform_int_distribution<int> dist (0, n);
            std::vector<int> data (static_cast<std::size_t>(n));
            for (int& x : data)
                x = dist (gen);

            const SortOutputs outputs = run_both_sorts (rt, data);
            if (print_benchmark_row (outputs) != 0)
                return 1;
        }

        return 0;
    }

} // namespace

int main (int argc, char* argv[])
{
    try
    {
        std::string kernel_path_arg;
        bool benchmark = false;
        bool benchmark_random = false;
        bool verbose = false;

        CLI::App app {"OpenCL bitonic sort"};
        app.add_flag (
            "--benchmark",
            benchmark,
            "Run one stdin-based benchmark row (CPU vs OpenCL)"
        );
        app.add_flag (
            "--benchmark-random",
            benchmark_random,
            "Run random benchmark table for predefined sizes"
        );
        auto* kernel_opt = app.add_option (
            "--kernel",
            kernel_path_arg,
            "Path to OpenCL kernel source"
        );
        app.add_flag (
            "--verbose",
            verbose,
            "Print OpenCL platform/device diagnostics"
        );

        CLI11_PARSE (app, argc, argv);

        std::filesystem::path kernel_path = resolve_default_kernel_path();
        if (kernel_opt->count() > 0)
            kernel_path = kernel_path_arg;

        if (benchmark && benchmark_random)
        {
            throw std::runtime_error (
                "Use either --benchmark (stdin-based) or --benchmark-random, not both"
            );
        }

        ocl::Runtime rt;
        rt.init (verbose);
        rt.build_program (kernel_path);

        if (benchmark_random)
            return run_benchmark_random (rt);

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
