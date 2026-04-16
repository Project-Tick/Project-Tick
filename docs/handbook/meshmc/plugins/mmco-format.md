# The .mmco Binary Module Format

## Table of Contents

1.  [Introduction](#introduction)
2.  [File Format Overview](#file-format-overview)
3.  [MMCOModuleInfo Structure](#mmcomoduleinfo-structure)
    - [Field Reference](#field-reference)
    - [magic](#magic)
    - [abi_version](#abi_version)
    - [name](#name)
    - [version](#version)
    - [author](#author)
    - [description](#description)
    - [license](#license)
    - [flags](#flags)
    - [Structure Layout Diagram](#structure-layout-diagram)
4.  [Required Exported Symbols](#required-exported-symbols)
    - [mmco_module_info](#mmco_module_info)
    - [mmco_init](#mmco_init)
    - [mmco_unload](#mmco_unload)
    - [Symbol Summary Table](#symbol-summary-table)
5.  [MMCO_DEFINE_MODULE Macro](#mmco_define_module-macro)
    - [Macro Parameters](#macro-parameters)
    - [Expanded Form](#expanded-form)
    - [Convenience Logging Macros](#convenience-logging-macros)
6.  [Symbol Visibility](#symbol-visibility)
    - [MMCO_EXPORT Macro](#mmco_export-macro)
    - [Platform Differences](#platform-differences)
    - [Compiler Flags for Visibility](#compiler-flags-for-visibility)
7.  [ABI Versioning](#abi-versioning)
    - [Version Negotiation](#version-negotiation)
    - [What Happens on Mismatch](#what-happens-on-mismatch)
    - [Forward Compatibility Strategy](#forward-compatibility-strategy)
    - [When Will the ABI Version Change?](#when-will-the-abi-version-change)
8.  [Loading Process](#loading-process)
    - [Step 1: Discovery](#step-1-discovery)
    - [Step 2: Library Opening](#step-2-library-opening)
    - [Step 3: Symbol Resolution](#step-3-symbol-resolution)
    - [Step 4: Validation](#step-4-validation)
    - [Step 5: Metadata Extraction](#step-5-metadata-extraction)
    - [Step 6: Initialization](#step-6-initialization)
    - [Loading Sequence Diagram](#loading-sequence-diagram)
    - [Duplicate Resolution](#duplicate-resolution)
9.  [Search Paths](#search-paths)
    - [Linux Search Paths](#linux-search-paths)
    - [Windows Search Paths](#windows-search-paths)
    - [macOS Search Paths](#macos-search-paths)
    - [Custom Search Paths](#custom-search-paths)
    - [Search Order and Precedence](#search-order-and-precedence)
10. [Building an .mmco Module](#building-an-mmco-module)
    - [Using CMake (Recommended)](#using-cmake-recommended)
    - [Using the Installed SDK](#using-the-installed-sdk)
    - [Manual Compilation (GCC/Clang)](#manual-compilation-gccclang)
    - [Manual Compilation (MSVC)](#manual-compilation-msvc)
    - [Required Compiler Flags](#required-compiler-flags)
11. [File Naming Conventions](#file-naming-conventions)
    - [Naming Rules](#naming-rules)
    - [Module ID Derivation](#module-id-derivation)
    - [Naming Best Practices](#naming-best-practices)
12. [Validation and Error Handling](#validation-and-error-handling)
    - [Loader Validation Checks](#loader-validation-checks)
    - [Error Messages Reference](#error-messages-reference)
    - [Failure Modes](#failure-modes)
    - [Debugging a Failed Load](#debugging-a-failed-load)
13. [Security Considerations](#security-considerations)
    - [Trust Model](#trust-model)
    - [What Plugins Can Access](#what-plugins-can-access)
    - [What Plugins Cannot Do](#what-plugins-cannot-do)
    - [Recommendations for Users](#recommendations-for-users)
    - [Recommendations for Plugin Authors](#recommendations-for-plugin-authors)
14. [Cross-Platform Notes](#cross-platform-notes)
    - [Linux](#linux)
    - [macOS](#macos)
    - [Windows](#windows)
    - [Platform Comparison Table](#platform-comparison-table)
    - [ABI Compatibility Across Platforms](#abi-compatibility-across-platforms)
15. [Examples](#examples)
    - [Minimal .mmco Module (C++)](#minimal-mmco-module-c)
    - [Minimal Module Without SDK Macro](#minimal-module-without-sdk-macro)
    - [Module With Hook Registration](#module-with-hook-registration)
    - [Complete CMake Project](#complete-cmake-project)
    - [Hex Dump Annotations](#hex-dump-annotations)
16. [Appendix: Magic Numbers](#appendix-magic-numbers)
    - [MMCO_MAGIC Byte Layout](#mmco_magic-byte-layout)
    - [Endianness](#endianness)
    - [Identifying .mmco Files With hex Tools](#identifying-mmco-files-with-hex-tools)
17. [Appendix: Quick Reference Card](#appendix-quick-reference-card)

---

## Introduction

An `.mmco` file is the binary module format used by the MeshMC Launcher's MMCO
(MeshMC Module Character Object) plugin system. It is the compiled, loadable
unit that extends MeshMC at runtime.

Conceptually, `.mmco` files are analogous to Linux kernel modules (`.ko`
files). Just as a `.ko` file is a standard ELF shared object that exports a
well-known `init_module` / `cleanup_module` interface for the kernel to call,
an `.mmco` file is a standard shared library that exports a well-known set of
symbols (`mmco_module_info`, `mmco_init`, `mmco_unload`) for the MeshMC
launcher to call. The parallel is intentional: the design draws on decades of
proven kernel module infrastructure, adapted for a user-space application
context.

However, unlike kernel modules, `.mmco` files:

- Run entirely in user space, within the MeshMC launcher process.
- Interact through a C-linkage function-pointer table (`MMCOContext`), not
  system calls.
- Are loaded with standard `dlopen()` / `LoadLibrary()`, not a custom kernel
  loader.
- Have no special filesystem format — they are plain shared libraries with a
  different file extension.

The `.mmco` format is designed to be:

| Goal | How It Is Achieved |
|---|---|
| **Cross-platform** | Standard shared library format (ELF, Mach-O, PE) on each OS |
| **ABI-stable** | C linkage, no C++ name mangling across the boundary |
| **Versioned** | Magic number + ABI version field for forward/backward compat |
| **Simple** | Three required exported symbols — that is the entire contract |
| **Discoverable** | Well-known search paths, `.mmco` extension for easy filtering |

This document is the authoritative reference for the `.mmco` binary module
format. After reading it, you should be able to create, build, install, and
debug `.mmco` modules without referring to the source code.

For the broader plugin system architecture, see
[Plugin System Overview](overview.md). For the full API reference, see the
API documentation for each section of `MMCOContext`.

---

## File Format Overview

An `.mmco` file is, at its core, a **standard shared library** with a non-
standard file extension. There is no custom binary format, no proprietary
container, and no bytecode — it is native machine code for the target
platform.

| Platform | Underlying Format | Native Extension | .mmco Replaces |
|---|---|---|---|
| Linux | ELF (Executable and Linkable Format) | `.so` | `.so` |
| macOS | Mach-O (Mach Object) | `.dylib` | `.dylib` |
| Windows | PE (Portable Executable) | `.dll` | `.dll` |

The only difference between a standard shared library and an `.mmco` file is:

1. **File extension**: `.mmco` instead of `.so` / `.dylib` / `.dll`.
2. **No `lib` prefix**: The file is named `myplugin.mmco`, not
   `libmyplugin.mmco`.
3. **Required exported symbols**: The library must export exactly three symbols
   with C linkage (documented below).

### What Makes It an .mmco?

A shared library qualifies as a valid `.mmco` module if and only if it meets
**all** of the following criteria:

1. It is a valid shared library for the host platform (ELF/Mach-O/PE).
2. It exports a symbol named `mmco_module_info` of type `MMCOModuleInfo`.
3. The `mmco_module_info.magic` field equals `MMCO_MAGIC` (`0x4D4D434F`).
4. The `mmco_module_info.abi_version` field equals the loader's
   `MMCO_ABI_VERSION` (currently `1`).
5. It exports a symbol named `mmco_init` with the signature
   `int mmco_init(MMCOContext*)`.
6. It exports a symbol named `mmco_unload` with the signature
   `void mmco_unload(void)`.

If any of these conditions are not met, the loader rejects the file and logs
a diagnostic message. The module is never partially loaded — it is all or
nothing.

### Relationship to the MeshMC Plugin SDK

The MeshMC Plugin SDK (provided as a single header `mmco_sdk.h` and a CMake
package `MeshMC_SDK`) simplifies the creation of `.mmco` files by providing:

- The `MMCO_DEFINE_MODULE` macro (fills in `mmco_module_info` automatically).
- The `MMCO_EXPORT` macro (handles symbol visibility).
- Convenience logging macros (`MMCO_LOG`, `MMCO_WARN`, `MMCO_ERR`,
  `MMCO_DBG`).
- All necessary type definitions (`MMCOContext`, `MMCOModuleInfo`, hook IDs,
  event structs).

Using the SDK is **strongly recommended** but not strictly required. A module
can be written from scratch in plain C or C++ as long as it exports the
required symbols with the correct types.

---

## MMCOModuleInfo Structure

The `MMCOModuleInfo` structure is the identity card of every `.mmco` module.
It is a plain C struct (no virtual methods, no constructors) that the loader
reads directly from the module's data segment after `dlopen()`/`LoadLibrary()`
resolves the `mmco_module_info` symbol.

### C Definition

```c
struct MMCOModuleInfo {
    uint32_t    magic;          /* Must be MMCO_MAGIC (0x4D4D434F)  */
    uint32_t    abi_version;    /* Must match MMCO_ABI_VERSION (1)  */
    const char* name;           /* Human-readable module name       */
    const char* version;        /* Module version string            */
    const char* author;         /* Author / maintainer              */
    const char* description;    /* Short description                */
    const char* license;        /* SPDX license identifier          */
    uint32_t    flags;          /* Reserved flags, set to 0         */
};
```

### Field Reference

The following table summarises every field:

| # | Field | C Type | Size (bytes) | Required | Default |
|---|---|---|---|---|---|
| 1 | `magic` | `uint32_t` | 4 | Yes | `0x4D4D434F` |
| 2 | `abi_version` | `uint32_t` | 4 | Yes | `1` |
| 3 | `name` | `const char*` | 8 (64-bit) / 4 (32-bit) | Yes | — |
| 4 | `version` | `const char*` | 8 / 4 | Yes | — |
| 5 | `author` | `const char*` | 8 / 4 | Yes | — |
| 6 | `description` | `const char*` | 8 / 4 | Yes | — |
| 7 | `license` | `const char*` | 8 / 4 | Yes | — |
| 8 | `flags` | `uint32_t` | 4 | Yes | `0` |

**Total struct size** (typical 64-bit, no padding):
4 + 4 + (5 × 8) + 4 = **52 bytes** (may vary with compiler padding; use
`sizeof(MMCOModuleInfo)` for the authoritative value).

**Total struct size** (typical 32-bit, no padding):
4 + 4 + (5 × 4) + 4 = **32 bytes**.

> **Note**: The pointer fields (`name`, `version`, etc.) point to string
> literals that reside in the module's read-only data segment. They are valid
> for as long as the shared library is loaded in memory. The loader copies
> these strings into `QString` objects during metadata extraction, so the
> pointers do not need to remain valid after `loadModule()` returns.

---

### magic

```
Field:    magic
Type:     uint32_t
Size:     4 bytes
Value:    MMCO_MAGIC = 0x4D4D434F
```

The `magic` field is a 32-bit unsigned integer that identifies the struct as a
valid MMCO module info block. Its value is the ASCII encoding of the string
`"MMCO"` interpreted as a big-endian 32-bit integer:

```
Byte 0 (MSB): 'M' = 0x4D
Byte 1:       'M' = 0x4D
Byte 2:       'C' = 0x43
Byte 3 (LSB): 'O' = 0x4F

Combined: 0x4D4D434F
```

**Purpose**: The magic number serves as a sanity check. Before reading any
other field, the loader verifies that `info->magic == MMCO_MAGIC`. This
catches several classes of errors:

- A shared library that happens to export a symbol named `mmco_module_info`
  but is not actually an MMCO module.
- A corrupted or truncated module file.
- A file that was compiled against a different, incompatible SDK.

**Validation rule**: If `magic != 0x4D4D434F`, the loader logs:

```
[PluginLoader] <path> bad magic: <hex value> (expected 0x4D4D434F)
```

and immediately unloads the library. No further fields are read.

**How to set it**: Use the `MMCO_MAGIC` preprocessor constant defined in
`MMCOFormat.h` or `mmco_sdk.h`:

```c
#define MMCO_MAGIC 0x4D4D434F
```

If you use the `MMCO_DEFINE_MODULE` macro, this is set automatically.

---

### abi_version

```
Field:    abi_version
Type:     uint32_t
Size:     4 bytes
Value:    MMCO_ABI_VERSION = 1
```

The `abi_version` field declares which version of the MMCO ABI the module was
compiled against. The loader compares this against its own compiled-in
`MMCO_ABI_VERSION` constant. If they do not match exactly, the module is
rejected.

**Current value**: `1` (the initial and, as of this writing, only ABI version).

**Purpose**: ABI versioning ensures that a module and the loader agree on the
layout of `MMCOModuleInfo`, the signatures of `mmco_init` and `mmco_unload`,
and (most critically) the layout of `MMCOContext`. If the context struct gains
or removes function pointers in a future release, the ABI version will be
bumped to prevent old modules from calling into invalid memory.

**Validation rule**: If `abi_version != MMCO_ABI_VERSION`, the loader logs:

```
[PluginLoader] <path> ABI version mismatch: <module version> (expected <loader version>)
```

and immediately unloads the library.

**How to set it**: Use the `MMCO_ABI_VERSION` preprocessor constant:

```c
#define MMCO_ABI_VERSION 1
```

If you use the `MMCO_DEFINE_MODULE` macro, this is set automatically.

---

### name

```
Field:    name
Type:     const char*
Size:     8 bytes (64-bit) / 4 bytes (32-bit)
Value:    Pointer to a null-terminated UTF-8 string
```

The `name` field is a pointer to a null-terminated C string containing the
human-readable name of the module. This is the name displayed in the MeshMC
plugin manager UI.

**Constraints**:

- Must not be `NULL`. If `NULL`, the loader substitutes an empty string.
- Should be short (under 64 characters recommended).
- Should be unique among installed modules (but this is not enforced — the
  file name determines the module ID, not this field).
- Should contain only printable characters. Avoid control characters.
- UTF-8 encoding is assumed. Non-ASCII characters are supported.

**Examples**:

```c
"My Awesome Plugin"
"MeshMC Auto-Backup"
"日本語プラグイン"  /* UTF-8 encoded Japanese */
```

**How to set it**: Pass as the first argument to `MMCO_DEFINE_MODULE`:

```c
MMCO_DEFINE_MODULE("My Plugin", "1.0.0", "Author", "Description", "MIT");
```

---

### version

```
Field:    version
Type:     const char*
Size:     8 bytes (64-bit) / 4 bytes (32-bit)
Value:    Pointer to a null-terminated UTF-8 string
```

The `version` field is a pointer to a null-terminated C string containing the
module's version. There is no enforced format, but **Semantic Versioning**
(`MAJOR.MINOR.PATCH`) is strongly recommended.

**Constraints**:

- Must not be `NULL`. If `NULL`, the loader substitutes an empty string.
- No programmatic parsing is performed by the loader — this is purely
  informational and displayed in the UI.
- Keep it short (under 32 characters).

**Examples**:

```c
"1.0.0"
"2.3.1-beta"
"0.1.0-alpha+build.42"
```

---

### author

```
Field:    author
Type:     const char*
Size:     8 bytes (64-bit) / 4 bytes (32-bit)
Value:    Pointer to a null-terminated UTF-8 string
```

The `author` field is a pointer to a null-terminated C string identifying the
author or maintainer of the module.

**Constraints**:

- Must not be `NULL`. If `NULL`, the loader substitutes an empty string.
- May contain an individual name, organization name, or email address.
- Under 128 characters recommended.

**Examples**:

```c
"Jane Doe"
"Project Tick Contributors"
"alice@example.com"
"Bob <bob@example.com>"
```

---

### description

```
Field:    description
Type:     const char*
Size:     8 bytes (64-bit) / 4 bytes (32-bit)
Value:    Pointer to a null-terminated UTF-8 string
```

The `description` field is a pointer to a null-terminated C string providing a
short summary of what the module does. This is displayed in the plugin manager
UI underneath the module name.

**Constraints**:

- Must not be `NULL`. If `NULL`, the loader substitutes an empty string.
- Should be a single sentence or short paragraph.
- Under 256 characters recommended.

**Examples**:

```c
"Automatically backs up worlds before launching an instance."
"Adds a dark theme to the launcher UI."
"Provides integration with CurseForge for mod downloads."
```

---

### license

```
Field:    license
Type:     const char*
Size:     8 bytes (64-bit) / 4 bytes (32-bit)
Value:    Pointer to a null-terminated UTF-8 string
```

The `license` field is a pointer to a null-terminated C string containing the
SPDX license identifier for the module. This allows users and automated tools
to quickly determine the module's licensing terms.

**Constraints**:

- Must not be `NULL`. If `NULL`, the loader substitutes an empty string.
- Should be a valid SPDX license expression (see https://spdx.org/licenses/).
- The loader does not validate SPDX syntax — this is informational.

**Examples**:

```c
"GPL-3.0-or-later"
"MIT"
"Apache-2.0"
"GPL-3.0-or-later WITH LicenseRef-MeshMC-MMCO-Module-Exception-1.0"
"LGPL-2.1-only"
"Proprietary"
```

**Note on licensing compatibility**: MeshMC itself is licensed under
`GPL-3.0-or-later WITH LicenseRef-MeshMC-MMCO-Module-Exception-1.0`. The MMCO
Module Exception allows plugins to use any license, including proprietary
licenses, without being considered derivative works of MeshMC. However, plugin
authors should still clearly declare their license for transparency.

---

### flags

```
Field:    flags
Type:     uint32_t
Size:     4 bytes
Value:    MMCO_FLAG_NONE = 0x00000000
```

The `flags` field is a 32-bit unsigned integer reserved for future use. In ABI
version 1, the only valid value is `MMCO_FLAG_NONE` (`0`).

**Purpose**: This field exists to allow future ABI-compatible extensions without
bumping the ABI version. For example, a future release might define flags like:

```c
#define MMCO_FLAG_REQUIRES_UI    0x00000001  /* Module needs GUI (cannot run headless) */
#define MMCO_FLAG_THREAD_SAFE    0x00000002  /* Module is safe to call from any thread */
#define MMCO_FLAG_EXPERIMENTAL   0x00000004  /* Module should show a warning in the UI */
```

These are **hypothetical** and not currently defined. For now, always set flags
to `MMCO_FLAG_NONE` (`0`).

**Validation rule**: The loader does not currently validate the flags field.
Unrecognized flags are silently ignored. However, setting non-zero flags in ABI
version 1 is undefined behavior and should be avoided.

**Defined constants**:

| Constant | Value | Meaning |
|---|---|---|
| `MMCO_FLAG_NONE` | `0x00000000` | No flags set (default) |

---

### Structure Layout Diagram

The following ASCII art shows the in-memory layout of `MMCOModuleInfo` on a
64-bit little-endian system (e.g., x86_64 Linux):

```
Offset  Size   Field          Content (example)
------  -----  -------------- ------------------------------------------
0x00    4      magic          4F 43 4D 4D  (0x4D4D434F in little-endian)
0x04    4      abi_version    01 00 00 00  (1 in little-endian)
0x08    8      name           -> "My Plugin\0"  (pointer to .rodata)
0x10    8      version        -> "1.0.0\0"      (pointer to .rodata)
0x18    8      author         -> "Alice\0"      (pointer to .rodata)
0x20    8      description    -> "A test plugin\0"
0x28    8      license        -> "MIT\0"
0x30    4      flags          00 00 00 00  (MMCO_FLAG_NONE)
0x34           (end of struct, possible padding to 0x38 for alignment)
```

On a 32-bit system, the pointer fields are 4 bytes each instead of 8, and
the total size is smaller:

```
Offset  Size   Field          Content (example)
------  -----  -------------- ------------------------------------------
0x00    4      magic          4F 43 4D 4D
0x04    4      abi_version    01 00 00 00
0x08    4      name           -> "My Plugin\0"
0x0C    4      version        -> "1.0.0\0"
0x10    4      author         -> "Alice\0"
0x14    4      description    -> "A test plugin\0"
0x18    4      license        -> "MIT\0"
0x1C    4      flags          00 00 00 00
0x20           (end of struct)
```

> **Important**: The pointers inside `MMCOModuleInfo` point to string literals
> in the shared library's `.rodata` (read-only data) section. These addresses
> are resolved by the dynamic linker when the library is loaded and are valid
> only while the library remains loaded in memory.

---

## Required Exported Symbols

Every valid `.mmco` module must export exactly three symbols with **C linkage**
(i.e., `extern "C"` in C++ code). These symbols constitute the complete
binary interface between the module and the MeshMC loader.

### mmco_module_info

```
Symbol:     mmco_module_info
Type:       MMCOModuleInfo (struct, not a pointer)
Linkage:    extern "C"
Visibility: MMCO_EXPORT (default visibility / dllexport)
```

This is a **global variable** (not a function) of type `MMCOModuleInfo`.
It must be initialized at compile time with valid values. The loader resolves
this symbol immediately after opening the shared library and reads its fields
to validate the module and extract metadata.

**Declaration** (manual, without SDK macro):

```cpp
#include "mmco_sdk.h"  // or MMCOFormat.h for the types

extern "C" MMCO_EXPORT MMCOModuleInfo mmco_module_info = {
    MMCO_MAGIC,                   // magic
    MMCO_ABI_VERSION,             // abi_version
    "My Plugin",                  // name
    "1.0.0",                      // version
    "Alice",                      // author
    "A simple test plugin.",      // description
    "MIT",                        // license
    MMCO_FLAG_NONE                // flags
};
```

**What the loader does with it**:

1. Resolves the symbol via `dlsym(handle, "mmco_module_info")` (POSIX) or
   `GetProcAddress(handle, "mmco_module_info")` (Windows).
2. Casts the returned pointer to `MMCOModuleInfo*`.
3. Checks `magic == MMCO_MAGIC`.
4. Checks `abi_version == MMCO_ABI_VERSION`.
5. Copies string fields (`name`, `version`, `author`, `description`,
   `license`) into `QString` objects in the `PluginMetadata` struct.
6. Stores the raw `MMCOModuleInfo*` in `PluginMetadata::moduleInfo`.

**Error conditions**:

| Condition | Loader Behaviour |
|---|---|
| Symbol not found | Log error, unload library, `loaded = false` |
| `magic` mismatch | Log error with hex values, unload library |
| `abi_version` mismatch | Log error with version numbers, unload library |
| String pointer is `NULL` | Substitute empty `QString`, continue loading |

---

### mmco_init

```
Symbol:     mmco_init
Type:       int (*)(MMCOContext*)
Linkage:    extern "C"
Visibility: MMCO_EXPORT (default visibility / dllexport)
Return:     0 on success, non-zero on failure
```

The `mmco_init` function is the module's **initialization entry point**. It is
called by the `PluginManager` (not the `PluginLoader`) after the module has
been successfully loaded and validated. The loader only resolves the symbol;
it does not call it directly.

**Signature**:

```cpp
extern "C" MMCO_EXPORT int mmco_init(MMCOContext* ctx);
```

**Parameters**:

| Parameter | Type | Description |
|---|---|---|
| `ctx` | `MMCOContext*` | Pointer to the API context struct. Valid for the lifetime of the module. |

The `MMCOContext` struct is a table of function pointers that provides the
module with access to all MeshMC APIs (logging, hooks, settings, instance
management, UI, etc.). The pointer is guaranteed to remain valid from the
moment `mmco_init` is called until after `mmco_unload` returns.

**Return value**:

| Value | Meaning |
|---|---|
| `0` | Initialization succeeded. The module is now active. |
| Non-zero (e.g., `1`, `-1`) | Initialization failed. The module will be unloaded immediately. |

**What to do in mmco_init**:

- Store the `ctx` pointer for later use (e.g., in a global or static variable).
- Register hooks via `ctx->hook_register(...)`.
- Read settings via `ctx->setting_get(...)`.
- Log a startup message via `ctx->log_info(...)`.
- Allocate resources needed by the module.

**What NOT to do in mmco_init**:

- Do not block for extended periods (no network requests, no disk-heavy I/O).
- Do not call `mmco_unload` from within `mmco_init`.
- Do not store the `ctx` pointer past the lifetime of the module.
- Do not fork or exec processes.

**Example**:

```cpp
static MMCOContext* g_ctx = nullptr;

extern "C" MMCO_EXPORT int mmco_init(MMCOContext* ctx) {
    g_ctx = ctx;

    MMCO_LOG(ctx, "Hello from My Plugin!");

    // Register a hook for instance launch events
    ctx->hook_register(ctx->module_handle,
                       MMCO_HOOK_INSTANCE_PRE_LAUNCH,
                       my_pre_launch_hook,
                       nullptr);

    return 0;  // success
}
```

**Error handling when mmco_init fails**:

If `mmco_init` returns a non-zero value, the `PluginManager` will:

1. Log a warning: `"Plugin <name> failed to initialize (code <N>)"`.
2. Call `mmco_unload()` to give the module a chance to clean up.
3. Close the shared library via `dlclose()` / `FreeLibrary()`.
4. Mark the module as `initialized = false`.

---

### mmco_unload

```
Symbol:     mmco_unload
Type:       void (*)(void)
Linkage:    extern "C"
Visibility: MMCO_EXPORT (default visibility / dllexport)
Return:     void (no return value)
```

The `mmco_unload` function is the module's **cleanup entry point**. It is
called when MeshMC shuts down or when the module is being explicitly unloaded
(e.g., user disables it in the plugin manager). After `mmco_unload` returns,
the shared library will be closed and all its code and data unmapped from
memory.

**Signature**:

```cpp
extern "C" MMCO_EXPORT void mmco_unload(void);
```

**Parameters**: None.

**Return value**: None (`void`).

**What to do in mmco_unload**:

- Unregister all hooks that were registered in `mmco_init`.
- Free any dynamically allocated resources.
- Close any open files or network connections.
- Flush any pending writes to settings.
- Set the stored `ctx` pointer to `nullptr`.

**What NOT to do in mmco_unload**:

- Do not call any `MMCOContext` functions after `mmco_unload` returns (the
  context may be invalidated).
- Do not block for extended periods.
- Do not throw C++ exceptions across the `extern "C"` boundary.

**Example**:

```cpp
extern "C" MMCO_EXPORT void mmco_unload(void) {
    if (g_ctx) {
        MMCO_LOG(g_ctx, "Goodbye from My Plugin!");

        // Unregister hooks
        g_ctx->hook_unregister(g_ctx->module_handle,
                               MMCO_HOOK_INSTANCE_PRE_LAUNCH,
                               my_pre_launch_hook);

        g_ctx = nullptr;
    }
}
```

**Shutdown order**: When MeshMC shuts down, plugins are unloaded in **reverse
discovery order**. That is, the last plugin to be loaded is the first to be
unloaded. This ensures that plugins which depend on other plugins (loaded
earlier) can still access their functionality during cleanup.

---

### Symbol Summary Table

| Symbol | Type | Linkage | Required | Called By |
|---|---|---|---|---|
| `mmco_module_info` | `MMCOModuleInfo` (variable) | `extern "C"` | Yes | `PluginLoader::loadModule()` |
| `mmco_init` | `int (*)(MMCOContext*)` | `extern "C"` | Yes | `PluginManager` (after load) |
| `mmco_unload` | `void (*)(void)` | `extern "C"` | Yes | `PluginManager` (at shutdown) |

---

## MMCO_DEFINE_MODULE Macro

The MeshMC Plugin SDK provides the `MMCO_DEFINE_MODULE` macro to simplify the
definition of the `mmco_module_info` symbol. Using it is the recommended way
to declare your module's identity.

### Macro Definition

```cpp
#define MMCO_DEFINE_MODULE(mod_name, mod_version, mod_author, mod_desc, mod_license) \
    extern "C" MMCO_EXPORT MMCOModuleInfo mmco_module_info = {                       \
        MMCO_MAGIC,                                                                  \
        MMCO_ABI_VERSION,                                                            \
        mod_name,                                                                    \
        mod_version,                                                                 \
        mod_author,                                                                  \
        mod_desc,                                                                    \
        mod_license,                                                                 \
        MMCO_FLAG_NONE                                                               \
    }
```

### Macro Parameters

| Parameter | Type | Description | Example |
|---|---|---|---|
| `mod_name` | `const char*` | Human-readable module name | `"Auto Backup"` |
| `mod_version` | `const char*` | Version string (semver recommended) | `"1.2.0"` |
| `mod_author` | `const char*` | Author name or identifier | `"Jane Doe"` |
| `mod_desc` | `const char*` | Short description of the module | `"Backs up worlds"` |
| `mod_license` | `const char*` | SPDX license identifier | `"MIT"` |

### Usage

```cpp
#include "mmco_sdk.h"

// This single line creates the mmco_module_info symbol with all required fields
MMCO_DEFINE_MODULE(
    "Auto Backup",
    "1.2.0",
    "Jane Doe",
    "Automatically backs up instance worlds before launching.",
    "MIT"
);

extern "C" MMCO_EXPORT int mmco_init(MMCOContext* ctx) {
    // ...
    return 0;
}

extern "C" MMCO_EXPORT void mmco_unload(void) {
    // ...
}
```

### Expanded Form

When the preprocessor expands `MMCO_DEFINE_MODULE("Auto Backup", "1.2.0",
"Jane Doe", "Automatically backs up...", "MIT")`, it produces exactly:

```cpp
extern "C" __attribute__((visibility("default"))) MMCOModuleInfo mmco_module_info = {
    0x4D4D434F,          // magic = MMCO_MAGIC
    1,                   // abi_version = MMCO_ABI_VERSION
    "Auto Backup",       // name
    "1.2.0",             // version
    "Jane Doe",          // author
    "Automatically backs up instance worlds before launching.",  // description
    "MIT",               // license
    0x00000000           // flags = MMCO_FLAG_NONE
};
```

On Windows, `__attribute__((visibility("default")))` is replaced with
`__declspec(dllexport)`.

### Why Use the Macro?

| Advantage | Explanation |
|---|---|
| Correctness | Magic and ABI version are always set to the correct compile-time values |
| Brevity | One line instead of 12 |
| Forward compatibility | If `MMCOModuleInfo` gains new fields in a future ABI, the macro will be updated to set their defaults |
| Consistency | All modules look the same, making code review easier |

### Convenience Logging Macros

The SDK also provides shorthand macros for logging through the context:

```cpp
#define MMCO_LOG(ctx, msg)  (ctx)->log_info((ctx)->module_handle, (msg))
#define MMCO_WARN(ctx, msg) (ctx)->log_warn((ctx)->module_handle, (msg))
#define MMCO_ERR(ctx, msg)  (ctx)->log_error((ctx)->module_handle, (msg))
#define MMCO_DBG(ctx, msg)  (ctx)->log_debug((ctx)->module_handle, (msg))
```

And a shorthand for the module handle:

```cpp
#define MMCO_MH (ctx->module_handle)
```

These assume a local variable named `ctx` of type `MMCOContext*` is in scope.

**Example**:

```cpp
extern "C" MMCO_EXPORT int mmco_init(MMCOContext* ctx) {
    MMCO_LOG(ctx, "Plugin initialized successfully.");
    MMCO_DBG(ctx, "Debug: checking settings...");

    const char* val = ctx->setting_get(MMCO_MH, "my_key");
    if (!val) {
        MMCO_WARN(ctx, "Setting 'my_key' not found, using defaults.");
    }

    return 0;
}
```

---

## Symbol Visibility

For the three required symbols to be resolvable by the loader at runtime, they
must be **visible** in the shared library's symbol table. Different platforms
have different mechanisms for controlling symbol visibility.

### MMCO_EXPORT Macro

The SDK defines `MMCO_EXPORT` to handle platform differences automatically:

```cpp
#if defined(_WIN32) || defined(__CYGWIN__)
#  define MMCO_EXPORT __declspec(dllexport)
#else
#  define MMCO_EXPORT __attribute__((visibility("default")))
#endif
```

### Platform Differences

| Platform | Mechanism | Compiler Attribute | Description |
|---|---|---|---|
| **Linux** (GCC/Clang) | ELF visibility | `__attribute__((visibility("default")))` | Marks the symbol as visible in the dynamic symbol table. Symbols without this attribute (or with `visibility("hidden")`) are not exported. |
| **macOS** (Clang) | Mach-O visibility | `__attribute__((visibility("default")))` | Same as Linux — Clang on macOS uses the same GCC attribute syntax. |
| **Windows** (MSVC) | PE export table | `__declspec(dllexport)` | Marks the symbol for inclusion in the DLL's export table. Without this, symbols are not exported from a DLL by default. |
| **Windows** (MinGW/Cygwin) | PE export table | `__declspec(dllexport)` | Same as MSVC. The `_WIN32` or `__CYGWIN__` preprocessor check catches both cases. |

### Compiler Flags for Visibility

When building an `.mmco` module, it is strongly recommended to compile with
**hidden visibility by default** and explicitly export only the three required
symbols using `MMCO_EXPORT`. This reduces the size of the dynamic symbol table,
improves load times, and prevents accidental symbol collisions between modules.

**GCC / Clang** (Linux and macOS):

```bash
-fvisibility=hidden
```

This flag sets the default visibility of all symbols to `hidden`. Only symbols
explicitly marked with `__attribute__((visibility("default")))` (i.e.,
`MMCO_EXPORT`) will be visible.

**MSVC** (Windows):

No special flag is needed. On Windows, DLL symbols are not exported by default
unless marked with `__declspec(dllexport)`.

**CMake**:

```cmake
set(CMAKE_CXX_VISIBILITY_PRESET hidden)
set(CMAKE_VISIBILITY_INLINES_HIDDEN YES)
```

Or per-target:

```cmake
set_target_properties(MyPlugin PROPERTIES
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN YES
)
```

### Why Hidden Visibility Matters

Consider a module that internally uses a function called `parse_config()`. If
this symbol is exported (visible), it could collide with another module's
`parse_config()` — or even with a symbol inside MeshMC itself. By compiling
with `-fvisibility=hidden`, only the three MMCO symbols are exported, and all
internal symbols remain private to the module.

```
Without -fvisibility=hidden:
  Exported: mmco_module_info, mmco_init, mmco_unload,
            parse_config, helper_func, MyClass::method, ...
            (everything is exported)

With -fvisibility=hidden:
  Exported: mmco_module_info, mmco_init, mmco_unload
            (only MMCO_EXPORT symbols)
  Hidden:   parse_config, helper_func, MyClass::method, ...
            (internal symbols, not visible)
```

---

## ABI Versioning

The MMCO plugin system uses a simple integer-based ABI versioning scheme to
ensure binary compatibility between modules and the MeshMC loader.

### Version Negotiation

There is no negotiation protocol. The check is a simple equality test:

```
if (module.abi_version != loader.MMCO_ABI_VERSION) → REJECT
```

Both the module and the loader are compiled with a specific `MMCO_ABI_VERSION`
constant. The module records its value in `mmco_module_info.abi_version`. The
loader reads this field and compares it to its own compiled-in value.

### What Happens on Mismatch

If the versions do not match, the loader:

1. Logs a warning message including both the module's and the loader's ABI
   versions:
   ```
   [PluginLoader] /path/to/module.mmco ABI version mismatch: 2 (expected 1)
   ```
2. Immediately unloads the shared library (`dlclose` / `FreeLibrary`).
3. Returns a `PluginMetadata` with `loaded = false`.
4. Continues scanning for other modules (the failure does not abort discovery).

The module is **never partially loaded**. The `mmco_init` function is never
called if the ABI version does not match.

### Forward Compatibility Strategy

The MMCO ABI is designed to change **rarely**. The `MMCOContext` struct uses
a function-pointer table pattern (similar to COM interfaces or Vulkan dispatch
tables), which allows new functions to be added at the end of the struct
without breaking existing modules — as long as the `struct_size` and
`abi_version` fields in the context are checked.

However, the current loader implementation performs a **strict equality check**
on `abi_version`. This means:

- A module compiled for ABI version 1 will **not** load on a loader that
  expects ABI version 2, even if the struct layout is a strict superset.
- A module compiled for ABI version 2 will **not** load on a loader that
  expects ABI version 1, even if the module only uses version-1 functions.

This strict policy is intentional for the initial release. It may be relaxed
in the future to allow forward/backward compatibility within a major ABI
generation.

### When Will the ABI Version Change?

The ABI version will be bumped when any of the following occur:

| Change | Example | Requires ABI Bump? |
|---|---|---|
| Field added to `MMCOModuleInfo` | New `priority` field | **Yes** |
| Field removed from `MMCOModuleInfo` | Remove `flags` | **Yes** |
| Field type changed in `MMCOModuleInfo` | `flags` → `uint64_t` | **Yes** |
| Function added to end of `MMCOContext` | New `instance_export()` | **Maybe** (if forward-compat is implemented) |
| Function removed from `MMCOContext` | Remove `log_debug()` | **Yes** |
| Function signature changed in `MMCOContext` | Extra parameter | **Yes** |
| New hook ID defined | `MMCO_HOOK_NEW_EVENT` | **No** |
| New flag constant defined | `MMCO_FLAG_NEW` | **No** |
| Bug fix in SDK | Any fix | **No** |

---

## Loading Process

The `.mmco` loading process is a multi-step pipeline handled by two classes:
`PluginLoader` (discovery and library loading) and `PluginManager`
(initialization and lifecycle). This section documents every step in detail.

### Step 1: Discovery

The `PluginLoader::discoverModules()` method iterates through the ordered
list of search paths (see [Search Paths](#search-paths)) and scans each
directory for files matching the glob pattern `*` + `MMCO_EXTENSION` (i.e.,
`*.mmco`).

```
For each directory in searchPaths():
    If directory does not exist → skip
    For each file matching *.mmco:
        Attempt to load the module (Step 2–5)
        If loaded == true:
            Check for duplicate module ID
            If duplicate → unload and skip
            Else → add to results
```

Discovery is non-recursive: only files directly inside the search directory
are considered. Subdirectories are not scanned.

**Logging**: The loader logs each directory it scans:

```
[PluginLoader] Scanning /home/user/.local/lib/mmcmodules
```

And a summary at the end:

```
[PluginLoader] Discovered 3 module(s)
```

### Step 2: Library Opening

For each `.mmco` file found, the loader opens it as a shared library using the
platform's dynamic linker:

**Linux / macOS**:

```c
void* handle = dlopen(path, RTLD_NOW | RTLD_LOCAL);
```

- `RTLD_NOW`: Resolve all symbols immediately. If any symbol cannot be
  resolved (e.g., missing dependency), `dlopen` fails immediately rather than
  deferring the error.
- `RTLD_LOCAL`: Symbols from this library are not made available for symbol
  resolution of subsequently loaded libraries. This prevents symbol conflicts
  between modules.

**Windows**:

```c
HMODULE handle = LoadLibraryW(path);
```

**Failure handling**: If the library cannot be opened:

```
[PluginLoader] Failed to load /path/to/module.mmco - <dlerror() message>
```

Common reasons for failure:

| Reason | Typical Error Message |
|---|---|
| Missing shared library dependency | `libfoo.so: cannot open shared object file` |
| Wrong architecture (e.g., 32-bit module on 64-bit host) | `wrong ELF class: ELFCLASS32` |
| File permissions | `cannot open shared object file: Permission denied` |
| Corrupted file | `invalid ELF header` |
| Missing C++ runtime | `undefined symbol: __cxa_pure_virtual` |

### Step 3: Symbol Resolution

After the library is opened, the loader resolves all three required symbols:

```
1. mmco_module_info   (MMCOModuleInfo*)
2. mmco_init          (int (*)(MMCOContext*))
3. mmco_unload        (void (*)(void))
```

**Linux / macOS**:

```c
auto* info = (MMCOModuleInfo*) dlsym(handle, "mmco_module_info");
auto  init = (InitFunc)        dlsym(handle, "mmco_init");
auto  unld = (UnloadFunc)      dlsym(handle, "mmco_unload");
```

**Windows**:

```c
auto* info = (MMCOModuleInfo*) GetProcAddress(handle, "mmco_module_info");
auto  init = (InitFunc)        GetProcAddress(handle, "mmco_init");
auto  unld = (UnloadFunc)      GetProcAddress(handle, "mmco_unload");
```

**Failure handling**: If any symbol is missing, the loader logs the missing
symbol name and unloads the library:

```
[PluginLoader] /path/to/module.mmco missing mmco_module_info symbol
[PluginLoader] /path/to/module.mmco missing mmco_init symbol
[PluginLoader] /path/to/module.mmco missing mmco_unload symbol
```

The symbol resolution order is: `mmco_module_info` first (so magic/ABI can be
validated before resolving the function symbols), then `mmco_init`, then
`mmco_unload`.

### Step 4: Validation

Once `mmco_module_info` is resolved, the loader performs two validation checks:

**Magic check**:

```cpp
if (info->magic != MMCO_MAGIC) {
    // Log: "bad magic: <hex> (expected 0x4D4D434F)"
    // Unload and return
}
```

**ABI version check**:

```cpp
if (info->abi_version != MMCO_ABI_VERSION) {
    // Log: "ABI version mismatch: <module> (expected <loader>)"
    // Unload and return
}
```

Both checks are fatal — any failure causes the module to be unloaded.

### Step 5: Metadata Extraction

After validation passes, the loader copies the module info fields into a
`PluginMetadata` struct:

```cpp
meta.moduleInfo   = info;                                    // raw pointer
meta.name         = QString::fromUtf8(info->name ? info->name : "");
meta.version      = QString::fromUtf8(info->version ? info->version : "");
meta.author       = QString::fromUtf8(info->author ? info->author : "");
meta.description  = QString::fromUtf8(info->description ? info->description : "");
meta.license      = QString::fromUtf8(info->license ? info->license : "");
meta.flags        = info->flags;
meta.initFunc     = init;     // resolved function pointer
meta.unloadFunc   = unload;   // resolved function pointer
meta.loaded       = true;
```

At this point, the `PluginMetadata` is complete and the module is considered
**loaded** (but not yet **initialized**).

The loader logs a success message:

```
[PluginLoader] Loaded module: My Plugin v1.0.0 by Alice
```

### Step 6: Initialization

Initialization is performed by the `PluginManager`, not the `PluginLoader`.
After all modules have been discovered and loaded, the `PluginManager`
iterates through them and calls `mmco_init` on each one:

```cpp
for (auto& meta : loadedModules) {
    MMCOContext* ctx = createContext(meta);
    int result = meta.initFunc(ctx);
    if (result == 0) {
        meta.initialized = true;
    } else {
        // Log failure, call mmco_unload(), close library
    }
}
```

The `MMCOContext` struct passed to `mmco_init` is populated with function
pointers that delegate to the launcher's internal APIs. Each module receives
its own `MMCOContext` instance with a unique `module_handle` that identifies
the calling module.

### Loading Sequence Diagram

```
                    PluginLoader                    PluginManager
                    ────────────                    ─────────────
                         │                               │
    discoverModules()    │                               │
    ─────────────────>   │                               │
                         │                               │
         ┌───────────────┤                               │
         │ For each search path:                         │
         │   scanDirectory(dir)                          │
         │     │                                         │
         │     ├── Find *.mmco files                     │
         │     │                                         │
         │     ├── For each .mmco:                       │
         │     │     loadModule(path)                    │
         │     │       │                                 │
         │     │       ├── dlopen() / LoadLibrary()      │
         │     │       ├── dlsym("mmco_module_info")     │
         │     │       ├── Validate magic                │
         │     │       ├── Validate abi_version           │
         │     │       ├── dlsym("mmco_init")            │
         │     │       ├── dlsym("mmco_unload")          │
         │     │       ├── Extract metadata              │
         │     │       └── Return PluginMetadata         │
         │     │             (loaded=true)               │
         │     │                                         │
         │     ├── Check for duplicate moduleId          │
         │     └── Add to results                        │
         │                                               │
         └───────────────┤                               │
                         │                               │
     Return modules[]    │                               │
    <────────────────    │                               │
                         │                               │
                         │          initializeAll()       │
                         │    ───────────────────────>    │
                         │                               │
                         │         ┌─────────────────────┤
                         │         │ For each loaded module:
                         │         │   Create MMCOContext  │
                         │         │   Call mmco_init(ctx) │
                         │         │   If result == 0:    │
                         │         │     initialized=true │
                         │         │   Else:              │
                         │         │     Call mmco_unload()│
                         │         │     Close library    │
                         │         └─────────────────────┤
                         │                               │
```

### Duplicate Resolution

If two `.mmco` files in different search paths have the same **module ID**
(derived from the file name — see [File Naming Conventions](#file-naming-conventions)),
only the first one found is loaded. Later duplicates are skipped:

```
[PluginLoader] Skipping duplicate module myplugin from /usr/lib/mmcmodules/myplugin.mmco
```

The search path order determines precedence:

1. Extra paths (from settings) — highest priority
2. `<app_dir>/mmcmodules/` — portable / in-tree
3. `~/.local/lib/mmcmodules/` — user-local
4. `/usr/local/lib/mmcmodules/` — local system
5. `/usr/lib/mmcmodules/` — system / distro packages

This means a user can override a system-installed module by placing a same-
named `.mmco` file in their user-local directory.

---

## Search Paths

MeshMC searches for `.mmco` files in a well-defined set of directories. The
directories are scanned in order, and the first module with a given ID wins
(duplicates are skipped).

### Linux Search Paths

| Priority | Path | Description |
|---|---|---|
| 1 | (extra paths from settings) | User-configured additional paths |
| 2 | `<app_dir>/mmcmodules/` | Next to the MeshMC binary (portable installs) |
| 3 | `~/.local/lib/mmcmodules/` | User-local modules |
| 4 | `/usr/local/lib/mmcmodules/` | Locally-compiled modules |
| 5 | `/usr/lib/mmcmodules/` | Distribution-packaged modules |

Where `<app_dir>` is the directory containing the MeshMC executable, as
returned by `QCoreApplication::applicationDirPath()`.

**Example** (MeshMC installed to `/opt/meshmc/bin/meshmc`):

```
1. /opt/meshmc/bin/mmcmodules/
2. /home/alice/.local/lib/mmcmodules/
3. /usr/local/lib/mmcmodules/
4. /usr/lib/mmcmodules/
```

### Windows Search Paths

| Priority | Path | Description |
|---|---|---|
| 1 | (extra paths from settings) | User-configured additional paths |
| 2 | `<app_dir>\mmcmodules\` | Next to `MeshMC.exe` |
| 3 | `%LOCALAPPDATA%\MeshMC\mmcmodules\` | User-local data directory |

On Windows, there are no system-wide search paths (`/usr/lib/...` equivalents).
The standard Windows application data directory is used instead.

**Example** (MeshMC installed to `C:\Program Files\MeshMC`):

```
1. C:\Program Files\MeshMC\mmcmodules\
2. C:\Users\Alice\AppData\Local\MeshMC\mmcmodules\
```

### macOS Search Paths

macOS uses the same search path logic as Linux:

| Priority | Path | Description |
|---|---|---|
| 1 | (extra paths from settings) | User-configured additional paths |
| 2 | `<app_dir>/mmcmodules/` | Inside or next to the .app bundle |
| 3 | `~/.local/lib/mmcmodules/` | User-local modules |
| 4 | `/usr/local/lib/mmcmodules/` | Homebrew or manually installed modules |
| 5 | `/usr/lib/mmcmodules/` | System (rarely used on macOS) |

### Custom Search Paths

Additional search paths can be added programmatically or through the MeshMC
settings UI:

```cpp
PluginLoader loader;
loader.addSearchPath("/my/custom/modules");
```

Custom paths are **prepended** to the search order, meaning they have the
highest priority (after any previously added custom paths).

### Search Order and Precedence

The complete search algorithm is:

```
searchPaths = extraPaths + defaultSearchPaths

For each path in searchPaths:
    If path does not exist as a directory → skip
    Scan for *.mmco files (non-recursive)
    For each .mmco file:
        Derive module ID from filename
        If module ID already seen → skip (duplicate)
        Attempt to load → if success, add to results
```

**Key points**:

- Scanning is **non-recursive** — only files directly in the directory.
- The first module with a given ID wins — later duplicates are silently
  skipped (with a debug log message).
- Directories that do not exist are silently skipped (no error).
- The `.mmco` extension match is case-sensitive on Linux/macOS and
  case-insensitive on Windows (following the platform filesystem semantics).

---

## Building an .mmco Module

This section covers the practical steps to compile a C++ source file into
a loadable `.mmco` module.

### Using CMake (Recommended)

The simplest way to build an `.mmco` module is to use CMake with the MeshMC
Plugin SDK's CMake package. The SDK installs a `MeshMCSDKConfig.cmake` that
provides the `MeshMC::SDK` target.

**CMakeLists.txt**:

```cmake
cmake_minimum_required(VERSION 3.21)
project(MyPlugin LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find Qt6 (required for MeshMC SDK)
find_package(Qt6 REQUIRED COMPONENTS Core Widgets Gui)

# Find the MeshMC Plugin SDK
find_package(MeshMC_SDK REQUIRED)

# Create the module as a MODULE library (not SHARED)
add_library(MyPlugin MODULE
    src/MyPlugin.cpp
)

# Link against the SDK (provides include paths and compile definitions)
target_link_libraries(MyPlugin PRIVATE
    MeshMC::SDK
    Qt6::Core
    Qt6::Widgets
    Qt6::Gui
)

# CRITICAL: Set the output name and extension
set_target_properties(MyPlugin PROPERTIES
    PREFIX ""           # No "lib" prefix
    SUFFIX ".mmco"      # Use .mmco extension
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN YES
)
```

**Build**:

```bash
mkdir build && cd build
cmake .. -DCMAKE_PREFIX_PATH="/path/to/meshmc/sdk;/path/to/qt6"
cmake --build . --config Release
```

The output will be `MyPlugin.mmco` in the build directory.

### Using the Installed SDK

If MeshMC was installed with `cmake --install`, the SDK is available as a
CMake package. The config file is at:

```
<install_prefix>/lib/cmake/MeshMC_SDK/MeshMCSDKConfig.cmake
```

It defines a target `MeshMC::SDK` that provides:

- Include directories for `mmco_sdk.h` and all MeshMC/Qt headers.
- Compile definitions (`MMCO_MAGIC`, `MMCO_ABI_VERSION`, etc.).

### Manual Compilation (GCC/Clang)

If you prefer not to use CMake, you can compile directly:

```bash
# Single-file module
g++ -std=c++17 -shared -fPIC -fvisibility=hidden \
    -I/path/to/meshmc/sdk/include \
    -I/path/to/qt6/include \
    -I/path/to/qt6/include/QtCore \
    -I/path/to/qt6/include/QtWidgets \
    -I/path/to/qt6/include/QtGui \
    -o myplugin.mmco \
    myplugin.cpp \
    -L/path/to/qt6/lib -lQt6Core -lQt6Widgets -lQt6Gui
```

**Flag breakdown**:

| Flag | Purpose |
|---|---|
| `-std=c++17` | C++17 standard (required by MeshMC SDK) |
| `-shared` | Build a shared library |
| `-fPIC` | Position-independent code (required for shared libs on Linux) |
| `-fvisibility=hidden` | Hide all symbols by default; only `MMCO_EXPORT` symbols are visible |
| `-I...` | Include paths for SDK and Qt headers |
| `-o myplugin.mmco` | Output file with `.mmco` extension |
| `-L... -lQt6...` | Link against Qt libraries |

**Multi-file module**:

```bash
# Compile each source file
g++ -std=c++17 -fPIC -fvisibility=hidden -c -I... -o main.o main.cpp
g++ -std=c++17 -fPIC -fvisibility=hidden -c -I... -o utils.o utils.cpp

# Link into .mmco
g++ -shared -o myplugin.mmco main.o utils.o -L... -lQt6Core -lQt6Widgets -lQt6Gui
```

### Manual Compilation (MSVC)

On Windows with the MSVC toolchain:

```bat
cl /std:c++17 /EHsc /LD /MD ^
    /I"C:\path\to\meshmc\sdk\include" ^
    /I"C:\path\to\qt6\include" ^
    /I"C:\path\to\qt6\include\QtCore" ^
    /I"C:\path\to\qt6\include\QtWidgets" ^
    /I"C:\path\to\qt6\include\QtGui" ^
    myplugin.cpp ^
    /Fe:myplugin.mmco ^
    /link /DLL ^
    "C:\path\to\qt6\lib\Qt6Core.lib" ^
    "C:\path\to\qt6\lib\Qt6Widgets.lib" ^
    "C:\path\to\qt6\lib\Qt6Gui.lib"
```

**Flag breakdown**:

| Flag | Purpose |
|---|---|
| `/std:c++17` | C++17 standard |
| `/EHsc` | Enable C++ exception handling |
| `/LD` | Create a DLL |
| `/MD` | Link with the multithreaded DLL runtime |
| `/Fe:myplugin.mmco` | Output filename |
| `/link /DLL` | Linker: create DLL |

### Required Compiler Flags

Regardless of the build system, these flags are **required** or **strongly
recommended**:

| Flag (GCC/Clang) | Flag (MSVC) | Required? | Purpose |
|---|---|---|---|
| `-std=c++17` | `/std:c++17` | **Required** | C++17 standard |
| `-shared` | `/LD` | **Required** | Build shared library |
| `-fPIC` | (default) | **Required** (Linux) | Position-independent code |
| `-fvisibility=hidden` | (default for DLLs) | Recommended | Hide internal symbols |
| `-O2` | `/O2` | Recommended | Optimization |
| `-Wall -Wextra` | `/W4` | Recommended | Warnings |

---

## File Naming Conventions

The file name of an `.mmco` module is significant because it determines the
**module ID**, which is used for duplicate detection and internal identification.

### Naming Rules

1. **No `lib` prefix**: The file must be named `myplugin.mmco`, **not**
   `libmyplugin.mmco`. CMake's default behavior for shared libraries on Linux
   is to add a `lib` prefix — you must explicitly turn this off:
   ```cmake
   set_target_properties(MyPlugin PROPERTIES PREFIX "")
   ```

2. **`.mmco` suffix**: The file must end with `.mmco` (lowercase). The loader
   only scans for files matching the `*.mmco` glob pattern.

3. **No spaces in filenames**: While technically allowed, spaces in filenames
   can cause issues with some build tools and shell commands. Use hyphens or
   underscores instead.

4. **Lowercase recommended**: For cross-platform compatibility, use lowercase
   filenames. macOS and Linux filesystems are case-sensitive; naming a file
   `MyPlugin.MMCO` will not be found by the `*.mmco` glob on these platforms.

### Module ID Derivation

The module ID is derived from the file name by stripping the path and the
`.mmco` extension:

```cpp
QString moduleId() const {
    QString base = filePath.section('/', -1);  // Get filename
    if (base.endsWith(MMCO_EXTENSION))
        base.chop(strlen(MMCO_EXTENSION));      // Remove ".mmco"
    return base;
}
```

**Examples**:

| File Path | Module ID |
|---|---|
| `/home/user/.local/lib/mmcmodules/autobackup.mmco` | `autobackup` |
| `/usr/lib/mmcmodules/dark-theme.mmco` | `dark-theme` |
| `C:\MeshMC\mmcmodules\my_plugin.mmco` | `my_plugin` |
| `/opt/meshmc/bin/mmcmodules/libfoo.mmco` | `libfoo` (**not recommended**) |

### Naming Best Practices

| Practice | Good | Bad |
|---|---|---|
| Use lowercase | `autobackup.mmco` | `AutoBackup.mmco` |
| Use hyphens or underscores | `auto-backup.mmco` | `auto backup.mmco` |
| Be descriptive | `world-backup.mmco` | `wb.mmco` |
| No `lib` prefix | `mymod.mmco` | `libmymod.mmco` |
| Keep it short | `dark-theme.mmco` | `my-awesome-dark-theme-for-meshmc-v2.mmco` |
| Use ASCII only | `autobackup.mmco` | `ñoño-plugin.mmco` |

---

## Validation and Error Handling

The `PluginLoader` performs a series of validation checks on every `.mmco`
file it encounters. This section documents every check, the order in which
they occur, and the exact error messages produced.

### Loader Validation Checks

The checks are performed in this exact order. The first failing check causes
the module to be rejected — subsequent checks are not performed.

| # | Check | Failure Condition | Severity |
|---|---|---|---|
| 1 | **Library loading** | `dlopen()` / `LoadLibrary()` returns NULL | Fatal |
| 2 | **`mmco_module_info` symbol** | Symbol not found in the library | Fatal |
| 3 | **Magic number** | `info->magic != 0x4D4D434F` | Fatal |
| 4 | **ABI version** | `info->abi_version != MMCO_ABI_VERSION` | Fatal |
| 5 | **`mmco_init` symbol** | Symbol not found in the library | Fatal |
| 6 | **`mmco_unload` symbol** | Symbol not found in the library | Fatal |
| 7 | **Duplicate module ID** | A module with the same ID is already loaded | Skip |

All checks labelled "Fatal" cause the library to be immediately unloaded. The
"Skip" check (duplicate) also unloads the library but is logged at debug level
rather than warning level.

### Error Messages Reference

The following table lists every error / warning message the loader can produce,
along with the check that triggers it:

| Check | Log Level | Message Template |
|---|---|---|
| Library load | Warning | `[PluginLoader] Failed to load <path> - <dlerror>` |
| Library load (Win) | Warning | `[PluginLoader] Failed to load <path> - LoadLibrary error: <code>` |
| Missing `mmco_module_info` | Warning | `[PluginLoader] <path> missing mmco_module_info symbol` |
| Bad magic | Warning | `[PluginLoader] <path> bad magic: <hex> (expected <hex>)` |
| ABI mismatch | Warning | `[PluginLoader] <path> ABI version mismatch: <N> (expected <M>)` |
| Missing `mmco_init` | Warning | `[PluginLoader] <path> missing mmco_init symbol` |
| Missing `mmco_unload` | Warning | `[PluginLoader] <path> missing mmco_unload symbol` |
| Duplicate | Debug | `[PluginLoader] Skipping duplicate module <id> from <path>` |
| Success | Debug | `[PluginLoader] Loaded module: <name> v<ver> by <author>` |
| Scan | Debug | `[PluginLoader] Scanning <dir>` |
| Summary | Debug | `[PluginLoader] Discovered <N> module(s)` |

### Failure Modes

Here are common failure scenarios and how to diagnose them:

**Scenario 1: Missing dependency**

```
[PluginLoader] Failed to load /path/to/myplugin.mmco - libfoo.so.1: cannot open
shared object file: No such file or directory
```

**Cause**: The module links against a shared library (`libfoo.so.1`) that is
not installed or not in the linker search path.

**Fix**: Install the missing library, or set `LD_LIBRARY_PATH` (Linux) or
`DYLD_LIBRARY_PATH` (macOS) to include the library's directory.

**Scenario 2: Wrong architecture**

```
[PluginLoader] Failed to load /path/to/myplugin.mmco - wrong ELF class: ELFCLASS32
```

**Cause**: The module was compiled for a different architecture (e.g., 32-bit
module on a 64-bit MeshMC installation).

**Fix**: Recompile the module for the correct architecture.

**Scenario 3: ABI version mismatch**

```
[PluginLoader] /path/to/myplugin.mmco ABI version mismatch: 2 (expected 1)
```

**Cause**: The module was compiled against a newer (or older) version of the
SDK than the installed MeshMC supports.

**Fix**: Recompile the module against the SDK version that matches the
installed MeshMC.

**Scenario 4: Symbol not exported**

```
[PluginLoader] /path/to/myplugin.mmco missing mmco_init symbol
```

**Cause**: The `mmco_init` function was not marked with `MMCO_EXPORT` or
`extern "C"`, so it is either not in the dynamic symbol table or has a
C++-mangled name that does not match the expected `mmco_init`.

**Fix**: Ensure all three required symbols have `extern "C"` linkage and
`MMCO_EXPORT` visibility.

### Debugging a Failed Load

When a module fails to load, use these diagnostic steps:

**1. Check the MeshMC log output** for the exact error message (see table
above).

**2. Verify the file is a valid shared library**:

```bash
# Linux
file myplugin.mmco
# Expected: ELF 64-bit LSB shared object, x86-64, ...

# macOS
file myplugin.mmco
# Expected: Mach-O 64-bit dynamically linked shared library x86_64
```

**3. List exported symbols**:

```bash
# Linux
nm -D myplugin.mmco | grep mmco
# Expected output:
# 0000000000001234 D mmco_module_info
# 0000000000005678 T mmco_init
# 000000000000abcd T mmco_unload

# macOS
nm -gU myplugin.mmco | grep mmco
# Expected (note leading underscore on macOS):
# 0000000000001234 D _mmco_module_info
# 0000000000005678 T _mmco_init
# 000000000000abcd T _mmco_unload
```

If the symbols are missing or have mangled names (e.g.,
`_Z9mmco_initP11MMCOContext`), the `extern "C"` linkage is missing.

**4. Check for missing dependencies**:

```bash
# Linux
ldd myplugin.mmco
# Look for "not found" entries

# macOS
otool -L myplugin.mmco
# Check all listed libraries are present
```

**5. Test loading manually**:

```bash
# Linux — attempt dlopen from a test program or with LD_DEBUG
LD_DEBUG=libs meshmc 2>&1 | grep myplugin
```

---

## Security Considerations

`.mmco` files are **native machine code** that runs within the MeshMC launcher
process with the **same privileges as the launcher itself**. There is no
sandbox, no bytecode interpreter, and no permission system — a loaded module
has full access to everything the process can access.

### Trust Model

The MMCO plugin system operates on an **explicit trust** model:

- **The user decides** which `.mmco` files to install. MeshMC does not
  automatically download or install modules.
- **There is no code signing** mechanism. MeshMC does not verify digital
  signatures on `.mmco` files.
- **There is no capability-based security**. A loaded module can call any
  function in the `MMCOContext` API, and can also call arbitrary system calls
  (file I/O, network, process management) through normal C/C++ APIs.

This is identical to the trust model of browser extensions, VS Code extensions,
and Linux kernel modules: the user installs them from sources they trust.

### What Plugins Can Access

Once loaded and initialized, a plugin has access to:

| Resource | Via |
|---|---|
| All `MMCOContext` API functions | The `ctx` pointer passed to `mmco_init` |
| The filesystem (read/write/delete) | Standard C/C++ file I/O, plus `ctx->fs_*` APIs |
| The network (HTTP, DNS, sockets) | Standard C/C++ networking, plus `ctx->http_*` APIs |
| Environment variables | `getenv()`, `std::getenv()` |
| The MeshMC process memory | C/C++ pointer arithmetic (within process address space) |
| Other loaded shared libraries | `dlsym(RTLD_DEFAULT, ...)` on POSIX systems |
| Child processes | `fork()`, `exec()` (though the SDK prohibits this — see below) |

### What Plugins Cannot Do

The MMCO SDK documentation states that plugins **MUST NOT**:

- **Fork or exec processes**: Spawning child processes is prohibited by
  convention. The launcher does not enforce this at the system call level, but
  modules that violate this rule may be blacklisted by the MeshMC project.
- **Directly `#include` Qt or MeshMC headers**: Plugins should use only
  the `mmco_sdk.h` header, which re-exports the allowed subset of Qt and
  MeshMC types. Using internal headers may break on ABI changes.

These restrictions are **policy-based, not technically enforced**. A malicious
module can violate them. This is why the trust model is critical.

### Recommendations for Users

1. **Only install `.mmco` files from trusted sources.** Treat them with the
   same caution as installing any native application.
2. **Verify the source code** if possible. Since `.mmco` modules are compiled
   from C/C++ source, review the source before building and installing.
3. **Check the license field** in the MeshMC plugin manager. Open source
   modules (especially those with source available on public repositories)
   are generally safer because they can be audited.
4. **Keep modules updated.** Module authors may release updates to fix
   security vulnerabilities.
5. **Remove unused modules.** Every loaded module increases the attack surface.

### Recommendations for Plugin Authors

1. **Minimize privileges.** Only use the API functions you actually need.
2. **Do not store secrets** (passwords, tokens) in settings — they are stored
   in plain text.
3. **Validate all input** from external sources (network responses, file
   contents, user input).
4. **Do not make network requests to untrusted servers** without user consent.
5. **Document what your module does** clearly so users can make informed trust
   decisions.
6. **Publish your source code** to enable community auditing.

---

## Cross-Platform Notes

The `.mmco` format is designed to be cross-platform, but there are inherent
differences between operating systems that affect how modules are built, loaded,
and behave at runtime.

### Linux

- **Shared library format**: ELF (Executable and Linkable Format).
- **Loading function**: `dlopen(path, RTLD_NOW | RTLD_LOCAL)`.
- **Symbol resolution**: `dlsym(handle, "symbol_name")`.
- **Unloading**: `dlclose(handle)`.
- **Compiler**: GCC or Clang.
- **PIC required**: Yes (`-fPIC`).
- **Visibility**: `__attribute__((visibility("default")))`.
- **Symbol prefix**: None (symbols are exported as-is).
- **Filesystem case sensitivity**: Case-sensitive (`.mmco` !== `.MMCO`).
- **Search paths**: `<app_dir>/mmcmodules/`, `~/.local/lib/mmcmodules/`,
  `/usr/local/lib/mmcmodules/`, `/usr/lib/mmcmodules/`.

### macOS

- **Shared library format**: Mach-O (Mach Object).
- **Loading function**: `dlopen(path, RTLD_NOW | RTLD_LOCAL)`.
- **Symbol resolution**: `dlsym(handle, "symbol_name")`.
- **Unloading**: `dlclose(handle)`.
- **Compiler**: Apple Clang (from Xcode Command Line Tools).
- **PIC required**: Yes (default on macOS).
- **Visibility**: `__attribute__((visibility("default")))`.
- **Symbol prefix**: Leading underscore `_` in the Mach-O symbol table (but
  `dlsym` accepts the name without the underscore).
- **Filesystem case sensitivity**: Case-insensitive by default (APFS),
  case-sensitive on case-sensitive volumes.
- **Architecture**: Universal binaries (arm64 + x86_64) are supported. The
  dynamic linker selects the correct slice.
- **Search paths**: Same as Linux.
- **Code signing**: macOS may require modules to be code-signed or have
  appropriate entitlements, depending on system security settings (Gatekeeper).

### Windows

- **Shared library format**: PE (Portable Executable) — DLL.
- **Loading function**: `LoadLibraryW(path)`.
- **Symbol resolution**: `GetProcAddress(handle, "symbol_name")`.
- **Unloading**: `FreeLibrary(handle)`.
- **Compiler**: MSVC (Visual Studio), MinGW, or Clang-cl.
- **PIC**: Not applicable (Windows DLLs are always relocatable).
- **Visibility**: `__declspec(dllexport)`.
- **Symbol prefix**: None for `__cdecl` linkage (which `extern "C"` uses).
- **Filesystem case sensitivity**: Case-insensitive (NTFS).
- **C runtime**: Modules should use the same C runtime as MeshMC (typically
  the MSVC dynamic runtime `/MD`). Mixing runtimes causes heap corruption.
- **Search paths**: `<app_dir>\mmcmodules\`, `%LOCALAPPDATA%\MeshMC\mmcmodules\`.

### Platform Comparison Table

| Feature | Linux | macOS | Windows |
|---|---|---|---|
| Binary format | ELF | Mach-O | PE (DLL) |
| Load API | `dlopen` | `dlopen` | `LoadLibraryW` |
| Symbol API | `dlsym` | `dlsym` | `GetProcAddress` |
| Close API | `dlclose` | `dlclose` | `FreeLibrary` |
| Visibility attr | `visibility("default")` | `visibility("default")` | `__declspec(dllexport)` |
| PIC required | Yes | Yes (default) | N/A |
| Case-sensitive FS | Yes | Depends | No |
| Symbol underscore | No | Yes (internal) | No |
| Error reporting | `dlerror()` | `dlerror()` | `GetLastError()` |

### ABI Compatibility Across Platforms

A `.mmco` compiled on one platform **cannot** be loaded on another platform.
Each platform uses a different binary format:

```
Linux   (.mmco = ELF)   ≠  macOS   (.mmco = Mach-O)  ≠  Windows (.mmco = PE)
```

Even within the same platform, modules must be compiled for the correct:

- **CPU architecture**: x86_64, aarch64 (ARM64), etc.
- **C++ standard library**: libstdc++ vs libc++ (Linux/macOS).
- **Qt version**: Must match the MeshMC binary's Qt version exactly.
- **Compiler ABI**: GCC and Clang are generally compatible on Linux; MSVC has
  its own ABI on Windows.

The C-linkage interface (`extern "C"`) eliminates most C++ ABI concerns at the
module boundary, but the SDK header includes Qt and MeshMC C++ types that must
be compiled with a compatible toolchain.

---

## Examples

### Minimal .mmco Module (C++)

This is the simplest possible `.mmco` module that passes validation:

```cpp
/* minimal.cpp — A minimal .mmco module */
#include "mmco_sdk.h"

// Declare module identity using the SDK macro
MMCO_DEFINE_MODULE(
    "Minimal Example",    // name
    "0.1.0",              // version
    "Example Author",     // author
    "A do-nothing module that demonstrates the minimum required code.",  // description
    "MIT"                 // license
);

// Global pointer to the API context
static MMCOContext* g_ctx = nullptr;

// Initialization entry point — called by the PluginManager
extern "C" MMCO_EXPORT int mmco_init(MMCOContext* ctx) {
    g_ctx = ctx;
    MMCO_LOG(ctx, "[Minimal] Initialized.");
    return 0;  // 0 = success
}

// Cleanup entry point — called at shutdown or when unloaded
extern "C" MMCO_EXPORT void mmco_unload(void) {
    if (g_ctx) {
        MMCO_LOG(g_ctx, "[Minimal] Unloaded.");
        g_ctx = nullptr;
    }
}
```

**Build** (Linux):

```bash
g++ -std=c++17 -shared -fPIC -fvisibility=hidden \
    -I/path/to/sdk/include \
    $(pkg-config --cflags Qt6Core Qt6Widgets Qt6Gui) \
    -o minimal.mmco minimal.cpp \
    $(pkg-config --libs Qt6Core Qt6Widgets Qt6Gui)
```

**Install**:

```bash
cp minimal.mmco ~/.local/lib/mmcmodules/
```

---

### Minimal Module Without SDK Macro

This example shows how to define the module without using `MMCO_DEFINE_MODULE`,
which is useful for understanding what the macro does or for writing modules in
plain C:

```cpp
/* manual.cpp — Manual module definition (no MMCO_DEFINE_MODULE) */
#include <cstdint>

// Reproduce the minimal required types
struct MMCOModuleInfo {
    uint32_t    magic;
    uint32_t    abi_version;
    const char* name;
    const char* version;
    const char* author;
    const char* description;
    const char* license;
    uint32_t    flags;
};

struct MMCOContext {
    uint32_t struct_size;
    uint32_t abi_version;
    void*    module_handle;
    void (*log_info)(void* mh, const char* msg);
    // ... (remaining fields omitted for brevity — include mmco_sdk.h in practice)
};

// Platform-specific export macro
#if defined(_WIN32) || defined(__CYGWIN__)
#  define MMCO_EXPORT __declspec(dllexport)
#else
#  define MMCO_EXPORT __attribute__((visibility("default")))
#endif

// Manually define the module info symbol
extern "C" MMCO_EXPORT MMCOModuleInfo mmco_module_info = {
    0x4D4D434F,              // magic = "MMCO"
    1,                       // abi_version
    "Manual Module",         // name
    "1.0.0",                 // version
    "Test Author",           // author
    "Manually defined module info without the SDK macro.",  // description
    "MIT",                   // license
    0x00000000               // flags = none
};

static MMCOContext* g_ctx = nullptr;

extern "C" MMCO_EXPORT int mmco_init(MMCOContext* ctx) {
    g_ctx = ctx;
    if (ctx && ctx->log_info) {
        ctx->log_info(ctx->module_handle, "[Manual] Hello!");
    }
    return 0;
}

extern "C" MMCO_EXPORT void mmco_unload(void) {
    if (g_ctx && g_ctx->log_info) {
        g_ctx->log_info(g_ctx->module_handle, "[Manual] Goodbye!");
    }
    g_ctx = nullptr;
}
```

---

### Module With Hook Registration

This example demonstrates a module that registers a hook to be notified
before an instance launches:

```cpp
/* pre_launch_hook.cpp — Demonstrates hook registration */
#include "mmco_sdk.h"

MMCO_DEFINE_MODULE(
    "Pre-Launch Logger",
    "1.0.0",
    "Alice",
    "Logs a message before every instance launch.",
    "MIT"
);

static MMCOContext* g_ctx = nullptr;

// Hook callback — called when an instance is about to launch
static int on_pre_launch(void* module_handle,
                          uint32_t hook_id,
                          void* payload,
                          void* user_data)
{
    (void)module_handle;
    (void)hook_id;
    (void)user_data;

    auto* info = static_cast<MMCOInstanceInfo*>(payload);
    if (g_ctx && info) {
        char buf[256];
        snprintf(buf, sizeof(buf),
                 "[PreLaunch] Instance '%s' (MC %s) is about to launch!",
                 info->instance_name,
                 info->minecraft_version);
        MMCO_LOG(g_ctx, buf);
    }

    return 0;  // 0 = allow the launch to proceed
}

extern "C" MMCO_EXPORT int mmco_init(MMCOContext* ctx) {
    g_ctx = ctx;
    MMCO_LOG(ctx, "[PreLaunch] Registering pre-launch hook...");

    ctx->hook_register(ctx->module_handle,
                       MMCO_HOOK_INSTANCE_PRE_LAUNCH,
                       on_pre_launch,
                       nullptr);

    return 0;
}

extern "C" MMCO_EXPORT void mmco_unload(void) {
    if (g_ctx) {
        g_ctx->hook_unregister(g_ctx->module_handle,
                               MMCO_HOOK_INSTANCE_PRE_LAUNCH,
                               on_pre_launch);
        MMCO_LOG(g_ctx, "[PreLaunch] Unregistered. Goodbye.");
        g_ctx = nullptr;
    }
}
```

---

### Complete CMake Project

Here is a complete, self-contained CMake project for a simple module:

**Directory structure**:

```
my-plugin/
├── CMakeLists.txt
└── src/
    └── MyPlugin.cpp
```

**CMakeLists.txt**:

```cmake
cmake_minimum_required(VERSION 3.21)
project(MyPlugin VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Dependencies
find_package(Qt6 REQUIRED COMPONENTS Core Widgets Gui)
find_package(MeshMC_SDK REQUIRED)

# Module target
add_library(${PROJECT_NAME} MODULE
    src/MyPlugin.cpp
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    MeshMC::SDK
    Qt6::Core
    Qt6::Widgets
    Qt6::Gui
)

set_target_properties(${PROJECT_NAME} PROPERTIES
    PREFIX ""
    SUFFIX ".mmco"
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN YES
    OUTPUT_NAME "myplugin"        # produces "myplugin.mmco"
)

# Install to user module directory
install(TARGETS ${PROJECT_NAME}
    LIBRARY DESTINATION "$ENV{HOME}/.local/lib/mmcmodules"
)
```

**src/MyPlugin.cpp**:

```cpp
#include "mmco_sdk.h"

MMCO_DEFINE_MODULE(
    "My Plugin",
    "1.0.0",
    "Your Name",
    "A template MeshMC plugin.",
    "MIT"
);

static MMCOContext* g_ctx = nullptr;

extern "C" MMCO_EXPORT int mmco_init(MMCOContext* ctx) {
    g_ctx = ctx;
    MMCO_LOG(ctx, "[MyPlugin] Ready.");
    return 0;
}

extern "C" MMCO_EXPORT void mmco_unload(void) {
    if (g_ctx) {
        MMCO_LOG(g_ctx, "[MyPlugin] Shutdown.");
        g_ctx = nullptr;
    }
}
```

**Build and install**:

```bash
cd my-plugin
mkdir build && cd build
cmake .. -DCMAKE_PREFIX_PATH="/path/to/meshmc/sdk;/path/to/qt6"
cmake --build . --config Release
cmake --install .
```

---

### Hex Dump Annotations

The following is an annotated hex dump of the `mmco_module_info` symbol from
a minimal module compiled on x86_64 Linux. The actual addresses vary between
builds; this is a representative example.

```
Symbol: mmco_module_info (type D = data, in .data or .rodata section)
Offset from symbol start:

  Offset  Hex                              ASCII      Field
  ──────  ───────────────────────────────  ─────────  ──────────────────
  0x00    4F 43 4D 4D                      OCMM       magic (LE: 0x4D4D434F)
  0x04    01 00 00 00                      ....       abi_version (LE: 1)
  0x08    XX XX XX XX XX XX XX XX          ........   name ptr  →  "Minimal Example\0"
  0x10    XX XX XX XX XX XX XX XX          ........   version ptr → "0.1.0\0"
  0x18    XX XX XX XX XX XX XX XX          ........   author ptr → "Example Author\0"
  0x20    XX XX XX XX XX XX XX XX          ........   description ptr → "A do-nothing...\0"
  0x28    XX XX XX XX XX XX XX XX          ........   license ptr → "MIT\0"
  0x30    00 00 00 00                      ....       flags (LE: 0)
```

**Notes**:

- The `XX` bytes represent pointer values that depend on the linker's address
  assignment. They point to string literals in the `.rodata` section.
- The magic bytes appear as `4F 43 4D 4D` in memory (little-endian x86_64),
  which is `0x4D4D434F` when read as a 32-bit integer. On a big-endian system,
  the bytes would appear as `4D 4D 43 4F`.
- The `abi_version` is `01 00 00 00` in little-endian = `0x00000001` = `1`.
- The `flags` field is all zeros = `MMCO_FLAG_NONE`.

**Inspecting the actual data with `objdump`**:

```bash
# Find the symbol address
nm -D minimal.mmco | grep mmco_module_info
# Output: 0000000000004000 D mmco_module_info

# Dump the data section around that address
objdump -s -j .data --start-address=0x4000 --stop-address=0x4038 minimal.mmco
```

**Inspecting with `readelf`** (Linux):

```bash
readelf -s minimal.mmco | grep mmco
# Shows symbol table entries for mmco_module_info, mmco_init, mmco_unload

readelf -x .rodata minimal.mmco | head -20
# Shows the string literals that the pointers reference
```

---

## Appendix: Magic Numbers

### MMCO_MAGIC Byte Layout

The `MMCO_MAGIC` constant is defined as:

```c
#define MMCO_MAGIC 0x4D4D434F
```

This is the ASCII encoding of the string `"MMCO"` interpreted as a 32-bit
integer in big-endian byte order:

```
Character:   M       M       C       O
ASCII hex:   0x4D    0x4D    0x43    0x4F
Position:    MSB     ...     ...     LSB

Combined (big-endian): 0x4D4D434F
```

### Endianness

The magic constant is **defined as a 32-bit integer value**, not as a byte
sequence. This means its in-memory byte representation depends on the host
CPU's endianness:

**Little-endian (x86, x86_64, ARM in LE mode)**:

```
Address:   +0    +1    +2    +3
Byte:      0x4F  0x43  0x4D  0x4D
ASCII:     'O'   'C'   'M'   'M'
```

The bytes appear **reversed** compared to the "MMCO" string because the least
significant byte (0x4F = 'O') is stored at the lowest address.

**Big-endian (PowerPC, SPARC, ARM in BE mode)**:

```
Address:   +0    +1    +2    +3
Byte:      0x4D  0x4D  0x43  0x4F
ASCII:     'M'   'M'   'C'   'O'
```

The bytes appear in "natural" reading order.

**Why this matters**:

- When comparing the magic in code (`info->magic == MMCO_MAGIC`), the
  comparison works correctly regardless of endianness because both sides are
  native 32-bit integers.
- When inspecting a hex dump of the `.mmco` file, you must account for the
  host endianness to identify the magic bytes. On the most common platforms
  (x86_64), the bytes will appear as `4F 43 4D 4D`.

### Identifying .mmco Files With hex Tools

You can use standard hex tools to verify that a file is a valid `.mmco` module
by looking for the magic number in the data segment.

**Using `xxd`**:

```bash
# Search for the MMCO magic in the file's data
xxd minimal.mmco | grep -i "4f43 4d4d"
```

Note: On little-endian systems, search for `4f43 4d4d` (the byte-reversed
form). On big-endian systems, search for `4d4d 434f`.

**Using `hexdump`**:

```bash
hexdump -C minimal.mmco | grep -i "4f 43 4d 4d"
```

**Using `nm` to find the symbol**:

```bash
nm -D minimal.mmco | grep mmco_module_info
# 0000000000004000 D mmco_module_info
```

Then dump that address:

```bash
objdump -s --start-address=0x4000 --stop-address=0x4004 -j .data minimal.mmco
```

**Using `strings` to verify metadata**:

```bash
strings minimal.mmco | grep -E "^(Minimal Example|0\.1\.0|Example Author|MIT)$"
```

This should show the string literals from the module info fields.

---

## Appendix: Quick Reference Card

This is a one-page summary of everything a plugin author needs to remember.

### Constants

| Constant | Value | Header |
|---|---|---|
| `MMCO_MAGIC` | `0x4D4D434F` | `MMCOFormat.h` / `mmco_sdk.h` |
| `MMCO_ABI_VERSION` | `1` | `MMCOFormat.h` / `mmco_sdk.h` |
| `MMCO_EXTENSION` | `".mmco"` | `MMCOFormat.h` / `mmco_sdk.h` |
| `MMCO_FLAG_NONE` | `0x00000000` | `MMCOFormat.h` / `mmco_sdk.h` |

### Required Symbols

```cpp
extern "C" MMCO_EXPORT MMCOModuleInfo mmco_module_info = { ... };
extern "C" MMCO_EXPORT int  mmco_init(MMCOContext* ctx);
extern "C" MMCO_EXPORT void mmco_unload(void);
```

### Minimal Boilerplate

```cpp
#include "mmco_sdk.h"

MMCO_DEFINE_MODULE("Name", "1.0.0", "Author", "Description", "License");

static MMCOContext* g_ctx = nullptr;

extern "C" MMCO_EXPORT int mmco_init(MMCOContext* ctx) {
    g_ctx = ctx;
    MMCO_LOG(ctx, "Init");
    return 0;
}

extern "C" MMCO_EXPORT void mmco_unload(void) {
    if (g_ctx) { MMCO_LOG(g_ctx, "Bye"); g_ctx = nullptr; }
}
```

### CMake Properties

```cmake
set_target_properties(MyPlugin PROPERTIES
    PREFIX ""
    SUFFIX ".mmco"
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN YES
)
```

### Search Paths (Linux)

```
<app_dir>/mmcmodules/
~/.local/lib/mmcmodules/
/usr/local/lib/mmcmodules/
/usr/lib/mmcmodules/
```

### Diagnostic Commands

```bash
file myplugin.mmco                         # Check binary format
nm -D myplugin.mmco | grep mmco            # Check exported symbols
ldd myplugin.mmco                          # Check dependencies (Linux)
otool -L myplugin.mmco                     # Check dependencies (macOS)
strings myplugin.mmco | grep -i mmco       # Find embedded strings
```

### Return Codes

| Function | Success | Failure |
|---|---|---|
| `mmco_init` | `0` | Any non-zero value |

### Validation Order

```
dlopen → mmco_module_info → magic → abi_version → mmco_init → mmco_unload → OK
```

---

*This document describes the .mmco binary module format as of MMCO ABI version
1. For the latest version, check the MeshMC Plugin SDK source code or the
online documentation.*
