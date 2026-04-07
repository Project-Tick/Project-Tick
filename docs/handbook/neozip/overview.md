# Neozip Overview

## What Is Neozip?

Neozip is Project Tick's fork of **neozip**, which is itself a modernized,
performance-oriented fork of the venerable zlib compression library. Neozip
provides a drop-in replacement for zlib with significantly improved throughput
on modern hardware while retaining full API and format compatibility with the
original zlib 1.3.1 specification.

The library implements the **DEFLATE** compressed data format (RFC 1951),
wrapped in either the **zlib** container (RFC 1950) or the **gzip** container
(RFC 1952). It also exposes raw deflate streams without any wrapper.

Neozip tracks upstream neozip closely. At the time of writing, the embedded
version strings are:

```c
#define NEOZIP_VERSION "2.3.90"
#define ZLIB_VERSION   "1.3.1.neozip"
```

---

## Why Neozip Exists

The original zlib library was written in the early 1990s when CPUs had very
different performance characteristics. While zlib is extremely portable and
well-tested, it leaves significant performance on the table on modern
processors because:

1. **No SIMD utilisation** — zlib's inner loops (match finding, checksumming,
   sliding the hash window) are scalar C targeting 32-bit architectures.
2. **Conservative data structures** — hash chain lengths, buffer sizes, and
   alignment are tuned for machines with tiny caches.
3. **No runtime CPU feature detection** — the same compiled binary cannot
   select between SSE2 and AVX-512 code paths at runtime.

Neozip (via neozip) addresses every one of these issues while maintaining
byte-for-byte compatible output with zlib for any given set of compression
parameters (when the `ZLIB_COMPAT` build option is enabled).

---

## Feature List

### Core Compression and Decompression

| Feature | Description |
|---|---|
| DEFLATE compression (RFC 1951) | Full implementation of LZ77 + Huffman coding |
| DEFLATE decompression | State-machine inflater with optimised fast paths |
| zlib wrapper (RFC 1950) | Adler-32 integrity, two-byte header |
| gzip wrapper (RFC 1952) | CRC-32 integrity, file metadata header |
| Raw deflate | No wrapper, caller handles integrity |
| Compression levels 0–9 | From stored (level 0) through maximum compression (level 9) |
| Multiple strategies | `Z_DEFAULT_STRATEGY`, `Z_FILTERED`, `Z_HUFFMAN_ONLY`, `Z_RLE`, `Z_FIXED` |
| Streaming API | Process data in arbitrarily-sized chunks via `deflate()` / `inflate()` |
| One-shot API | `compress()` / `uncompress()` for simple in-memory use |
| gzip file I/O | `gzopen()`, `gzread()`, `gzwrite()`, `gzprintf()`, etc. |
| Dictionary support | Pre-seed compression / decompression with a shared dictionary |

### Performance Optimisations

| Optimisation | Details |
|---|---|
| Runtime CPU detection | `cpu_features.c` queries CPUID (x86), `/proc/cpuinfo` (ARM), etc. |
| Function dispatch table | `functable.c` selects the best implementation for each hot function |
| x86 SSE2 | `slide_hash`, `compare256`, `chunkset`, `inflate_fast`, CRC-32 Chorba |
| x86 SSSE3 | `adler32`, `chunkset`, `inflate_fast` |
| x86 SSE4.1 | CRC-32 Chorba SSE4.1 variant |
| x86 SSE4.2 | `adler32_copy` |
| x86 PCLMULQDQ | Carryless-multiply CRC-32 |
| x86 AVX2 | `adler32`, `compare256`, `chunkset`, `slide_hash`, `inflate_fast`, `longest_match` |
| x86 AVX-512 | `adler32`, `compare256`, `chunkset`, `inflate_fast`, `longest_match` |
| x86 AVX-512 VNNI | `adler32` using VPDPBUSD |
| x86 VPCLMULQDQ | Vectorised CRC-32 with AVX2 and AVX-512 widths |
| ARM NEON | `adler32`, `compare256`, `chunkset`, `slide_hash`, `inflate_fast`, `longest_match` |
| ARM CRC32 extension | Hardware CRC-32 instructions |
| ARM PMULL+EOR3 | Polynomial multiply CRC-32 with SHA3 three-way XOR |
| ARMv6 SIMD | `slide_hash` for 32-bit ARM |
| PowerPC VMX/VSX | `adler32`, `slide_hash`, `chunkset`, `inflate_fast` |
| POWER8/9 | Optimised Adler-32, CRC-32, compare256 |
| RISC-V RVV | Vector extensions for core loops |
| RISC-V Zbc | Bit-manipulation CRC-32 |
| IBM z/Architecture DFLTCC | Hardware deflate/inflate in a single instruction |
| LoongArch LSX/LASX | SIMD for CRC-32 and general loops |

