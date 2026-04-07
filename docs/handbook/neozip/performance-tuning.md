# Performance Tuning

## Overview

Neozip offers multiple controls for trading compression ratio against
speed: compression level, strategy, window size, memory level, hardware
acceleration, and buffer sizing. This guide describes how each knob
affects performance and when to use them.

---

## Compression Level

The `level` parameter (0–9) selects the deflate strategy function and its
internal tuning parameters via the `configuration_table`:

```c
static const config configuration_table[10] = {
/*      good_length  lazy  nice  max_chain  func */
/* 0 */ {0,    0,  0,    0, deflate_stored},    // No compression
/* 1 */ {0,    0,  0,    0, deflate_quick},      // Fastest (Intel)
/* 2 */ {4,    4,  8,    4, deflate_fast},        // Fast greedy
/* 3 */ {4,    6, 32,   32, deflate_fast},
/* 4 */ {4,    4, 16,   16, deflate_medium},      // Balanced (Intel)
/* 5 */ {8,   16, 32,   32, deflate_medium},
/* 6 */ {8,   16,128,  128, deflate_medium},      // Default
/* 7 */ {8,   32,128,  256, deflate_slow},        // Slow lazy
/* 8 */ {32, 128,258, 1024, deflate_slow},
/* 9 */ {32, 258,258, 4096, deflate_slow},        // Maximum
};
```

| Parameter | Effect |
|---|---|
| `good_length` | Reduce match search when match ≥ this length |
| `max_lazy` | Don't try lazy match if current match ≥ this |
| `nice_length` | Stop searching once match ≥ this length |
| `max_chain` | Maximum hash chain steps to search |

### Level Selection Guide

| Use Case | Recommended Level | Rationale |
|---|---|---|
| Real-time streaming | 1 | `deflate_quick`: static Huffman, minimal search |
| Network compression | 2–3 | `deflate_fast`: greedy match, short chains |
| General purpose | 6 (default) | `deflate_medium`: good ratio/speed balance |
| Archival storage | 9 | `deflate_slow`: full lazy evaluation, deep chains |
| Pre-compressed data | 0 | `deflate_stored`: passthrough with framing |

### Speed vs. Ratio Tradeoffs

Approximate throughput (x86_64 with AVX2, single core):

| Level | Compression Speed | Ratio (typical) |
|---|---|---|
| 0 | ~5 GB/s | 1.00 (none) |
| 1 | ~800 MB/s | 2.0–2.5:1 |
| 3 | ~400 MB/s | 2.2–2.8:1 |
| 6 | ~150 MB/s | 2.5–3.2:1 |
| 9 | ~30 MB/s | 2.6–3.4:1 |

Decompression speed is largely independent of the compression level
(~1–2 GB/s), since it only depends on the encoded stream, not the search
strategy.

---

## Strategy Selection

### `Z_DEFAULT_STRATEGY` (0)

Standard DEFLATE with adaptive Huffman coding and LZ77 matching.
Best for most data types.

### `Z_FILTERED` (1)

Optimised for data produced by filters (e.g., delta encoding, integer
sequences). Uses shorter hash chains and favours Huffman coding efficiency.

### `Z_HUFFMAN_ONLY` (2)

Disables LZ77 matching entirely. Every byte is encoded as a literal.
Fast but poor compression ratio for most data. Useful when the data has
already been transformed (e.g., BWT output).

```c
// deflate_huff.c: Only emits literals
block_state deflate_huff(deflate_state *s, int flush) {
    for (;;) {
        // No match search — emit one literal per byte
        zng_tr_tally_lit(s, s->window[s->strstart]);
        s->strstart++;
        s->lookahead--;
        if (s->sym_next == s->sym_end) {
            FLUSH_BLOCK(s, 0);
        }
    }
}
```

### `Z_RLE` (3)

Run-length encoding: only matches at distance 1. Very fast for data with
repeated byte patterns:

