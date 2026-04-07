# API Reference

## Overview

Neozip exposes its public API through `zlib.h` (generated from `zlib.h.in`).
In zlib-compat mode (`ZLIB_COMPAT=ON`), function names match standard zlib.
In native mode, all symbols are prefixed with `zng_` and the stream type
becomes `zng_stream`.

The `PREFIX()` macro handles the distinction:
```c
// ZLIB_COMPAT=ON:  PREFIX(deflateInit) → deflateInit
// ZLIB_COMPAT=OFF: PREFIX(deflateInit) → zng_deflateInit
```

---

## Core Data Structures

### `z_stream` / `zng_stream`

```c
typedef struct PREFIX3(stream_s) {
    // Input
    const uint8_t *next_in;    // Next input byte
    uint32_t       avail_in;   // Number of bytes available at next_in
    size_t         total_in;   // Total bytes read so far

    // Output
    uint8_t       *next_out;   // Next output byte position
    uint32_t       avail_out;  // Remaining free space at next_out
    size_t         total_out;  // Total bytes written so far

    // Error
    const char    *msg;        // Last error message (or NULL)

    // Internal
    struct internal_state *state;  // Private state (DO NOT access)

    // Memory management
    alloc_func     zalloc;     // Allocation function (or Z_NULL for default)
    free_func      zfree;      // Free function (or Z_NULL for default)
    void          *opaque;     // Private data for zalloc/zfree

    // Type indicator
    int            data_type;  // Best guess about data type (deflate output hint)

    // Checksum
    unsigned long  adler;      // Adler-32 or CRC-32 of data
    unsigned long  reserved;   // Reserved for future use
} PREFIX3(stream);
```

### `gz_header`

```c
typedef struct PREFIX(gz_header_s) {
    int       text;       // True if compressed data is believed to be text
    unsigned long time;   // Modification time
    int       xflags;     // Extra flags (not used by gzopen)
    int       os;         // Operating system
    uint8_t  *extra;      // Pointer to extra field (or Z_NULL)
    unsigned  extra_len;  // Extra field length
    unsigned  extra_max;  // Space at extra (when reading header)
    char     *name;       // Pointer to file name (or Z_NULL)
    unsigned  name_max;   // Space at name (when reading header)
    char     *comment;    // Pointer to comment (or Z_NULL)
    unsigned  comm_max;   // Space at comment (when reading header)
    int       hcrc;       // True if header CRC present
    int       done;       // True when done reading gzip header
} PREFIX(gz_header);
```

---

## Compression

### Initialisation

```c
int PREFIX(deflateInit)(PREFIX3(stream) *strm, int level);
int PREFIX(deflateInit2)(PREFIX3(stream) *strm, int level, int method,
                          int windowBits, int memLevel, int strategy);
```

**Parameters**:
- `level` — Compression level (0–9, or `Z_DEFAULT_COMPRESSION = -1`):
  | Level | Meaning | Strategy Function |
  |---|---|---|
  | 0 | No compression | `deflate_stored` |
  | 1 | Fastest | `deflate_quick` (Intel) |
  | 2 | Fast | `deflate_fast` |
  | 3 | Fast | `deflate_fast` |
  | 4–5 | Medium | `deflate_medium` (Intel) |
  | 6 | Default | `deflate_medium` (Intel) |
  | 7 | Medium-slow | `deflate_slow` |
  | 8–9 | Maximum compression | `deflate_slow` |

- `method` — Always `Z_DEFLATED` (8)

- `windowBits` — Window size and format:
  | Value | Format |
  |---|---|
  | 8..15 | zlib wrapper |
  | -8..-15 | Raw deflate |
  | 24..31 (8..15 + 16) | gzip wrapper |

- `memLevel` — Memory usage (1–9, default 8):
  Controls hash table and buffer sizes. Higher = more memory, potentially
  better compression.

- `strategy`:
  | Constant | Value | Description |
  |---|---|---|
  | `Z_DEFAULT_STRATEGY` | 0 | Normal compression |
  | `Z_FILTERED` | 1 | Tuned for filtered/delta data |
  | `Z_HUFFMAN_ONLY` | 2 | Huffman only, no string matching |
  | `Z_RLE` | 3 | Run-length encoding only (dist=1) |
  | `Z_FIXED` | 4 | Fixed Huffman tables only |