### Algorithmic Improvements

| Improvement | Details |
|---|---|
| Quick deflate (level 1) | Intel-designed single-pass strategy (`deflate_quick.c`) |
| Medium deflate (levels 3-6) | Intel-designed strategy bridging fast and slow (`deflate_medium.c`) |
| Chorba CRC-32 | Modern CRC algorithm by Kadatch & Jenkins with braided and SIMD variants |
| 64-bit bit buffer | `bi_buf` is `uint64_t` instead of `unsigned long`, reducing flush frequency |
| Unified memory allocation | Single `zalloc` call for all deflate/inflate buffers, cache-line aligned |
| LIT_MEM mode | Separate distance/length buffers for platforms without fast unaligned access |
| Rolling hash for level 9 | `insert_string_roll` for better match quality at maximum compression |

### Build System

| Feature | Details |
|---|---|
| CMake (≥ 3.14) | Primary build system with extensive option detection |
| C11 standard | Default; C99, C17, C23 also supported |
| zlib-compat mode | `ZLIB_COMPAT=ON` produces a drop-in `libz` replacement |
| Native mode | `ZLIB_COMPAT=OFF` produces `libneozip` with `zng_` prefixed API |
| Static and shared libraries | Both targets generated |
| Google Test suite | Comprehensive C++ test suite under `test/` |
| Fuzz targets | Under `test/fuzz/` for OSS-Fuzz integration |
| Benchmark suite | Google Benchmark harnesses under `test/benchmarks/` |
| Sanitizer support | ASan, MSan, TSan, UBSan integration via `WITH_SANITIZER` |
| Code coverage | `WITH_CODE_COVERAGE` for lcov/gcov |

---

## Repository Structure

The neozip source tree is organised as follows:

