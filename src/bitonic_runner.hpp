#pragma once

#include "opencl_runtime.hpp"

#include <cstdint>
#include <vector>

namespace bitonic
{

    struct SortResult
    {
        std::uint64_t total_ns;
        std::uint64_t kernel_ns;
    };

    SortResult bitonic_sort (ocl::Runtime& rt, std::vector<int>& data);

} // namespace bitonic
