# Building Neozip

## Prerequisites

- **CMake** ≥ 3.14 (up to 4.2.1 supported)
- A **C11** compiler (GCC ≥ 5, Clang ≥ 5, MSVC ≥ 2013, Intel ICC, NVIDIA HPC)
- Optional: C++ compiler for Google Test, benchmarks, and fuzz targets
- Optional: Google Test (fetched automatically by CMake)
- Optional: Google Benchmark (fetched automatically)

---

## Quick Start

### Default Build (Native API)

```bash
cd neozip
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
```

This produces `libneozip.so` (or `.dylib` / `.dll`) and `libneozip.a` with the
`zng_` prefixed API.

### zlib-Compatible Build

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release -DZLIB_COMPAT=ON
cmake --build build -j$(nproc)
```

This produces `libz.so` and `libz.a` with standard zlib symbol names,
suitable as a drop-in replacement for system zlib.

### Install

```bash
cmake --install build --prefix /usr/local
```

---

## CMake Configuration Options

### Core Options

| Option | Type | Default | Description |
|---|---|---|---|
| `ZLIB_COMPAT` | BOOL | `OFF` | Build with zlib-compatible API. Produces `libz` instead of `libneozip`. All public symbols use standard zlib names. Forces `WITH_GZFILEOP=ON`. |
| `ZLIB_ALIASES` | BOOL | `ON` | Provide zlib-compatible CMake targets regardless of `ZLIB_COMPAT`. |
| `WITH_GZFILEOP` | BOOL | `ON` | Compile gzip file I/O functions (`gzopen`, `gzread`, `gzwrite`, etc.). Forced `ON` in compat mode. |
| `WITH_OPTIM` | BOOL | `ON` | Enable architecture-specific optimisations. Set `OFF` to use only generic C code. |
| `WITH_NEW_STRATEGIES` | BOOL | `ON` | Use the `deflate_quick` and `deflate_medium` strategy functions. When `OFF`, only `deflate_fast`, `deflate_slow`, and special strategies are used. |
| `WITH_REDUCED_MEM` | BOOL | `OFF` | Reduce memory usage at the cost of performance. For memory-constrained environments. |
| `WITH_ALL_FALLBACKS` | BOOL | `OFF` | Build all generic fallback functions even when SIMD is available. Useful for benchmarking or running `gbench` comparisons. |
| `WITH_CRC32_CHORBA` | BOOL | `ON` | Enable the optimised Chorba CRC-32 algorithm. |

### Native Instructions

| Option | Type | Default | Description |
|---|---|---|---|
| `WITH_NATIVE_INSTRUCTIONS` | BOOL | `OFF` | Compile with `-march=native` (GCC/Clang) or equivalent. Produces a binary optimised for the build machine. Disables runtime CPU detection (`WITH_RUNTIME_CPU_DETECTION` is forced `OFF`). |
| `WITH_RUNTIME_CPU_DETECTION` | BOOL | `ON` | Build with runtime CPU feature detection. When `OFF`, function dispatch is resolved at compile time and only the features guaranteed by the target architecture are used. |
| `NATIVE_ARCH_OVERRIDE` | STRING | (empty) | Override the native instruction flag. For example, `-march=haswell`. |

### x86 SIMD Options

These options are available when building on x86/x86-64:

| Option | Type | Default | Depends On | Description |
|---|---|---|---|---|
| `WITH_SSE2` | BOOL | `ON` | — | Build with SSE2 intrinsics. Always available on x86-64. |
| `WITH_SSSE3` | BOOL | `ON` | `WITH_SSE2` | Build with SSSE3 intrinsics. |
| `WITH_SSE41` | BOOL | `ON` | `WITH_SSSE3` | Build with SSE4.1 intrinsics. |
| `WITH_SSE42` | BOOL | `ON` | `WITH_SSE41` | Build with SSE4.2 intrinsics. |
| `WITH_PCLMULQDQ` | BOOL | `ON` | `WITH_SSE42` | Build with PCLMULQDQ carryless multiply for CRC-32. |
| `WITH_AVX2` | BOOL | `ON` | `WITH_SSE42` | Build with AVX2 intrinsics (also requires BMI2 at runtime). |
| `WITH_AVX512` | BOOL | `ON` | `WITH_AVX2` | Build with AVX-512 (F, DQ, BW, VL) intrinsics. |
| `WITH_AVX512VNNI` | BOOL | `ON` | `WITH_AVX512` | Build with AVX-512 VNNI for Adler-32 using `VPDPBUSD`. |
| `WITH_VPCLMULQDQ` | BOOL | `ON` | `WITH_PCLMULQDQ`, `WITH_AVX2` | Build with VPCLMULQDQ vectorised CRC-32. |

The dependency chain is enforced by CMake `cmake_dependent_option`. Disabling
an earlier option automatically disables all options that depend on it.

### ARM SIMD Options

| Option | Type | Default | Description |
|---|---|---|---|
| `WITH_ARMV6` | BOOL | `ON` (32-bit only) | Build with ARMv6 SIMD for `slide_hash`. |
| `WITH_ARMV8` | BOOL | `ON` | Build with ARMv8 intrinsics (CRC32 instructions). |
| `WITH_NEON` | BOOL | `ON` | Build with NEON intrinsics. |

### PowerPC Options

| Option | Type | Default | Description |
|---|---|---|---|
| `WITH_ALTIVEC` | BOOL | `ON` | Build with AltiVec (VMX) optimisations. |
| `WITH_POWER8` | BOOL | `ON` | Build with POWER8 (VSX) optimisations. |
| `WITH_POWER9` | BOOL | `ON` | Build with POWER9 optimisations. |

### RISC-V Options

| Option | Type | Default | Description |
|---|---|---|---|
| `WITH_RVV` | BOOL | `ON` | Build with RISC-V Vector extension intrinsics. |
| `WITH_RISCV_ZBC` | BOOL | `ON` | Build with RISC-V Bit-manipulation CRC-32. |

### IBM z/Architecture Options

| Option | Type | Default | Description |
|---|---|---|---|
| `WITH_DFLTCC_DEFLATE` | BOOL | `OFF` | Build with DFLTCC hardware compression on IBM Z. |
| `WITH_DFLTCC_INFLATE` | BOOL | `OFF` | Build with DFLTCC hardware decompression on IBM Z. |
| `WITH_CRC32_VX` | BOOL | `ON` | Build with vectorised CRC-32 on IBM Z. |

### LoongArch Options

| Option | Type | Default | Description |
|---|---|---|---|
| `WITH_CRC32_LA` | BOOL | `ON` | Build with vectorised CRC-32 on LoongArch. |
| `WITH_LSX` | BOOL | `ON` | Build with LoongArch LSX SIMD. |
| `WITH_LASX` | BOOL | `ON` (depends on `WITH_LSX`) | Build with LoongArch LASX (256-bit) SIMD. |

### Inflate Options

| Option | Type | Default | Description |
|---|---|---|---|
| `WITH_INFLATE_STRICT` | BOOL | `OFF` | Enable strict distance checking in inflate. Rejects distances greater than the output produced so far. |
| `WITH_INFLATE_ALLOW_INVALID_DIST` | BOOL | `OFF` | Zero-fill invalid inflate distances instead of returning an error. |

### Testing and Development Options

| Option | Type | Default | Description |
|---|---|---|---|
| `BUILD_TESTING` | BOOL | `ON` | Build the test suite. |
| `WITH_GTEST` | BOOL | `ON` (if `BUILD_TESTING`) | Build Google Test-based tests. |
| `WITH_FUZZERS` | BOOL | `OFF` (if `BUILD_TESTING`) | Build fuzz targets under `test/fuzz/`. |
| `WITH_BENCHMARKS` | BOOL | `OFF` (if `BUILD_TESTING`) | Build Google Benchmark harnesses under `test/benchmarks/`. |
| `WITH_BENCHMARK_APPS` | BOOL | `OFF` (if `BUILD_TESTING`) | Build application-level benchmarks. |
| `WITH_SANITIZER` | STRING | `OFF` | Enable a sanitizer. Values: `OFF`, `memory`, `address`, `undefined`, `thread`. |
| `WITH_CODE_COVERAGE` | BOOL | `OFF` | Enable code coverage reporting (lcov/gcov). |
| `WITH_MAINTAINER_WARNINGS` | BOOL | `OFF` | Enable extra compiler warnings for project maintainers. |
| `INSTALL_UTILS` | BOOL | `OFF` | Install `minigzip` and `minideflate` utilities. |

### Symbol Prefix

| Option | Type | Default | Description |
|---|---|---|---|
| `ZLIB_SYMBOL_PREFIX` | STRING | (empty) | Add a custom prefix to all public symbols. Useful for embedding neozip into a larger library to avoid symbol conflicts. |

---

## C Standard Selection

The default C standard is **C11**. You can override it:

```bash
cmake -B build -DCMAKE_C_STANDARD=99   # Use C99
cmake -B build -DCMAKE_C_STANDARD=17   # Use C17 (requires CMake ≥ 3.21)
cmake -B build -DCMAKE_C_STANDARD=23   # Use C23 (requires CMake ≥ 3.21)
```

Valid standards: `99`, `11`, `17`, `23`. The build will fail with a clear
error message if an unsupported value is specified.

---

## Build Types

When not using a multi-config generator (e.g., Ninja, Makefiles), the
default build type is **Release**:

| Build Type | Compiler Flags | Use Case |
|---|---|---|
| `Release` | `-O3` (GCC/Clang) | Production builds, benchmarks |
| `Debug` | `-O0 -g` | Development, debugging |
| `RelWithDebInfo` | `-O2 -g` | Profiling with debug symbols |
| `MinSizeRel` | `-Os` | Size-constrained environments |

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Debug
```