```
neozip/
├── CMakeLists.txt          # Top-level build configuration
├── deflate.c / deflate.h   # Core compression engine
├── deflate_fast.c          # Level 1-2 (or 2-3) fast strategy
├── deflate_medium.c        # Level 3-6 medium strategy (Intel)
├── deflate_slow.c          # Level 7-9 lazy/slow strategy
├── deflate_quick.c         # Level 1 quick strategy (Intel)
├── deflate_stored.c        # Level 0 stored (no compression)
├── deflate_huff.c          # Huffman-only strategy
├── deflate_rle.c           # Run-length encoding strategy
├── deflate_p.h             # Private deflate inline helpers
├── inflate.c / inflate.h   # Decompression state machine
├── inflate_p.h             # Private inflate inline helpers
├── infback.c               # Inflate with caller-provided window
├── inftrees.c / inftrees.h # Huffman code table builder for inflate
├── inffast_tpl.h           # Fast inflate inner loop template
├── inffixed_tbl.h          # Fixed Huffman tables for inflate
├── trees.c / trees.h       # Huffman tree construction for deflate
├── trees_emit.h            # Bit emission macros for tree output
├── trees_tbl.h             # Static Huffman tree tables
├── adler32.c               # Adler-32 checksum entry points
├── adler32_p.h             # Scalar Adler-32 implementation
├── crc32.c                 # CRC-32 checksum entry points
├── crc32_braid_p.h         # Braided CRC-32 configuration
├── crc32_braid_comb.c      # CRC-32 combine logic
├── crc32_chorba_p.h        # Chorba CRC-32 algorithm
├── compress.c              # One-shot compress()
├── uncompr.c               # One-shot uncompress()
├── gzlib.c                 # gzip file I/O common code
├── gzread.c                # gzip file reading
├── gzwrite.c               # gzip file writing
├── gzguts.h                # gzip internal definitions
├── zlib.h.in               # Public API header (zlib-compat mode)
├── neozip.h.in            # Public API header (native mode)
├── zbuild.h                # Build-system defines, compiler abstraction
├── zutil.h / zutil.c       # Internal utility functions
├── zutil_p.h               # Private utility helpers
├── zendian.h               # Endianness detection and byte-swap macros
├── zmemory.h               # Aligned memory read/write helpers
├── zarch.h                 # Architecture detection macros
├── cpu_features.c / .h     # Runtime CPU feature detection dispatch
├── functable.c / .h        # Runtime function pointer dispatch table
├── arch_functions.h        # Architecture-specific function declarations
├── arch_natives.h          # Native (compile-time) arch selection
├── insert_string.c         # Hash table insert implementations
├── insert_string_p.h       # Private insert_string helpers
├── insert_string_tpl.h     # Insert string template macros
├── match_tpl.h             # Longest-match template (compare256 based)
├── chunkset_tpl.h          # Chunk memory-set template
├── compare256_rle.h        # RLE-optimised compare256
├── arch/                   # Architecture-specific SIMD implementations
│   ├── generic/            # Portable C fallbacks
│   ├── x86/                # SSE2, SSSE3, SSE4, AVX2, AVX-512, PCLMULQDQ
│   ├── arm/                # NEON, CRC32 extension, PMULL
│   ├── power/              # VMX, VSX, POWER8, POWER9
│   ├── s390/               # IBM z DFLTCC
│   ├── riscv/              # RVV, Zbc
│   └── loongarch/          # LSX, LASX
├── test/                   # GTest test suite, fuzz targets, benchmarks
├── cmake/                  # CMake modules (intrinsic detection, etc.)
├── doc/                    # Upstream documentation
├── tools/                  # Utility scripts
└── win32/                  # Windows-specific files
```

---

## Data Formats

Neozip processes three container formats, all built on top of the same DEFLATE
compressed data representation:

### Raw Deflate (RFC 1951)

A sequence of DEFLATE blocks with no framing. The caller is responsible for
any integrity checking. Selected by passing a negative `windowBits` value
(e.g., `-15`) to `deflateInit2()` / `inflateInit2()`.

### zlib Format (RFC 1950)

```
+---+---+   +---+---+---+---+
| CMF|FLG|   |     DATA      |  ...  +---+---+---+---+
+---+---+   +---+---+---+---+        |   ADLER-32    |
                                      +---+---+---+---+
```

- **CMF** (Compression Method and Flags): method = 8 (deflate), window size
- **FLG**: check bits, optional preset dictionary flag (`FDICT`)
- **DATA**: raw deflate blocks
- **ADLER-32**: checksum of uncompressed data (big-endian)

Overhead: 6 bytes (`ZLIB_WRAPLEN`).

### gzip Format (RFC 1952)

```
+---+---+---+---+---+---+---+---+---+---+  +-------+  +---+---+---+---+---+---+---+---+
|ID1|ID2| CM|FLG|     MTIME     |XFL| OS |  | DATA  |  |     CRC-32    |    ISIZE      |
+---+---+---+---+---+---+---+---+---+---+  +-------+  +---+---+---+---+---+---+---+---+
```

