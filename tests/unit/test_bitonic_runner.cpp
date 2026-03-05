#include "bitonic_runner.hpp"

#include <algorithm>
#include <climits>
#include <random>
#include <string>
#include <vector>

#include <gtest/gtest.h>

class BitonicRunnerTest : public ::testing::Test
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
        const bitonic::SortResult result = bitonic::bitonic_sort (rt, actual);

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

TEST_F (BitonicRunnerTest, HandlesEmptyArray)
{
    expect_sorted_like_std ({});
}

TEST_F (BitonicRunnerTest, HandlesSingleElement)
{
    expect_sorted_like_std ({42});
}

TEST_F (BitonicRunnerTest, HandlesAlreadySorted)
{
    expect_sorted_like_std ({1, 2, 3, 4, 5, 6, 7, 8});
}

TEST_F (BitonicRunnerTest, HandlesReverseSorted)
{
    expect_sorted_like_std ({8, 7, 6, 5, 4, 3, 2, 1});
}

TEST_F (BitonicRunnerTest, HandlesDuplicates)
{
    expect_sorted_like_std ({5, 1, 5, 3, 3, 2, 5, 1, 2, 2});
}

TEST_F (BitonicRunnerTest, HandlesAllEqualElements)
{
    std::vector<int> input (257, 42);
    expect_sorted_like_std (input);
}

TEST_F (BitonicRunnerTest, HandlesNonPowerOfTwoSize)
{
    expect_sorted_like_std ({9, 4, 1, 7, 3, 8, 2, 6, 5});
}

TEST_F (BitonicRunnerTest, HandlesIntBoundaries)
{
    expect_sorted_like_std ({INT_MAX, INT_MIN, 0, -1, 1, INT_MAX, INT_MIN});
}

TEST_F (BitonicRunnerTest, MatchesStdSortForRandomCases)
{
    constexpr int seed = 20260304;
    std::mt19937 gen (seed);
    std::uniform_int_distribution<int> size_dist (0, 1024);
    std::uniform_int_distribution<int> val_dist (-100000, 100000);

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

TEST_F (BitonicRunnerTest, ReusesRuntimeForMultipleCalls)
{
    const std::vector<std::vector<int>> cases = {
        {5, 4, 3, 2, 1},
        {INT_MAX, 0, INT_MIN, 7, -7, 7},
        {9, 1, 8, 2, 7, 3, 6, 4, 5},
    };

    for (const auto& input : cases)
        expect_sorted_like_std (input);
}

TEST_F (BitonicRunnerTest, LargeDeterministicNonPowerOfTwo)
{
    constexpr int seed = 424242;
    std::mt19937 gen (seed);
    std::uniform_int_distribution<int> dist (-2'000'000, 2'000'000);

    std::vector<int> input (5003);
    for (int& x : input)
        x = dist (gen);

    expect_sorted_like_std (input);
}
