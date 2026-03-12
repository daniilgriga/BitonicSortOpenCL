![Linux](https://img.shields.io/badge/Linux-FCC624?style=for-the-badge&logo=linux&logoColor=black)
![C++17](https://img.shields.io/badge/C++17-%2300599C.svg?style=for-the-badge&logo=c%2B%2B&logoColor=white)
![CMake](https://img.shields.io/badge/CMake-%23008FBA.svg?style=for-the-badge&logo=cmake&logoColor=white)

# Bitonic Sort - OpenCL

Integer sorting with the **bitonic sort network** on OpenCL devices (GPU preferred, CPU runtime supported for development/debug).

This repository contains a full host + device pipeline:
- OpenCL platform/device discovery and runtime setup
- Kernel compilation with detailed diagnostics
- Bitonic network execution on device memory
- CLI mode for stdin/stdout sorting
- Unit tests + end-to-end datasets

<p align="center">
    <img src="docs/pipeline.svg" alt="OpenCL execution pipeline" width="860"/>
</p>

---

## Algorithm

Bitonic sort is a **comparison network**: the compare-exchange schedule is data-independent, so each stage can be executed in parallel.

The network illustration below shows `N = 8`:

<p align="center">
    <img src="docs/bitonic_network.svg" alt="Bitonic sorting network for N = 8" width="520"/>
</p>

Implementation details:
- Two kernels: `bitonic_sort_local` (local-memory sort within work-groups) and `bitonic_sort_step_half_ctz` (global merge across groups)
- Host first runs the local kernel to sort blocks in shared memory, then launches global `(k, j)` stages for the remaining merge phases
- Each stage compares independent pairs and swaps if required
- OpenCL events are collected to measure aggregate kernel time

---

## Input/Output Contract

`bitonic_sort` reads from `stdin` and writes to `stdout`.

Input format:
```text
N
a0 a1 ... a(N-1)
```

Output format:
```text
b0 b1 ... b(N-1)
```

Contract:
- `N` must be a non-negative integer
- values are `int32`
- output is sorted in non-decreasing order
- output length is exactly `N`
- for `N = 0`, output is empty

### Non-power-of-two `N`

Bitonic networks are natural for sizes `2^k`.
This implementation supports arbitrary `N` by:
1. padding to next power of two with `INT_MAX`,
2. sorting on device,
3. trimming back to original length `N`.

---

## Requirements

| Dependency | Version                          |
|------------|----------------------------------|
| CMake      | >= 3.15                          |
| C++        | 17                               |
| OpenCL     | >= 1.2 (runtime + headers)       |
| CLI11      | any                              |
| GTest      | any (only when `BUILD_TESTS=ON`) |

> [!NOTE]
> Some OpenCL runtimes report compiler availability but still fail to build kernels (see [Known Runtime Issues](#known-runtime-issues)).
> Recommended flow: install dependencies externally and let CMake resolve them via `find_package`.
> Optional fallback: configure with `-DBITONIC_FETCH_DEPS=ON` to auto-fetch missing dependencies.

Ubuntu packages:
```bash
sudo apt-get update
sudo apt-get install -y cmake ninja-build g++ libcli11-dev opencl-clhpp-headers ocl-icd-opencl-dev pocl-opencl-icd libgtest-dev
```

Windows (vcpkg):
```powershell
vcpkg install cli11:x64-windows opencl:x64-windows gtest:x64-windows
```

---

## Build

```bash
# release (app only; BUILD_TESTS=OFF by default)
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build

# debug + unit tests
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTS=ON
cmake --build build

# with sanitizers
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTS=ON -DSANITIZE=ON
cmake --build build

# Windows checker-style command (Ninja + clang)
cmake -S . -B build -G Ninja -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_C_COMPILER=clang -DCMAKE_BUILD_TYPE=Release
cmake --build build

# optional fallback if dependencies are not installed system-wide
cmake -S . -B build -G Ninja -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_C_COMPILER=clang -DCMAKE_BUILD_TYPE=Release -DBITONIC_FETCH_DEPS=ON
cmake --build build
```

Binary: `build/bitonic_sort`
Kernel source copied by CMake to: `build/kernels/bitonic.cl`

### CMake layout

- `CMakeLists.txt`: project targets, options, and build flow
- `cmake/BitonicDependencies.cmake`: dependency resolution (`OpenCL`, `CLI11`, `GTest`, OpenCL C++ headers)

---

## Usage

### Sort mode

Reads from **stdin**, writes to **stdout**. Device diagnostics go to **stderr** when `--verbose` is enabled.

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

Show CLI help:
```bash
./build/bitonic_sort --help
```

> [!NOTE]
> For non-Linux hosts, passing explicit `--kernel` is the most reliable option
> on Linux, if `kernels/bitonic.cl` is not found in current working directory, binary also tries `<binary_dir>/kernels/bitonic.cl`.

### Benchmark mode

`--benchmark` runs one benchmark row for input read from `stdin`:

```bash
echo "8
8 7 6 5 4 3 2 1" | ./build/bitonic_sort --benchmark
```

`--benchmark-random` runs fixed random benchmark sizes:

```bash
./build/bitonic_sort --benchmark-random
```

`--benchmark` and `--benchmark-random` are mutually exclusive.

Example output below is for `--benchmark-random`:

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

Current suite includes runtime tests and bitonic correctness tests.

Important:
- compile-dependent tests are skipped on non-buildable OpenCL runtimes
- this is expected behavior, not a false pass

### E2E tests

```bash
./tests/e2e/run_all.sh ./build/bitonic_sort
```

The script passes an absolute `--kernel` path automatically (`<repo>/kernels/bitonic.cl`).

E2E facts:
- `24` cases (`001..024`)
- includes empty/single/duplicates/boundary values/random
- includes both power-of-two and non-power-of-two sizes
- maximum case: `N = 1,000,000` (`023.dat`)
- per-test timeout in script: `120s`

`E2E` is intentionally run via `tests/e2e/run_all.sh` and is **not** registered in `ctest`.

Regenerate E2E datasets:
```bash
python3 tests/e2e/generate_tests.py
```

### Test Matrix

| Layer |          Tool          |                              Scope                             |
|-------|------------------------|----------------------------------------------------------------|
| Unit  | `ctest` / GoogleTest   | Runtime init, build errors, bitonic correctness vs `std::sort` |
| E2E   | `tests/e2e/run_all.sh` | CLI contract (`stdin -> stdout`) on fixed `.dat/.ans` corpus   |

---

## Benchmark

OpenCL platform/device diagnostics are printed to `stderr` only when `--verbose` is passed.
Charts are generated by:

```bash
python3 docs/gen_benchmark.py
```

### AMD Vega (gfx90c:xnack+) - integrated GPU

<p align="center">
    <img src="docs/benchmark.svg" alt="Benchmark on AMD Vega (gfx90c:xnack+)" width="660"/>
</p>

On this setup, OpenCL becomes faster than `std::sort` starting from `N = 8,192`.
For small `N`, transfer and launch overhead dominates.

### NVIDIA GTX 1080 Ti

<p align="center">
    <img src="docs/benchmark_gtx1080.svg" alt="Benchmark on NVIDIA GTX 1080 Ti" width="660"/>
</p>

For GTX 1080 Ti, OpenCL is slower at `N = 1,024` and `N = 8,192`, then significantly faster starting from `N = 65,536`.

Interpretation guidance:
- compare both `OCL total` and `OCL kernel` columns
- for small `N`, transfer/launch overhead dominates
- for larger `N`, throughput benefits are visible on real GPU runtimes

### Metric meaning

- `OCL total`: end-to-end OpenCL path measured on host wall-clock (buffer upload, kernel launches, synchronization, buffer readback, and host-side overhead).
- `OCL kernel`: sum of OpenCL event profiling durations for kernel execution on the device.
- `Speedup` in this table is computed as `std::sort / OCL total`, because it reflects real application-visible performance.
- If `OCL total` is much larger than `OCL kernel`, the bottleneck is transfer/launch/host overhead rather than pure kernel compute.

### Why Crossover Differs Across Machines

- `OCL total` includes copy and launch overhead, not only kernel compute.
- On integrated GPUs, host/device memory is often shared, so data-transfer overhead is lower.
- On discrete GPUs, host-device copies usually go over PCIe, which can dominate at small `N` (for example around `N = 8,192`).
- `std::sort` baseline depends on CPU performance; with a faster CPU, GPU crossover tends to happen at larger `N`.

---

## Known Runtime Issues

Some environments expose OpenCL devices that cannot actually build kernels.
Typical symptom:
- `CL_BUILD_PROGRAM_FAILURE (-11)` during kernel build

Runtime behavior in this project:
- `init()` probes candidate devices with a tiny kernel build
- prefers buildable GPU, then buildable non-GPU
- falls back to best available device with warning if probe fails everywhere
- `build_program()` reports detailed diagnostics:
    - kernel path
    - platform/device
    - build options
    - error code + build status + build log

This design makes CI and cross-machine debugging significantly easier.

## Known Sanitizer Limitations (ROCm)

On some AMD ROCm setups, `ASan/LSan` reports leaks originating in vendor runtime libraries (`/opt/rocm/lib/libamdocl64.so`, `libhsa-runtime64.so`) during OpenCL platform discovery (`clGetPlatformIDs`).

Notes:
- unit tests may pass functionally, but `ctest` fails because leak-check treats external runtime leaks as fatal;
- stack traces point into ROCm/HSA shared libraries, not project code.

Recommended local command on affected machines:
```bash
ASAN_OPTIONS=detect_leaks=0 ctest --test-dir build --output-on-failure
```

In CI, leak suppressions are configured for known external runtime leaks (`.github/lsan.supp`).