- **ID1, ID2**: Magic bytes `0x1f`, `0x8b`
- **CM**: Compression method (8 = deflate)
- **FLG**: Flags for text, CRC, extra, name, comment
- **MTIME**: Modification time (Unix epoch)
- **XFL**: Extra flags (2 = best compression, 4 = fastest)
- **OS**: Operating system code
- **DATA**: Raw deflate blocks
- **CRC-32**: CRC of uncompressed data
- **ISIZE**: Uncompressed size mod 2^32

Overhead: 18 bytes (`GZIP_WRAPLEN`).

---

## Compilation Modes

### zlib-Compatible Mode (`ZLIB_COMPAT=ON`)

When built with `-DZLIB_COMPAT=ON`:

- The library is named `libz` (no suffix).
- All public symbols use standard zlib names: `deflateInit`, `inflate`, `crc32`, etc.
- The `z_stream` structure uses `unsigned long` for `total_in` / `total_out`.
- Header file is `zlib.h`.
- Symbol prefix macro `PREFIX()` expands to `z_` (via mangling headers).
- The `ZLIB_COMPAT` preprocessor macro is defined.
- gzip file operations (`WITH_GZFILEOP`) are forced on.

This is the mode to use when neozip must be a transparent drop-in replacement
for system zlib.

### Native Mode (`ZLIB_COMPAT=OFF`)

When built with `-DZLIB_COMPAT=OFF` (the default):

- The library is named `libneozip`.
- All public symbols use `zng_` prefixed names: `zng_deflateInit`, `zng_inflate`, etc.
- The `zng_stream` structure uses fixed-width types (`uint32_t`).
- Header file is `neozip.h`.
- Symbol prefix macro `PREFIX()` expands to `zng_`.
- No `ZLIB_COMPAT` macro is defined.

Native mode is recommended for new code. Types are cleaner and there is no
ambiguity about which zlib implementation is in use.

---

## Compression Levels and Strategies

### Compression Levels

Neozip maps each compression level (0–9) to a specific **strategy function**
and a set of tuning parameters defined in the `configuration_table` in
`deflate.c`:

```c
static const config configuration_table[10] = {
/*      good lazy nice chain */
/* 0 */ {0,    0,  0,    0, deflate_stored},  /* store only */
/* 1 */ {0,    0,  0,    0, deflate_quick},   /* quick strategy */
/* 2 */ {4,    4,  8,    4, deflate_fast},
/* 3 */ {4,    6, 16,    6, deflate_medium},
/* 4 */ {4,   12, 32,   24, deflate_medium},
/* 5 */ {8,   16, 32,   32, deflate_medium},
/* 6 */ {8,   16, 128, 128, deflate_medium},
/* 7 */ {8,   32, 128, 256, deflate_slow},
/* 8 */ {32, 128, 258, 1024, deflate_slow},
/* 9 */ {32, 258, 258, 4096, deflate_slow},
};
```

The `config` fields are:
- **good_length** — reduce lazy search above this match length
- **max_lazy** — do not perform lazy search above this match length
- **nice_length** — quit search above this match length
- **max_chain** — maximum hash chain length to traverse
- **func** — pointer to the strategy function

### Strategy Functions

| Strategy | Levels | Source File | Description |
|---|---|---|---|
| `deflate_stored` | 0 | `deflate_stored.c` | No compression; copies input as stored blocks |
| `deflate_quick` | 1 | `deflate_quick.c` | Fastest compression; static Huffman, minimal match search |
| `deflate_fast` | 2 (or 1–3 without quick) | `deflate_fast.c` | Greedy matching, no lazy evaluation |
| `deflate_medium` | 3–6 | `deflate_medium.c` | Balanced: limited lazy evaluation, match merging |
| `deflate_slow` | 7–9 | `deflate_slow.c` | Full lazy evaluation, deepest hash chain search |
| `deflate_huff` | (Z_HUFFMAN_ONLY) | `deflate_huff.c` | Huffman-only, no LZ77 matching |
| `deflate_rle` | (Z_RLE) | `deflate_rle.c` | Run-length encoding, distance always 1 |