### Compression

```c
int PREFIX(deflate)(PREFIX3(stream) *strm, int flush);
```

**Flush values**:
| Constant | Value | Behaviour |
|---|---|---|
| `Z_NO_FLUSH` | 0 | Compress as much as possible |
| `Z_PARTIAL_FLUSH` | 1 | Flush pending output |
| `Z_SYNC_FLUSH` | 2 | Flush + align to byte boundary |
| `Z_FULL_FLUSH` | 3 | Flush + reset state (sync point) |
| `Z_FINISH` | 4 | Finish the stream |
| `Z_BLOCK` | 5 | Flush to next block boundary |
| `Z_TREES` | 6 | Flush + emit tree (for debugging) |

**Return codes**:
| Code | Value | Meaning |
|---|---|---|
| `Z_OK` | 0 | Progress made |
| `Z_STREAM_END` | 1 | All input consumed and flushed |
| `Z_STREAM_ERROR` | -2 | Invalid state |
| `Z_BUF_ERROR` | -5 | No progress possible |

### Cleanup

```c
int PREFIX(deflateEnd)(PREFIX3(stream) *strm);
```

### Auxiliary

```c
int PREFIX(deflateSetDictionary)(PREFIX3(stream) *strm,
                                  const uint8_t *dictionary, unsigned dictLength);
int PREFIX(deflateGetDictionary)(PREFIX3(stream) *strm,
                                  uint8_t *dictionary, unsigned *dictLength);
int PREFIX(deflateCopy)(PREFIX3(stream) *dest, PREFIX3(stream) *source);
int PREFIX(deflateReset)(PREFIX3(stream) *strm);
int PREFIX(deflateParams)(PREFIX3(stream) *strm, int level, int strategy);
int PREFIX(deflateTune)(PREFIX3(stream) *strm, int good_length, int max_lazy,
                         int nice_length, int max_chain);
unsigned long PREFIX(deflateBound)(PREFIX3(stream) *strm, unsigned long sourceLen);
int PREFIX(deflatePending)(PREFIX3(stream) *strm, unsigned *pending, int *bits);
int PREFIX(deflatePrime)(PREFIX3(stream) *strm, int bits, int value);
int PREFIX(deflateSetHeader)(PREFIX3(stream) *strm, PREFIX(gz_headerp) head);
```

---

## Decompression

### Initialisation

```c
int PREFIX(inflateInit)(PREFIX3(stream) *strm);
int PREFIX(inflateInit2)(PREFIX3(stream) *strm, int windowBits);
```

**`windowBits`**:
| Value | Format |
|---|---|
| 8..15 | zlib |
| -8..-15 | Raw deflate |
| 24..31 | gzip only |
| 40..47 | Auto-detect zlib or gzip |

### Decompression

```c
int PREFIX(inflate)(PREFIX3(stream) *strm, int flush);
```

**Flush values for inflate**: `Z_NO_FLUSH`, `Z_SYNC_FLUSH`, `Z_FINISH`,
`Z_BLOCK`, `Z_TREES`

**Return codes**:
| Code | Value | Meaning |
|---|---|---|
| `Z_OK` | 0 | Progress made |
| `Z_STREAM_END` | 1 | End of stream reached |
| `Z_NEED_DICT` | 2 | Dictionary required |
| `Z_DATA_ERROR` | -3 | Invalid compressed data |
| `Z_MEM_ERROR` | -4 | Out of memory |
| `Z_BUF_ERROR` | -5 | No progress possible |
| `Z_STREAM_ERROR` | -2 | Invalid parameters |

### Cleanup

```c
int PREFIX(inflateEnd)(PREFIX3(stream) *strm);
```

### Auxiliary

