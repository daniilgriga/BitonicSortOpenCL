#include "bitonic_runner.hpp"

#include <algorithm>
#include <random>
#include <string>
#include <vector>

#include <gtest/gtest.h>

class BitonicSortTest : public ::testing::Test
{
protected:
    ocl::Runtime rt;

    void SetUp() override
    {
        try
        {
            rt.init();
            rt.build_program ("kernels/bitonic.cl");
        }
        catch (const std::exception& e)
        {
            GTEST_SKIP() << "OpenCL is unavailable in test environment: " << e.what();
        }
    }

    void expect_sorted_like_std (const std::vector<int>& input)
    {
        std::vector<int> expected = input;
        std::sort (expected.begin(), expected.end());

        std::vector<int> actual = input;
        bitonic::SortResult result = bitonic::bitonic_sort (rt, actual);

        EXPECT_EQ (actual, expected);
        if (input.size() <= 1)
        {
            EXPECT_EQ (result.total_ns, 0u);
            EXPECT_EQ (result.kernel_ns, 0u);
        }
        else
        {
            EXPECT_GE (result.total_ns, result.kernel_ns);
        }
    }
};

TEST_F (BitonicSortTest, HandlesEmptyArray)
{
    expect_sorted_like_std ({});
}

TEST_F (BitonicSortTest, HandlesSingleElement)
{
    expect_sorted_like_std ({42});
}

TEST_F (BitonicSortTest, HandlesAlreadySorted)
{
    expect_sorted_like_std ({1, 2, 3, 4, 5, 6, 7, 8});
}

TEST_F (BitonicSortTest, HandlesReverseSorted)
{
    expect_sorted_like_std ({8, 7, 6, 5, 4, 3, 2, 1});
}

TEST_F (BitonicSortTest, HandlesDuplicates)
{
    expect_sorted_like_std ({5, 1, 5, 3, 3, 2, 5, 1, 2, 2});
}

TEST_F (BitonicSortTest, HandlesNonPowerOfTwoSize)
{
    expect_sorted_like_std ({9, 4, 1, 7, 3, 8, 2, 6, 5});
}

TEST_F (BitonicSortTest, MatchesStdSortForReferenceSizes)
{
    constexpr int seed = 20260302;
    std::mt19937 gen (seed);
    std::uniform_int_distribution<int> dist (-10000, 10000);

    const std::vector<std::size_t> sizes = {2, 3, 4, 5, 8, 16, 31, 32, 33, 1000};
    for (std::size_t size : sizes)
    {
        std::vector<int> input (size);
        for (int& x : input)
            x = dist (gen);

        SCOPED_TRACE ("size=" + std::to_string (size));
        expect_sorted_like_std (input);
    }
}

TEST_F (BitonicSortTest, MatchesStdSortForMultipleRandomRuns)
{
    constexpr int seed = 20260301;
    std::mt19937 gen (seed);
    std::uniform_int_distribution<int> size_dist (0, 512);
    std::uniform_int_distribution<int> val_dist (-5000, 5000);

    for (int run = 0; run < 20; ++run)
    {
        const int size = size_dist (gen);
        std::vector<int> input (static_cast<std::size_t>(size));
        for (int& x : input)
            x = val_dist (gen);

        SCOPED_TRACE ("run=" + std::to_string (run) + ", size=" + std::to_string (size));
        expect_sorted_like_std (input);
    }
}