### Explicit Strategies

The `strategy` parameter to `deflateInit2()` can override the default level-based
selection:

- **`Z_DEFAULT_STRATEGY` (0)** — Normal deflate; level selects the function.
- **`Z_FILTERED` (1)** — Optimised for data produced by a filter (e.g., PNG predictors).
  Uses `deflate_slow` with short match rejection.
- **`Z_HUFFMAN_ONLY` (2)** — No LZ77; every byte is a literal.
- **`Z_RLE` (3)** — Only find runs of identical bytes (distance = 1).
- **`Z_FIXED` (4)** — Use fixed Huffman codes instead of dynamic trees.

---

## Memory Layout

Neozip uses a **single-allocation** strategy for both deflate and inflate
states. The function `alloc_deflate()` in `deflate.c` computes the total
buffer size required and calls `zalloc` exactly once, then partitions the
returned memory into:

1. **Window buffer** — Aligned to `WINDOW_PAD_SIZE` (64 or 4096 bytes depending
   on architecture). Size: `2 * (1 << windowBits)`.
2. **prev array** — `Pos` (uint16_t) array of size `1 << windowBits`. Aligned to 64 bytes.
3. **head array** — `Pos` array of size `HASH_SIZE` (65536). Aligned to 64 bytes.
4. **pending_buf** — Output bit buffer of size `lit_bufsize * LIT_BUFS + 1`. Aligned to 64 bytes.
5. **deflate_state** — The `internal_state` struct itself. Aligned to 64 bytes
   (cache-line aligned via `ALIGNED_(64)`).
6. **deflate_allocs** — Book-keeping struct tracking the original allocation pointer.

The `inflate_state` uses an analogous scheme via `alloc_inflate()`:

1. **Window buffer** — `(1 << MAX_WBITS) + 64` bytes with `WINDOW_PAD_SIZE` alignment.
2. **inflate_state** — The state struct, 64-byte aligned.
3. **inflate_allocs** — Book-keeping.

This approach minimises the number of `malloc` calls, improves cache locality,
and simplifies cleanup (a single `zfree` releases everything).

---

## Thread Safety

Neozip is thread-safe under the following conditions:

1. Each `z_stream` instance is accessed from only one thread at a time.
2. The `zalloc` and `zfree` callbacks are thread-safe (the defaults use
   `malloc` / `free`, which are thread-safe on all supported platforms).

The function dispatch table (`functable`) uses atomic stores during
initialisation:

```c
#define FUNCTABLE_ASSIGN(VAR, FUNC_NAME) \
    __atomic_store(&(functable.FUNC_NAME), &(VAR.FUNC_NAME), __ATOMIC_SEQ_CST)
#define FUNCTABLE_BARRIER() __atomic_thread_fence(__ATOMIC_SEQ_CST)
```

This ensures that even if multiple threads call `deflateInit` / `inflateInit`
concurrently, the function table is initialised safely.

---

## Version Identification

The library provides several ways to query version information:

```c
const char *zlibVersion(void);         // Returns "1.3.1.neozip" in compat mode
const char *neozip_version(void);      // Returns "2.3.90"

// Compile-time constants
#define NEOZIP_VERSION  "2.3.90"
#define NEOZIP_VERNUM   0x02039000L
#define ZLIB_VERSION    "1.3.1.neozip"
#define ZLIB_VERNUM     0x131f
```

In compat mode, `deflateInit` and `inflateInit` verify that the header version
matches the library version to prevent ABI mismatches:

```c
#define CHECK_VER_STSIZE(version, stream_size) \
    (version == NULL || version[0] != ZLIB_VERSION[0] || \
     stream_size != (int32_t)sizeof(PREFIX3(stream)))
```

---

## Licensing