```c
int PREFIX(inflateSetDictionary)(PREFIX3(stream) *strm,
                                  const uint8_t *dictionary, unsigned dictLength);
int PREFIX(inflateGetDictionary)(PREFIX3(stream) *strm,
                                  uint8_t *dictionary, unsigned *dictLength);
int PREFIX(inflateSync)(PREFIX3(stream) *strm);
int PREFIX(inflateCopy)(PREFIX3(stream) *dest, PREFIX3(stream) *source);
int PREFIX(inflateReset)(PREFIX3(stream) *strm);
int PREFIX(inflateReset2)(PREFIX3(stream) *strm, int windowBits);
int PREFIX(inflatePrime)(PREFIX3(stream) *strm, int bits, int value);
long PREFIX(inflateMark)(PREFIX3(stream) *strm);
int PREFIX(inflateGetHeader)(PREFIX3(stream) *strm, PREFIX(gz_headerp) head);
int PREFIX(inflateBack)(PREFIX3(stream) *strm,
                         in_func in, void *in_desc,
                         out_func out, void *out_desc);
int PREFIX(inflateBackEnd)(PREFIX3(stream) *strm);
```

---

## One-Shot Functions

### Compress

```c
int PREFIX(compress)(uint8_t *dest, size_t *destLen,
                      const uint8_t *source, size_t sourceLen);
int PREFIX(compress2)(uint8_t *dest, size_t *destLen,
                       const uint8_t *source, size_t sourceLen, int level);
unsigned long PREFIX(compressBound)(unsigned long sourceLen);
```

`compress()` uses level `Z_DEFAULT_COMPRESSION`. `compress2()` allows
specifying the level.

`compressBound()` returns the maximum compressed size for a given source
length, useful for allocating the output buffer.

### Uncompress

```c
int PREFIX(uncompress)(uint8_t *dest, size_t *destLen,
                        const uint8_t *source, size_t sourceLen);
int PREFIX(uncompress2)(uint8_t *dest, size_t *destLen,
                         const uint8_t *source, size_t *sourceLen);
```

`uncompress2()` also updates `*sourceLen` with the number of source bytes
consumed.

---

## Checksum Functions

### Adler-32

```c
unsigned long PREFIX(adler32)(unsigned long adler,
                               const uint8_t *buf, unsigned len);
unsigned long PREFIX(adler32_z)(unsigned long adler,
                                 const uint8_t *buf, size_t len);
unsigned long PREFIX(adler32_combine)(unsigned long adler1,
                                       unsigned long adler2, z_off_t len2);
```

Initial value: `adler32(0L, Z_NULL, 0)` returns `1`.

### CRC-32

```c
unsigned long PREFIX(crc32)(unsigned long crc,
                             const uint8_t *buf, unsigned len);
unsigned long PREFIX(crc32_z)(unsigned long crc,
                               const uint8_t *buf, size_t len);
unsigned long PREFIX(crc32_combine)(unsigned long crc1,
                                     unsigned long crc2, z_off_t len2);
unsigned long PREFIX(crc32_combine_gen)(z_off_t len2);
unsigned long PREFIX(crc32_combine_op)(unsigned long crc1,
                                        unsigned long crc2, unsigned long op);
```

Initial value: `crc32(0L, Z_NULL, 0)` returns `0`.

---

## Gzip File Operations

```c
gzFile PREFIX(gzopen)(const char *path, const char *mode);
gzFile PREFIX(gzdopen)(int fd, const char *mode);
int    PREFIX(gzbuffer)(gzFile file, unsigned size);
int    PREFIX(gzsetparams)(gzFile file, int level, int strategy);

int    PREFIX(gzread)(gzFile file, void *buf, unsigned len);
size_t PREFIX(gzfread)(void *buf, size_t size, size_t nitems, gzFile file);
int    PREFIX(gzwrite)(gzFile file, const void *buf, unsigned len);
size_t PREFIX(gzfwrite)(const void *buf, size_t size, size_t nitems, gzFile file);
int    PREFIX(gzprintf)(gzFile file, const char *format, ...);
int    PREFIX(gzputs)(gzFile file, const char *s);
char  *PREFIX(gzgets)(gzFile file, char *buf, int len);
int    PREFIX(gzputc)(gzFile file, int c);
int    PREFIX(gzgetc)(gzFile file);
int    PREFIX(gzungetc)(int c, gzFile file);
int    PREFIX(gzflush)(gzFile file, int flush);

z_off64_t PREFIX(gzseek64)(gzFile file, z_off64_t offset, int whence);
z_off64_t PREFIX(gztell64)(gzFile file);
z_off64_t PREFIX(gzoffset64)(gzFile file);
int       PREFIX(gzrewind)(gzFile file);
int       PREFIX(gzeof)(gzFile file);
int       PREFIX(gzdirect)(gzFile file);

int         PREFIX(gzclose)(gzFile file);
int         PREFIX(gzclose_r)(gzFile file);
int         PREFIX(gzclose_w)(gzFile file);
const char *PREFIX(gzerror)(gzFile file, int *errnum);
void        PREFIX(gzclearerr)(gzFile file);
```

