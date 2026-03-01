#include <algorithm>
#include <chrono>
#include <climits>
#include <limits>
#include <stdexcept>
#include <vector>

#include "bitonic_runner.hpp"

namespace
{
    std::size_t next_pow2 (std::size_t x)
    {
        std::size_t n = 1;
        while (n < x)
            n <<= 1;

        return n;
    }
} // namespace

namespace bitonic
{

    SortResult bitonic_sort (ocl::Runtime& rt, std::vector<int>& data)
    {
        const std::size_t input_size = data.size();
        if (input_size <= 1)
            return {0, 0};

        const auto total_start = std::chrono::steady_clock::now();
        std::uint64_t kernel_ns = 0;

        const std::size_t padded_size = next_pow2(input_size);
        if (padded_size > static_cast<std::size_t>(std::numeric_limits<int>::max()))
            throw std::runtime_error("Input is too large for int kernel args");

        std::vector<int> padded_data(padded_size, INT_MAX);
        std::copy(data.begin(), data.end(), padded_data.begin());

        cl::Buffer buffer(rt.context(), CL_MEM_READ_WRITE, padded_size * sizeof(int));

        rt.queue().enqueueWriteBuffer(
            buffer, CL_TRUE, 0, padded_size * sizeof(int), padded_data.data());

        cl::Kernel kernel(rt.program(), "bitonic_sort_step");

        std::vector<cl::Event> events;

        for (std::size_t k = 2; k <= padded_size; k <<= 1)
        {
            for (std::size_t j = k >> 1; j > 0; j >>= 1)
            {
                kernel.setArg(0, buffer);
                kernel.setArg(1, static_cast<int>(j));
                kernel.setArg(2, static_cast<int>(k));

                cl::Event event;
                rt.queue().enqueueNDRangeKernel(
                    kernel,
                    cl::NullRange,
                    cl::NDRange(padded_size),
                    cl::NullRange,
                    nullptr,
                    &event);

                events.push_back(event);
            }
        }

        rt.queue().finish();

        for (const cl::Event& event : events)
        {
            const cl_ulong start = event.getProfilingInfo<CL_PROFILING_COMMAND_START>();
            const cl_ulong end   = event.getProfilingInfo<CL_PROFILING_COMMAND_END>();
            kernel_ns += static_cast<std::uint64_t>(end - start);
        }

        rt.queue().enqueueReadBuffer(
            buffer, CL_TRUE, 0, padded_size * sizeof(int), padded_data.data());

        padded_data.resize(input_size);
        data = std::move(padded_data);

        const auto total_end = std::chrono::steady_clock::now();
        const auto total_ns = static_cast<std::uint64_t>(
            std::chrono::duration_cast<std::chrono::nanoseconds>(total_end - total_start).count());

        return {total_ns, kernel_ns};
    }
} // namespace bitonic