Neozip inherits the zlib/libpng license from both zlib and neozip:

> This software is provided 'as-is', without any express or implied warranty.
> Permission is granted to anyone to use this software for any purpose,
> including commercial applications, and to alter it and redistribute it freely,
> subject to the following restrictions: [...]

See `LICENSE.md` in the neozip source tree for the full text.

---

## Key Differences from Upstream zlib

| Area | zlib 1.3.1 | Neozip (neozip) |
|---|---|---|
| Bit buffer width | 32-bit `unsigned long` | 64-bit `uint64_t` |
| Hash table size | 32768 entries (15 bits) | 65536 entries (16 bits) |
| Match buffer format | Overlaid `sym_buf` only | `LIT_MEM` option for separate `d_buf`/`l_buf` |
| Hash function | Three-byte rolling | Four-byte CRC-based or multiplicative |
| SIMD acceleration | None | Extensive (see Performance Optimisations) |
| CPU detection | None (compile-time only) | Runtime `cpuid` / feature detection |
| Memory allocation | Multiple `zalloc` calls | Single allocation, cache-aligned |
| Minimum match length | 3 (`STD_MIN_MATCH`) | Internally uses `WANT_MIN_MATCH = 4` for speed |
| Quick strategy | None | `deflate_quick` for level 1 |
| Medium strategy | None | `deflate_medium` for levels 3–6 |
| Data structure alignment | None | `ALIGNED_(64)` on key structs |
| Build system | Makefile / CMake | CMake primary with full feature detection |

---

## Quick Start

### Building

```bash
cd neozip
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . -j$(nproc)
```

### Using in a CMake Project

```cmake
find_package(neozip CONFIG REQUIRED)
target_link_libraries(myapp PRIVATE neozip::neozip)
```

Or in zlib-compat mode:

```cmake
find_package(ZLIB CONFIG REQUIRED)
target_link_libraries(myapp PRIVATE ZLIB::ZLIB)
```

### Minimal Compression Example

```c
#include <neozip.h>
#include <string.h>
#include <stdio.h>

int main(void) {
    const char *source = "Hello, Neozip! This is a test of compression.";
    size_t source_len = strlen(source);

    size_t dest_len = zng_compressBound(source_len);
    unsigned char *dest = malloc(dest_len);

    int ret = zng_compress(dest, &dest_len, (const unsigned char *)source, source_len);
    if (ret == Z_OK) {
        printf("Compressed %zu bytes to %zu bytes\n", source_len, dest_len);
    }

    unsigned char *recovered = malloc(source_len + 1);
    size_t recovered_len = source_len;
    zng_uncompress(recovered, &recovered_len, dest, dest_len);
    recovered[recovered_len] = '\0';
    printf("Recovered: %s\n", recovered);

    free(dest);
    free(recovered);
    return 0;
}
```

---

## Further Reading

- [Architecture](architecture.md) — Module-by-module breakdown of the source
- [Building](building.md) — Complete CMake option reference
- [Deflate Algorithms](deflate-algorithms.md) — LZ77 match finding and strategies
- [Inflate Engine](inflate-engine.md) — Decompression state machine
- [Huffman Coding](huffman-coding.md) — Tree construction and bit emission
- [Checksum Algorithms](checksum-algorithms.md) — CRC-32 and Adler-32 details
- [Hardware Acceleration](hardware-acceleration.md) — CPU detection and dispatch
- [x86 Optimizations](x86-optimizations.md) — SSE/AVX/PCLMULQDQ implementations
- [ARM Optimizations](arm-optimizations.md) — NEON and CRC32 extension
- [Gzip Support](gzip-support.md) — gzip file I/O layer
- [API Reference](api-reference.md) — Full public API documentation
- [Performance Tuning](performance-tuning.md) — Benchmarking and tuning guide
- [Testing](testing.md) — Test suite reference
- [Code Style](code-style.md) — Coding conventions