---

## Cross-Compilation

### Toolchain File

```bash
cmake -B build -DCMAKE_TOOLCHAIN_FILE=/path/to/toolchain.cmake
```

The build system will log the toolchain being used:
```
-- Using CMake toolchain: /path/to/toolchain.cmake
```

### Example: Cross-compiling for ARM64

```cmake
# aarch64-toolchain.cmake
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR aarch64)
set(CMAKE_C_COMPILER aarch64-linux-gnu-gcc)
set(CMAKE_CXX_COMPILER aarch64-linux-gnu-g++)
```

```bash
cmake -B build-arm64 \
  -DCMAKE_TOOLCHAIN_FILE=aarch64-toolchain.cmake \
  -DCMAKE_BUILD_TYPE=Release
cmake --build build-arm64 -j$(nproc)
```

### Example: MinGW cross-compilation for Windows

A pre-made toolchain file is provided:

```bash
cmake -B build-mingw \
  -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain-mingw32.cmake \
  -DCMAKE_BUILD_TYPE=Release
```

---

## zlib-Compat vs. Native Mode

### API Differences

| Aspect | `ZLIB_COMPAT=ON` | `ZLIB_COMPAT=OFF` (default) |
|---|---|---|
| Library name | `libz.so` / `libz.a` | `libneozip.so` / `libneozip.a` |
| Header | `zlib.h` | `neozip.h` |
| Symbol prefix | `z_` (via mangling) | `zng_` |
| Stream type | `z_stream` (with `unsigned long`) | `zng_stream` (with `uint32_t`) |
| CMake target | `ZLIB::ZLIB` | `neozip::neozip` |
| Config file | `zlib-config.cmake` | `neozip-config.cmake` |
| pkg-config | `zlib.pc` | (generated) |
| gzip file ops | Always included | Controlled by `WITH_GZFILEOP` |

