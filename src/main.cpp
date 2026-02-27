#include "bitonic_runner.hpp"
#include "opencl_runtime.hpp"

#include <algorithm>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <random>
#include <stdexcept>
#include <vector>

namespace {

    constexpr const char* DEFAULT_KERNEL_PATH = "kernels/bitonic.cl";

    int run_sort (ocl::Runtime& rt)
    {
        int n;
        if (!(std::cin >> n) || n < 0)
        {
            std::cerr << "Error: expected non-negative integer N\n";
            return 1;
        }

        std::vector<int> data (n);
        for (int i = 0; i < n; ++i)
        {
            if (!(std::cin >> data[i]))
            {
                std::cerr << "Error: expected " << n << " integers, got " << i << "\n";
                return 1;
            }
        }

        bitonic::bitonic_sort (rt, data);

        for (int i = 0; i < n; ++i)
            std::cout << data[i] << (i + 1 < n ? ' ' : '\n');

        return 0;
    }

    double ns_to_ms (std::uint64_t ns)
    {
        return static_cast<double>(ns) / 1'000'000.0;
    }

    int run_benchmark (ocl::Runtime& rt)
    {
        constexpr int seed = 13;
        const std::vector<int> sizes = {1024, 8192, 65536, 262144, 524288, 1048576};

        std::cout << "\n"
                  << std::setw (10) << "N"
                  << std::setw (14) << "std::sort"
                  << std::setw (14) << "OCL total"
                  << std::setw (14) << "OCL kernel"
                  << std::setw (10) << "Speedup"
                  << "\n"
                  << std::string (62, '-') << "\n";

        for (int n : sizes)
        {
            std::mt19937 gen (seed);
            std::uniform_int_distribution<int> dist (0, n);
            std::vector<int> data (n);

            for (int& x : data)
                x = dist (gen);

            // CPU baseline
            std::vector<int> cpu_data = data;
            auto cpu_start = std::chrono::steady_clock::now();
            std::sort (cpu_data.begin(), cpu_data.end());
            auto cpu_end = std::chrono::steady_clock::now();
            auto cpu_ns  = std::chrono::duration_cast<std::chrono::nanoseconds>(cpu_end - cpu_start).count();

            std::vector<int> ocl_data = data;
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
        }

        return 0;
    }

} // namespace

int main (int argc, char* argv[])
{
    try
    {
        std::filesystem::path kernel_path = DEFAULT_KERNEL_PATH;
        bool benchmark = false;

        for (int i = 1; i < argc; ++i)
        {
            if (std::strcmp (argv[i], "--benchmark") == 0)
            {
                benchmark = true;
            }
            else if (std::strcmp (argv[i], "--kernel") == 0 && i + 1 < argc)
            {
                kernel_path = argv[++i];
            }
        }

        ocl::Runtime rt;
        rt.init();
        rt.build_program (kernel_path);

        return benchmark ? run_benchmark (rt) : run_sort (rt);
    }
    catch (const std::exception& e)
    {
        std::cerr << "Error: " << e.what() << "\n";

        return 1;
    }
}