```c
// deflate_rle.c
block_state deflate_rle(deflate_state *s, int flush) {
    // Only search for matches at distance == 1
    // Uses compare256_rle for fast run detection
    match_len = FUNCTABLE_CALL(compare256)(scan, scan - 1);
}
```

### `Z_FIXED` (4)

Forces use of static (fixed) Huffman tables for every block. Eliminates
the overhead of dynamic tree transmission. Slightly faster for small
blocks where the tree overhead dominates.

### Strategy Selection Guide

| Data Type | Strategy |
|---|---|
| General text/binary | `Z_DEFAULT_STRATEGY` |
| Numeric arrays, deltas | `Z_FILTERED` |
| Pre-transformed data | `Z_HUFFMAN_ONLY` |
| Runs of repeated bytes | `Z_RLE` |
| Very small blocks | `Z_FIXED` |
| Random/encrypted data | Level 0 (skip entirely) |

---

## Window Size (`windowBits`)

Controls the LZ77 sliding window (8–15, default 15):

| windowBits | Window Size | Memory (deflate) |
|---|---|---|
| 9 | 512 B | ~4 KB |
| 10 | 1 KB | ~8 KB |
| 11 | 2 KB | ~16 KB |
| 12 | 4 KB | ~32 KB |
| 13 | 8 KB | ~64 KB |
| 14 | 16 KB | ~128 KB |
| 15 | 32 KB | ~256 KB |

Smaller windows use less memory but find fewer long-distance matches,
reducing compression ratio. For streaming protocols with tight memory
budgets, windowBits=10–12 is a reasonable compromise.

---

## Memory Level (`memLevel`)

Controls the internal hash table and buffer sizes (1–9, default 8):

```c
#define DEF_MEM_LEVEL 8

// In deflateInit2:
s->hash_size = 1 << (memLevel + 7);  // hash_bits = memLevel + 7
s->lit_bufsize = 1 << (memLevel + 6);
```

| memLevel | Hash Table Entries | Literal Buffer | Total Memory |
|---|---|---|---|
| 1 | 256 | 128 | ~1 KB |
| 4 | 2048 | 1024 | ~16 KB |
| 8 (default) | 32768 | 16384 | ~256 KB |
| 9 | 65536 | 32768 | ~512 KB |

Higher memLevel improves hash distribution (fewer collisions) and allows
more symbols to accumulate before flushing, improving Huffman coding
efficiency.

---

## Hardware Acceleration

### Enabling SIMD

**Runtime detection** (default, recommended for distributed binaries):
```bash
cmake .. -DWITH_RUNTIME_CPU_DETECTION=ON
```

**Native compilation** (fastest, for local/dedicated use):
```bash
cmake .. -DWITH_NATIVE_INSTRUCTIONS=ON
```

This passes `-march=native` to the compiler, enabling all instructions
supported by the build machine.

### Selective Feature Control

Disable specific SIMD features:
```bash
cmake .. -DWITH_AVX512=OFF       # Avoid AVX-512 (thermal throttling concern)
cmake .. -DWITH_VPCLMULQDQ=OFF   # Disable VPCLMULQDQ CRC
cmake .. -DWITH_NEON=OFF         # Disable NEON on ARM
```

### SIMD Impact by Operation

| Operation | Scalar | Best SIMD | Speedup |
|---|---|---|---|
| Adler-32 | ~1 B/cycle | ~32 B/cycle (AVX-512+VNNI) | 32× |
| CRC-32 | ~4 B/cycle | ~64 B/cycle (VPCLMULQDQ) | 16× |
| Compare256 | ~1 B/cycle | ~16 B/cycle (AVX2) | 16× |
| Slide Hash | ~1 entry/cycle | ~32 entries/cycle (AVX-512) | 32× |
| Inflate Copy | ~1 B/cycle | ~32 B/cycle (AVX2 chunkmemset) | 32× |

---

## Buffer Sizing

### Compression Buffers