---

## Utility Functions

```c
const char *PREFIX(zlibVersion)(void);
const char *PREFIX(neozip_version)(void);  // neozip-specific
unsigned long PREFIX(zlibCompileFlags)(void);
const char *PREFIX(zError)(int err);
```

### `zlibCompileFlags()`

Returns a bitmask indicating compilation options:

| Bit | Meaning |
|---|---|
| 0–1 | size of uInt (0=16, 1=32, 2=64) |
| 2–3 | size of unsigned long |
| 4–5 | size of void * |
| 6–7 | size of z_off_t |
| 8 | Debug build |
| 9 | Assembly code used |
| 10 | DYNAMIC_CRC_TABLE |
| 12 | NO_GZCOMPRESS |
| 16 | PKZIP_BUG_WORKAROUND |
| 17 | FASTEST (deflate_fast only) |

---

## Version Constants

```c
#define NEOZIP_VERSION      "2.3.90"
#define NEOZIP_VER_MAJOR    2
#define NEOZIP_VER_MINOR    3
#define NEOZIP_VER_REVISION 90
#define NEOZIP_VER_STATUS   0    // 0=devel, 1=alpha, 2=beta, ...9=release

#define ZLIB_VERSION        "1.3.1.neozip"   // Compat version
#define ZLIB_VERNUM         0x1310
```

---

## Usage Examples

### Basic Compression

```c
#include <zlib.h>   // or <neozip.h> in native mode

void compress_data(const uint8_t *input, size_t input_len) {
    size_t bound = compressBound(input_len);
    uint8_t *output = malloc(bound);
    size_t output_len = bound;

    int ret = compress2(output, &output_len, input, input_len, Z_DEFAULT_COMPRESSION);
    if (ret != Z_OK) { /* handle error */ }

    // output[0..output_len-1] contains compressed data
    free(output);
}
```

### Streaming Compression

```c
z_stream strm = {0};
deflateInit2(&strm, 6, Z_DEFLATED, 15 + 16, 8, Z_DEFAULT_STRATEGY);
// windowBits = 15 + 16 → gzip format

strm.next_in = input;
strm.avail_in = input_len;
strm.next_out = output;
strm.avail_out = output_size;

while (strm.avail_in > 0) {
    int ret = deflate(&strm, Z_NO_FLUSH);
    if (ret == Z_STREAM_ERROR) break;
    // Flush output if avail_out == 0
}

deflate(&strm, Z_FINISH);
deflateEnd(&strm);
```

### Streaming Decompression

```c
z_stream strm = {0};
inflateInit2(&strm, 15 + 32);  // Auto-detect zlib/gzip

strm.next_in = compressed;
strm.avail_in = compressed_len;
strm.next_out = output;
strm.avail_out = output_size;

int ret;
do {
    ret = inflate(&strm, Z_NO_FLUSH);
    if (ret == Z_DATA_ERROR || ret == Z_MEM_ERROR) break;
} while (ret != Z_STREAM_END);

inflateEnd(&strm);
```

### Gzip File I/O

```c
// Write
gzFile gz = gzopen("data.gz", "wb9");   // Level 9
gzwrite(gz, data, data_len);
gzclose(gz);

// Read
gzFile gz = gzopen("data.gz", "rb");
char buf[4096];
int n;
while ((n = gzread(gz, buf, sizeof(buf))) > 0) {
    // Process buf[0..n-1]
}
gzclose(gz);
```
