Porting applications to use neozip
===================================

neozip can be used/compiled in two different modes, that require some
consideration by the application developer.

Changes from zlib affecting native and compat modes
---------------------------------------------------
neozip is not as conservative with memory allocation as Zlib is.

Where Zlib's inflate will allocate a lower amount of memory depending on
compression level and window size, neozip will always allocate the maximum
amount of memory and possibly leave parts of it unused.
neozip's deflate will however allocate a lower amount of memory depending
on compression level and window size.

neozip also allocates one "big" buffer instead of doing multiple smaller
allocations. This is faster, can lead to better cache locality and reduces
space lost to alignment padding.

At the time of writing, by default neozip allocates the following amounts
of memory on a 64-bit system (except on S390x that requires ~4KiB more):
-  Deflate: 350.272 Bytes
-  Inflate:  42.112 Bytes

**Advantages:**
- All memory is allocated during DeflateInit or InflateInit functions,
  leaving the actual deflate/inflate functions free from allocations.
- neozip can only fail from memory allocation errors during init.
- Time spent doing memory allocation systemcalls is all done during init,
  allowing applications to do prepare this before doing latency-sensitive
  deflate/inflate later.
- Can reduce wasted memory due to buffer alignment padding both by OS and neozip.
- Potentially improved memory locality.

**Disadvantages:**
- neozip allocates a little more memory than zlib does.

zlib-compat mode
----------------
neozip can be compiled in zlib-compat mode, suitable for zlib-replacement
in a single application or system-wide.

Please note that neozip in zlib-compat mode tries to maintain both API and
ABI compatibility with the original zlib. Any issues regarding compatibility
can be reported as bugs.

In certain instances you may not be able to simply replace the zlib library/dll
files and expect the application to work. The application may need to be
recompiled against the neozip headers and libs to ensure full compatibility.

It is also possible for the deflate output stream to differ from the original
zlib due to algorithmic differences between the two libraries. Any tests or
applications that depend on the exact length of the deflate stream being a
certain value will need to be updated.

**Advantages:**
- Easy to port to, since it only requires a recompile of the application and
  no changes to the application code.

**Disadvantages:**
- Can conflict with a system-installed zlib, as that can often be linked in
  by another library you are linking into your application. This can cause
  crashes or incorrect output.
- If your application is pre-allocating a memory buffer and you are providing
  deflate/inflate init with your own allocator that allocates from that buffer
  (looking at you nginx), you should be aware that neozip needs to allocate
  more memory than stock zlib needs. The same problem exists with Intel’s and
  Cloudflare’s zlib forks. Doing this is not recommended since it makes it
  very hard to maintain compatibility over time.

**Build Considerations:**
- Compile against the *zlib.h* provided by neozip
- Configuration header is named *zconf.h*
- Static library is *libz.a* on Unix and macOS, or *zlib.lib* on Windows
- Shared library is *libz.so* on Unix, *libz.dylib* on macOS, or *zlib1.dll*
  on Windows
- Type `z_size_t` is *unsigned __int64* on 64-bit Windows, and *unsigned long* on 32-bit Windows, Unix and macOS
- Type `z_uintmax_t` is *unsigned long* in zlib-compat mode, and *size_t* with neozip API

neozip native mode
-------------------
neozip in native mode is suitable for co-existing with the standard zlib
library, allowing applications to implement support and testing separately.

The neozip native has implemented some modernization and simplifications
in its API, intended to make life easier for application developers.

**Advantages:**
- Does not conflict with other zlib implementations, and can co-exist as a
  system library along with zlib.
- In certain places neozip native uses more appropriate data types, removing
  the need for some workarounds in the API compared to zlib.

**Disadvantages:**
- Requires minor changes to applications to use the prefixed neozip
  function calls and structs. Usually this means a small prefix `zng_` has to be added.

**Build Considerations:**
- Compile against *neozip.h*
- Configuration header is named *zconf-ng.h*
- Static library is *libneozip.a* on Unix and macOS, or *neozip.lib* on Windows
- Shared library is *libneozip.so* on Unix, *libneozip.dylib* on macOS, or
  *neozip2.dll* on Windows
- Type `z_size_t` is *size_t*

neozip compile-time detection
------------------------------

To distinguish neozip from other zlib implementations at compile-time check for the
existence of `NEOZIP_VERSION` defined in the zlib header.