For streaming compression, the output buffer should be at least as large
as `deflateBound(sourceLen)` for the expected input chunk size:

```c
size_t out_size = deflateBound(&strm, chunk_size);
```

Larger buffers reduce system call overhead and improve throughput.

### Gzip Buffer

```c
gzbuffer(gz, size);  // Set before first read/write
```

Default `GZBUFSIZE` is 131072 (128 KB). For sequential I/O, larger
buffers (256 KB–1 MB) improve throughput by amortising I/O overhead.

### Inflate Buffers

The inflate engine benefits from output buffers ≥ 32 KB (the maximum
window size). Buffers ≥ 64 KB keep the fast path active longer (the
fast path requires ≥ 258 bytes of output space and ≥ 6 bytes of input).

---

## `deflateTune()`

Fine-tune the `configuration_table` parameters at runtime without
changing the level:

```c
int deflateTune(z_stream *strm, int good_length, int max_lazy,
                int nice_length, int max_chain);
```

Example — high-speed level 6:
```c
deflateInit(&strm, 6);
deflateTune(&strm, 4, 8, 32, 64);  // Shorter chains than default
```

Example — deeper search at level 4:
```c
deflateInit(&strm, 4);
deflateTune(&strm, 16, 64, 128, 512);  // Deeper search
```

---

## Profiling Tips

### 1. Identify the Bottleneck

Use `perf` or equivalent to identify whether compression is CPU-bound
(expect: hash lookup, match search) or I/O-bound (expect: read/write
syscalls):

```bash
perf record -g ./minigzip < large_file > /dev/null
perf report
```

Look for hot functions:
- `longest_match_*` — String matching (CPU-bound)
- `adler32_*` / `crc32_*` — Checksumming (CPU-bound)
- `slide_hash_*` — Window maintenance (CPU-bound)
- `__write` / `__read` — I/O (I/O-bound)

### 2. Verify SIMD Usage

Check which implementations are selected:

```bash
# Check for SIMD symbols in the binary
nm -D libneozip.so | grep -E 'avx2|neon|sse|pclmul'
```

Or set a breakpoint in `init_functable()` during debugging.

### 3. Benchmark Specific Functions

Use the built-in benchmarks:
```bash
cmake .. -DWITH_BENCHMARKS=ON
cmake --build .
./benchmark_adler32 --benchmark_repetitions=5
./benchmark_compress --benchmark_filter="BM_Compress/6"
```

---

## Common Tuning Scenarios

### High-Throughput Compression (Level 1)

```c
deflateInit2(&strm, 1, Z_DEFLATED, 15 + 16, 8, Z_DEFAULT_STRATEGY);
```

Level 1 uses `deflate_quick`: no hash chain walking, static Huffman tables,
minimal overhead. Best for cases where compression speed matters more than
ratio (real-time logging, network IPC).

### Maximum Compression (Level 9)

```c
deflateInit2(&strm, 9, Z_DEFLATED, 15, 9, Z_DEFAULT_STRATEGY);
```

Level 9 + memLevel 9 provides the deepest search (`max_chain=4096`) and
largest hash table. Use for archival where decompression speed matters
but compression can be slow.

### Memory-Constrained Environment

```c
deflateInit2(&strm, 6, Z_DEFLATED, 10, 4, Z_DEFAULT_STRATEGY);
```

windowBits=10 (1KB window) + memLevel=4 gives ~16KB total memory.
Suitable for embedded systems.

### Multiple Streams in Parallel

Each `z_stream` is independent. For multi-threaded compression, create
one stream per thread:

```c
// Thread-safe: each thread has its own z_stream
#pragma omp parallel for
for (int i = 0; i < num_chunks; i++) {
    z_stream strm = {};
    deflateInit(&strm, 6);
    // compress chunk[i]
    deflateEnd(&strm);
}
```

The `functable` initialisation is thread-safe (atomic init flag), so
the first call from any thread will safely initialise SIMD dispatch.
