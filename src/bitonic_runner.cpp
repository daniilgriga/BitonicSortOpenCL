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
        {
            if (n > std::numeric_limits<std::size_t>::max() / 2)
                throw std::runtime_error ("next_pow2 overflow");
            n <<= 1;
        }

        return n;
    }

    int trailing_zeroes_pow2 (std::size_t x)
    {
        int p = 0;
        while ((x & 1u) == 0u)
        {
            x >>= 1u;
            ++p;
        }
        return p;
    }

    cl_int to_cl_int_checked (std::size_t value, const char* what)
    {
        if (value > static_cast<std::size_t>(std::numeric_limits<cl_int>::max()))
            throw std::runtime_error (std::string (what) + " does not fit into cl_int");

        return static_cast<cl_int>(value);
    }

    std::size_t pick_local_block_size (
        const cl::Kernel& local_kernel,
        const ocl::Runtime& rt,
        std::size_t padded_size)
    {
        std::size_t block_size = std::min<std::size_t> (256, padded_size);

        const std::size_t kernel_wg_limit =
            local_kernel.getWorkGroupInfo<CL_KERNEL_WORK_GROUP_SIZE> (rt.device());
        block_size = std::min (block_size, kernel_wg_limit);

        const cl_ulong device_local_mem = rt.device().getInfo<CL_DEVICE_LOCAL_MEM_SIZE>();
        const std::size_t mem_wg_limit = static_cast<std::size_t>(device_local_mem / sizeof(int));
        block_size = std::min (block_size, mem_wg_limit);

        // save power-of-two block size for local bitonic
        std::size_t pow2 = 1;
        while ((pow2 << 1u) <= block_size)
            pow2 <<= 1u;
        block_size = pow2;

        while (block_size > 1 && (padded_size % block_size) != 0)
            block_size >>= 1u;

        return block_size;
    }

    std::size_t prepare_padded_data (const std::vector<int>& input, std::vector<int>& padded_data)
    {
        const std::size_t padded_size = next_pow2 (input.size());
        if (padded_size > static_cast<std::size_t>(std::numeric_limits<cl_int>::max()))
            throw std::runtime_error ("Input is too large for cl_int kernel args");

        padded_data.assign (padded_size, INT_MAX);
        std::copy (input.begin(), input.end(), padded_data.begin());
        return padded_size;
    }

    std::size_t enqueue_local_phase (
        ocl::Runtime& rt,
        cl::Buffer& buffer,
        std::size_t padded_size,
        cl::Kernel& local_kernel,
        std::vector<cl::Event>& events)
    {
        std::size_t global_start_k = 2;
        const std::size_t block_size = pick_local_block_size (local_kernel, rt, padded_size);
        if (block_size < 2)
            return global_start_k;

        local_kernel.setArg (0, buffer);
        local_kernel.setArg (1, cl::Local (block_size * sizeof(int)));

        cl::Event local_event;
        rt.queue().enqueueNDRangeKernel (
            local_kernel,
            cl::NullRange,
            cl::NDRange (padded_size),
            cl::NDRange (block_size),
            nullptr,
            &local_event
        );
        events.push_back (local_event);
        return block_size << 1u;
    }

    void enqueue_global_merge_phase (
        ocl::Runtime& rt,
        cl::Buffer& buffer,
        std::size_t padded_size,
        std::size_t global_start_k,
        cl::Kernel& global_kernel,
        std::vector<cl::Event>& events)
    {
        for (std::size_t k = global_start_k; k <= padded_size; k <<= 1)
        {
            for (std::size_t j = k >> 1; j > 0; j >>= 1)
            {
                const cl_int j_arg = to_cl_int_checked (j, "j");
                const cl_int k_arg = to_cl_int_checked (k, "k");
                const cl_int ctz_arg = static_cast<cl_int>(trailing_zeroes_pow2 (j));

                global_kernel.setArg (0, buffer);
                global_kernel.setArg (1, j_arg);
                global_kernel.setArg (2, k_arg);
                global_kernel.setArg (3, ctz_arg);

                cl::Event event;
                rt.queue().enqueueNDRangeKernel (
                    global_kernel,
                    cl::NullRange,
                    cl::NDRange (padded_size / 2),
                    cl::NullRange,
                    nullptr,
                    &event
                );
                events.push_back (event);
            }
        }
    }

    std::uint64_t collect_kernel_time_ns (ocl::Runtime& rt, const std::vector<cl::Event>& events)
    {
        rt.queue().finish();

        std::uint64_t kernel_ns = 0;
        for (const cl::Event& event : events)
        {
            const cl_ulong start = event.getProfilingInfo<CL_PROFILING_COMMAND_START>();
            const cl_ulong end   = event.getProfilingInfo<CL_PROFILING_COMMAND_END>();
            kernel_ns += static_cast<std::uint64_t>(end - start);
        }
        return kernel_ns;
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
        std::vector<int> padded_data;
        const std::size_t padded_size = prepare_padded_data (data, padded_data);

        cl::Buffer buffer (rt.context(), CL_MEM_READ_WRITE, padded_size * sizeof(int));
        rt.queue().enqueueWriteBuffer (
            buffer, CL_TRUE, 0, padded_size * sizeof(int), padded_data.data());

        cl::Kernel global_kernel (rt.program(), "bitonic_sort_step_half_ctz");
        cl::Kernel local_kernel (rt.program(), "bitonic_sort_local");

        std::vector<cl::Event> events;
        const std::size_t global_start_k = enqueue_local_phase (
            rt, buffer, padded_size, local_kernel, events
        );
        enqueue_global_merge_phase (
            rt, buffer, padded_size, global_start_k, global_kernel, events
        );
        const std::uint64_t kernel_ns = collect_kernel_time_ns (rt, events);

        rt.queue().enqueueReadBuffer (
            buffer, CL_TRUE, 0, padded_size * sizeof(int), padded_data.data());
        padded_data.resize (input_size);
        data = std::move (padded_data);

        const auto total_end = std::chrono::steady_clock::now();
        const auto total_ns = static_cast<std::uint64_t>(
            std::chrono::duration_cast<std::chrono::nanoseconds>(total_end - total_start).count());

        return {total_ns, kernel_ns};
    }
} // namespace bitonic