### Name Mangling

In zlib-compat mode, `zlib_name_mangling.h` maps neozip's `zng_` names to
standard `z_` names:

```c
#define zng_deflateInit    deflateInit
#define zng_inflate        inflate
#define zng_crc32          crc32
// ... etc.
```

This is generated from `zlib_name_mangling.h.in` by CMake.

---

## Runtime CPU Detection vs. Native Build

### Runtime Detection (default)

With `WITH_RUNTIME_CPU_DETECTION=ON`:
- The `functable.c` dispatch table is compiled
- `cpu_features.c` queries CPUID/feature registers at first use
- The binary works on any CPU of the target architecture
- Slight overhead from indirect function calls through `functable`

### Native Build

With `WITH_NATIVE_INSTRUCTIONS=ON`:
- `-march=native` (or equivalent) is passed to the compiler
- `WITH_RUNTIME_CPU_DETECTION` is forced `OFF`
- All dispatch is resolved at compile time via `native_` macros
- The binary only works on CPUs with the same (or newer) features as the build machine
- No dispatch overhead — direct function calls
- May enable additional compiler auto-vectorisation

```bash
cmake -B build -DWITH_NATIVE_INSTRUCTIONS=ON -DCMAKE_BUILD_TYPE=Release
```

You can verify the selected native flag in the CMake output:
```
-- Performing Test HAVE_MARCH_NATIVE
-- Performing Test HAVE_MARCH_NATIVE - Success
```

### When to Use Each

| Scenario | Recommendation |
|---|---|
| Distribution package | Runtime detection (default) |
| Application bundled with specific hardware | Native instructions |
| Development and testing | Runtime detection |
| Maximum performance on known hardware | Native instructions |
| Benchmark comparisons | Both; compare results |

---

## Sanitizer Builds

```bash
# Address Sanitizer (memory errors)
cmake -B build-asan -DWITH_SANITIZER=address -DCMAKE_BUILD_TYPE=Debug
cmake --build build-asan

# Memory Sanitizer (uninitialised reads)
cmake -B build-msan -DWITH_SANITIZER=memory -DCMAKE_BUILD_TYPE=Debug
cmake --build build-msan

# Undefined Behaviour Sanitizer
cmake -B build-ubsan -DWITH_SANITIZER=undefined -DCMAKE_BUILD_TYPE=Debug
cmake --build build-ubsan

# Thread Sanitizer (data races)
cmake -B build-tsan -DWITH_SANITIZER=thread -DCMAKE_BUILD_TYPE=Debug
cmake --build build-tsan
```

