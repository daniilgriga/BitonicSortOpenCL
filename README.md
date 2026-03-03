# BitonicSort - OpenCL

![CI](https://github.com/daniilgriga/BitonicSort/actions/workflows/ci.yml/badge.svg)

GPU-accelerated integer sorting via the **bitonic sort** algorithm, implemented in OpenCL C and driven by a C++17 host program.

## Table of Contents

- [Algorithm](#algorithm)
- [Requirements](#requirements)
- [Build](#build)
- [Usage](#usage)
- [Testing](#testing)
- [Benchmark](#benchmark)
- [Implementation notes](#implementation-notes)

---

## Algorithm

Bitonic sort is a **comparison network**: the sequence of compare-exchange operations is fixed at compile time and does not depend on the input data. This makes it directly mappable to GPU parallelism - each step launches one OpenCL kernel where every work-item independently swaps one pair of elements.

The diagram below shows the full network for **N = 8** (used here as an illustration; the implementation supports arbitrary N up to ~1M). Each vertical connector is a **comparator** - it reads two elements and, if they are out of order, swaps them. The arrow points to where the **smaller** value goes. All comparators within one shaded column are **data-independent** and execute in parallel as a single kernel launch.

<p align="center">
    <img src="docs/bitonic_network.svg" alt="Bitonic sorting network for N = 8" width="520"/>
</p>

The sort proceeds in three phases: k = 2 builds sorted pairs with alternating directions, k = 4 merges pairs into sorted groups of 4, and k = 8 performs the final merge producing a fully sorted sequence.

---

## Requirements

| Dependency | Version |
|------------|---------|
| CMake      | >= 3.15 |
| C++        | 17      |
| OpenCL     | >= 1.2  |
| GTest      | any     |

Tested on **POCL** (CPU) and **AMD ROCm** (Vega GPU). Any OpenCL 1.2-compatible runtime should work.

---

## Build

```bash
# Release
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build

# Debug + unit tests
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTS=ON
cmake --build build

# With sanitizers
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTS=ON -DSANITIZE=ON
cmake --build build
```

The binary is `build/bitonic_sort`. The kernel is automatically copied to `build/kernels/bitonic.cl` by CMake.

---

## Usage

### Sort mode

Reads from **stdin**, writes to **stdout**. Diagnostics go to **stderr**.

Input format:
```
N
a[0] a[1] ... a[N-1]
```

Output format:
```
a[0] a[1] ... a[N-1]
```

All values are 32-bit signed integers. For N = 0, output is empty.

Example:
```bash
echo "5
3 1 4 1 5" | ./build/bitonic_sort
# stdout: 1 1 3 4 5
```

Override the default kernel path with `--kernel`:
```bash
./build/bitonic_sort --kernel /path/to/bitonic.cl
```

### Benchmark mode

Compares `std::sort` (CPU baseline) against OpenCL across a range of array sizes:

```bash
./build/bitonic_sort --benchmark
```

```
         N     std::sort     OCL total    OCL kernel   Speedup
--------------------------------------------------------------
      1024       0.04 ms       6.90 ms       0.07 ms     0.01x
      8192       0.93 ms       0.55 ms       0.21 ms     1.70x
     65536       3.13 ms       1.54 ms       1.03 ms     2.03x
    262144      13.79 ms       8.61 ms       7.38 ms     1.60x
    524288      29.35 ms      26.33 ms      21.39 ms     1.11x
   1048576      61.90 ms      55.98 ms      47.46 ms     1.11x
```

---

## Testing

### Unit tests

```bash
ctest --test-dir build --output-on-failure
```

16 tests total: 8 for `opencl_runtime`, 8 for `bitonic_runner`.

### E2E tests

```bash
./tests/e2e/run_all.sh ./build/bitonic_sort
```

23 test cases covering: empty input, single element, power-of-two and non-power-of-two sizes, negative values, `INT_MIN`/`INT_MAX` boundary values, duplicates, and large arrays up to N = 100,000.

---

## Benchmark

The selected OpenCL platform and device are printed to stderr on startup.

### AMD Vega (gfx90c) - integrated GPU

<p align="center">
  <img src="docs/benchmark.svg" alt="Benchmark: std::sort vs OpenCL bitonic sort" width="660"/>
</p>

OpenCL outperforms `std::sort` starting around N = 8,192. For small N, host-device transfer overhead dominates.

### NVIDIA GTX 1080

<!-- TODO -->