---

## Code Coverage

```bash
cmake -B build-cov -DWITH_CODE_COVERAGE=ON -DCMAKE_BUILD_TYPE=Debug
cmake --build build-cov
cd build-cov
ctest
# Generate coverage report using lcov/gcov
```

---

## Building with ccache

```bash
cmake -B build -DCMAKE_C_COMPILER_LAUNCHER=ccache
cmake --build build -j$(nproc)
```

---

## Building as a Subdirectory

Neozip supports being included as a CMake subdirectory:

```cmake
add_subdirectory(neozip)
target_link_libraries(myapp PRIVATE zlibstatic)
```

The `test/add-subdirectory-project/` directory contains an example.

---

## Output Artifacts

After a successful build, the following artifacts are produced:

### Libraries

| Target | Static | Shared |
|---|---|---|
| Native mode | `libneozip.a` | `libneozip.so.2.3.90` (Linux) |
| Compat mode | `libz.a` | `libz.so.1.3.1.neozip` (Linux) |

### Utilities (optional)

| Utility | Description |
|---|---|
| `minigzip` | Minimal gzip compressor/decompressor |
| `minideflate` | Minimal raw deflate tool |

### Test Binaries (optional)

| Binary | Description |
|---|---|
| `gtest_zlib` | Google Test test runner |
| `example` | Classic zlib example |
| `switchlevels` | Level-switching test |
| `infcover` | Inflate code coverage test |

---

## Compiler-Specific Notes

### GCC

- Full support from GCC 5 onwards
- `-march=native` works reliably
- `-fno-lto` is automatically applied when `WITH_NATIVE_INSTRUCTIONS=OFF` to
  prevent LTO from hoisting SIMD code into non-SIMD translation units

### Clang

- Full support from Clang 5 onwards
- Supports all the same flags as GCC

### MSVC

- Minimum version: Visual Studio 2013 (MSVC 1800)
- SSE2 macros (`__SSE__`, `__SSE2__`) are explicitly defined since MSVC does
  not set them by default
- Chorba SSE2/SSE4.1 variants require MSVC 2022 (version 1930+)

### Intel ICC

- Supports `-diag-disable=10441` for deprecation warning suppression
- Classic Intel compiler flags are handled

### NVIDIA HPC

- Uses `-tp px` or `-tp native` for target selection
- Supports the standard compilation flow

---

## Feature Summary

At the end of configuration, CMake prints a feature summary showing which
options are enabled. Example output:

```
-- Feature summary for zlib 1.3.1
-- The following features have been enabled:
-- * CMAKE_BUILD_TYPE, Build type: Release (default)
-- * WITH_GZFILEOP
-- * WITH_OPTIM
-- * WITH_NEW_STRATEGIES
-- * WITH_CRC32_CHORBA
-- * WITH_RUNTIME_CPU_DETECTION
-- * WITH_SSE2
-- * WITH_SSSE3
-- * WITH_SSE41
-- * WITH_SSE42
-- * WITH_PCLMULQDQ
-- * WITH_AVX2
-- * WITH_AVX512
-- * WITH_AVX512VNNI
-- * WITH_VPCLMULQDQ
```

---

## Verifying the Build

```bash
# Run the test suite
cd build
ctest --output-on-failure -j$(nproc)

# Check the library version
./minigzip -h 2>&1 | head -1

# Verify architecture detection
cmake -B build -DCMAKE_BUILD_TYPE=Release 2>&1 | grep -i "arch\|SIMD\|SSE\|AVX\|NEON"
```

---

## Integration in Other Projects

### Using `find_package`

```cmake
# Native mode
find_package(neozip 2.3 CONFIG REQUIRED)
target_link_libraries(myapp PRIVATE neozip::neozip)

# Compat mode
find_package(ZLIB 1.3 CONFIG REQUIRED)
target_link_libraries(myapp PRIVATE ZLIB::ZLIB)
```

### Using pkg-config

```bash
pkg-config --cflags --libs zlib    # compat mode
```

### Using FetchContent

```cmake
include(FetchContent)
FetchContent_Declare(neozip
    SOURCE_DIR ${CMAKE_SOURCE_DIR}/neozip)
FetchContent_MakeAvailable(neozip)
target_link_libraries(myapp PRIVATE zlibstatic)
```
